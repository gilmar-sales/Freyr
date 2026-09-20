#include "Freyr/Serialization/Snapshot.hpp"

#include "Freyr/Core/ComponentManager.hpp"
#include "Freyr/Core/EntityManager.hpp"
#include "Freyr/Core/ThreadPool.hpp"
#include "Freyr/Hierarchy/HierarchyComponents.hpp"
#include "Freyr/Hierarchy/HierarchyManager.hpp"

#include <cstring>
#include <sstream>
#include <vector>

namespace FREYR_NAMESPACE
{
    namespace
    {
        constexpr std::uint32_t kSnapshotMagic   = 0x46525952u; // 'FRYR'
        constexpr std::uint32_t kSnapshotVersion = 1;

        template <typename T>
        void WritePod(std::ostream& out, const T& value)
        {
            static_assert(std::is_trivially_copyable_v<T>);
            out.write(reinterpret_cast<const char*>(&value), sizeof(T));
        }

        template <typename T>
        T ReadPod(std::istream& in)
        {
            static_assert(std::is_trivially_copyable_v<T>);
            T value {};
            in.read(reinterpret_cast<char*>(&value), sizeof(T));
            if (!in)
                throw SnapshotError("Unexpected end of snapshot stream");
            return value;
        }

        void WriteString(std::ostream& out, std::string_view value)
        {
            WritePod(out, static_cast<std::uint32_t>(value.size()));
            out.write(value.data(), static_cast<std::streamsize>(value.size()));
        }

        std::string ReadString(std::istream& in)
        {
            const auto size = ReadPod<std::uint32_t>(in);
            std::string value(size, '\0');
            if (size > 0)
            {
                in.read(value.data(), static_cast<std::streamsize>(size));
                if (!in)
                    throw SnapshotError("Unexpected end of snapshot stream while reading string");
            }
            return value;
        }

        struct RemapContext
        {
            const std::vector<Entity>* oldToNew = nullptr;
            EntityManager*             entities = nullptr;
        };

        EntityHandle RemapHandle(EntityHandle handle, void* ctx)
        {
            const auto& remap = *static_cast<RemapContext*>(ctx);
            if (handle.entity == NullEntity || handle.entity >= remap.oldToNew->size())
                return NullHandle;
            const Entity mapped = (*remap.oldToNew)[handle.entity];
            if (mapped == NullEntity || !remap.entities->IsAlive(mapped))
                return NullHandle;
            return remap.entities->HandleOf(mapped);
        }
    } // namespace

    void SnapshotWriter::Save(Registry& registry, std::ostream& out) const
    {
        registry.FlushHierarchyComponents();
        registry.ExecuteTasks();

        auto& components = *registry.mComponentManager;
        auto& entities   = *registry.mEntityManager;
        auto& pool       = *registry.mThreadPool;

        std::vector<Entity> aliveEntities;
        components.ForEachArchetype([&](Archetype* archetype) {
            archetype->GetRegisteredEntities(aliveEntities);
        });

        WritePod(out, kSnapshotMagic);
        WritePod(out, kSnapshotVersion);
        WritePod(out, static_cast<std::uint64_t>(entities.MaxEntities()));
        WritePod(out, static_cast<std::uint32_t>(aliveEntities.size()));

        for (const Entity entity : aliveEntities)
        {
            WritePod(out, entity);
            WritePod(out, entities.GetGeneration(entity));
        }

        std::vector<Archetype*> archetypes;
        components.ForEachArchetype([&](Archetype* archetype) {
            if (archetype->Count() > 0)
                archetypes.push_back(archetype);
        });

        WritePod(out, static_cast<std::uint32_t>(archetypes.size()));

        pool.StartWorkers();

        for (Archetype* archetype : archetypes)
        {
            std::vector<ComponentId> componentIds;
            archetype->ForEachComponent([&](ComponentId componentId, std::string_view) {
                componentIds.push_back(componentId);
            });

            WritePod(out, static_cast<std::uint32_t>(componentIds.size()));
            for (const ComponentId componentId : componentIds)
            {
                const SnapshotCodec* codec = components.FindSnapshotCodec(componentId);
                if (!codec)
                    throw SnapshotError("Snapshot requires trivially_copyable registered component");

                WriteString(out, codec->name);
                WritePod(out, codec->size);
                WritePod(out, codec->align);
            }

            std::vector<ArchetypeChunk*> chunks;
            archetype->ForEachChunk([&](ArchetypeChunk* chunk) {
                if (chunk->Count() > 0)
                    chunks.push_back(chunk);
            });

            WritePod(out, static_cast<std::uint32_t>(chunks.size()));

            auto chunkBuffers = skr::MakeArc<std::vector<std::vector<char>>>(chunks.size());
            auto sharedChunks = skr::MakeArc<std::vector<ArchetypeChunk*>>(chunks);
            auto sharedIds    = skr::MakeArc<std::vector<ComponentId>>(componentIds);
            auto* codecs      = &components;

            for (std::size_t jobIndex = 0; jobIndex < chunks.size(); ++jobIndex)
            {
                pool.AddTask(Task {
                    [chunkBuffers, sharedChunks, sharedIds, codecs, jobIndex]()
                    {
                        ArchetypeChunk* chunk = (*sharedChunks)[jobIndex];
                        const auto      count = chunk->Count();

                        std::ostringstream local;
                        WritePod(local, static_cast<std::uint32_t>(count));

                        const auto entitiesSpan = chunk->GetEntitiesSpan();
                        for (std::size_t i = 0; i < count; ++i)
                            WritePod(local, entitiesSpan[i]);

                        for (const ComponentId componentId : *sharedIds)
                        {
                            const SnapshotCodec* codec = codecs->FindSnapshotCodec(componentId);
                            codec->writeColumn(chunk, count, local);
                        }

                        const auto str = local.str();
                        (*chunkBuffers)[jobIndex].assign(str.begin(), str.end());
                    }});
            }
            pool.WaitForAllTasks();

            for (const auto& buffer : *chunkBuffers)
                out.write(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        }

        pool.StopWorkers();

        if (!out)
            throw SnapshotError("Failed writing snapshot stream");
    }

    void SnapshotReader::Load(Registry& registry, std::istream& in) const
    {
        {
            auto& components = *registry.mComponentManager;
            std::vector<Entity> alive;
            components.ForEachArchetype([&](Archetype* archetype) {
                archetype->GetRegisteredEntities(alive);
            });
            for (const Entity entity : alive)
                registry.DestroyEntity(entity);
            registry.ExecuteTasks();
        }

        auto& components = *registry.mComponentManager;
        auto& entities   = *registry.mEntityManager;
        auto& hierarchy  = *registry.mHierarchyManager;
        auto& pool       = *registry.mThreadPool;

        if (ReadPod<std::uint32_t>(in) != kSnapshotMagic)
            throw SnapshotError("Invalid snapshot magic");
        if (ReadPod<std::uint32_t>(in) != kSnapshotVersion)
            throw SnapshotError("Unsupported snapshot version");

        const auto maxEntities = ReadPod<std::uint64_t>(in);
        if (maxEntities > entities.MaxEntities())
            throw SnapshotError("Snapshot MaxEntities exceeds registry capacity");

        const auto aliveCount = ReadPod<std::uint32_t>(in);
        std::vector<Entity> oldEntities(aliveCount);
        std::vector<Generation> oldGenerations(aliveCount);
        for (std::uint32_t i = 0; i < aliveCount; ++i)
        {
            oldEntities[i]    = ReadPod<Entity>(in);
            oldGenerations[i] = ReadPod<Generation>(in);
        }

        std::vector<Entity> oldToNew(static_cast<std::size_t>(entities.MaxEntities()), NullEntity);
        for (std::uint32_t i = 0; i < aliveCount; ++i)
        {
            const Entity neu = entities.CreateEntity();
            if (oldEntities[i] >= oldToNew.size())
                throw SnapshotError("Snapshot entity index out of range");
            oldToNew[oldEntities[i]] = neu;
            (void) oldGenerations[i];
        }

        const auto archetypeCount = ReadPod<std::uint32_t>(in);

        struct PendingChunk
        {
            std::vector<const SnapshotCodec*> codecs;
            std::vector<Entity>               oldChunkEntities;
            std::vector<std::vector<std::uint8_t>> columns;
        };

        std::vector<PendingChunk> pendingChunks;

        for (std::uint32_t archetypeIndex = 0; archetypeIndex < archetypeCount; ++archetypeIndex)
        {
            const auto componentCount = ReadPod<std::uint32_t>(in);
            std::vector<const SnapshotCodec*> codecs;
            codecs.reserve(componentCount);

            for (std::uint32_t c = 0; c < componentCount; ++c)
            {
                const auto name  = ReadString(in);
                const auto size  = ReadPod<std::uint32_t>(in);
                const auto align = ReadPod<std::uint32_t>(in);

                const SnapshotCodec* codec = components.FindSnapshotCodecByName(name);
                if (!codec)
                    throw SnapshotError("Snapshot component not registered: " + name);
                if (codec->size != size || codec->align != align)
                    throw SnapshotError("Snapshot component layout mismatch: " + name);
                codecs.push_back(codec);
            }

            const auto chunkCount = ReadPod<std::uint32_t>(in);
            for (std::uint32_t chunkIndex = 0; chunkIndex < chunkCount; ++chunkIndex)
            {
                const auto count = ReadPod<std::uint32_t>(in);
                PendingChunk pending;
                pending.codecs = codecs;
                pending.oldChunkEntities.resize(count);
                for (std::uint32_t i = 0; i < count; ++i)
                    pending.oldChunkEntities[i] = ReadPod<Entity>(in);

                pending.columns.resize(codecs.size());
                for (std::size_t c = 0; c < codecs.size(); ++c)
                {
                    pending.columns[c].resize(static_cast<std::size_t>(count) * codecs[c]->size);
                    if (count > 0)
                    {
                        in.read(reinterpret_cast<char*>(pending.columns[c].data()),
                                static_cast<std::streamsize>(pending.columns[c].size()));
                        if (!in)
                            throw SnapshotError("Unexpected end of snapshot while reading columns");
                    }
                }
                pendingChunks.push_back(std::move(pending));
            }
        }

        for (const auto& pending : pendingChunks)
        {
            for (std::size_t i = 0; i < pending.oldChunkEntities.size(); ++i)
            {
                const Entity oldEntity = pending.oldChunkEntities[i];
                if (oldEntity >= oldToNew.size() || oldToNew[oldEntity] == NullEntity)
                    throw SnapshotError("Chunk entity missing from entity table");
                const Entity neu = oldToNew[oldEntity];

                for (std::size_t c = 0; c < pending.codecs.size(); ++c)
                {
                    const auto* codec = pending.codecs[c];
                    const void* bytes = pending.columns[c].data() + i * codec->size;
                    codec->addFromBytes(components, neu, bytes);
                }
            }
        }

        registry.ExecuteTasks();

        auto remapCtx = skr::MakeArc<RemapContext>(RemapContext {
            .oldToNew = &oldToNew,
            .entities = &entities,
        });

        struct RemapJob
        {
            ArchetypeChunk*                   chunk = nullptr;
            std::vector<const SnapshotCodec*> codecs;
        };

        auto remapJobs = skr::MakeArc<std::vector<RemapJob>>();
        components.ForEachArchetype([&](Archetype* archetype) {
            std::vector<const SnapshotCodec*> codecs;
            archetype->ForEachComponent([&](ComponentId componentId, std::string_view) {
                if (const auto* codec = components.FindSnapshotCodec(componentId))
                    codecs.push_back(codec);
            });
            archetype->ForEachChunk([&](ArchetypeChunk* chunk) {
                if (chunk->Count() > 0)
                    remapJobs->push_back(RemapJob {.chunk = chunk, .codecs = codecs});
            });
        });

        pool.StartWorkers();
        for (std::size_t jobIndex = 0; jobIndex < remapJobs->size(); ++jobIndex)
        {
            pool.AddTask(Task {
                [remapJobs, remapCtx, jobIndex]()
                {
                    const RemapJob& job = (*remapJobs)[jobIndex];
                    const auto count = job.chunk->Count();
                    for (const SnapshotCodec* codec : job.codecs)
                    {
                        if (codec->remapColumn)
                            codec->remapColumn(job.chunk, count, RemapHandle, remapCtx.get());
                    }
                }});
        }
        pool.WaitForAllTasks();
        pool.StopWorkers();

        std::vector<std::pair<Entity, Entity>> edges;
        components.ForEachArchetype([&](Archetype* archetype) {
            if (!archetype->HasComponent<ChildOf>())
                return;
            archetype->ForEachChunk([&](ArchetypeChunk* chunk) {
                const auto count = chunk->Count();
                const auto entitiesSpan = chunk->GetEntitiesSpan();
                const auto childOfSpan  = chunk->GetComponentSpan<ChildOf>();
                for (std::size_t i = 0; i < count; ++i)
                {
                    if (!entities.IsAlive(childOfSpan[i].parent))
                        continue;
                    edges.emplace_back(entitiesSpan[i], childOfSpan[i].parent.entity);
                }
            });
        });

        for (const auto& [child, parent] : edges)
            hierarchy.SetParent(child, parent);

        registry.FlushHierarchyComponents();
        registry.ExecuteTasks();
    }
} // namespace FREYR_NAMESPACE
