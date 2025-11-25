#pragma once

#include "MPMCQueue.hpp"

#include <cmath>

#include "Freyr/Containers/ArchetypeChunk.hpp"
#include "Freyr/Containers/Signature.hpp"
#include "Freyr/Core/Profiling.hpp"
#include "Freyr/Core/TaskManager.hpp"

namespace
FREYR_NAMESPACE
{
    struct EntityChunk
    {
        Entity          entity{};
        ArchetypeChunk* archetypeChunk{};
    };

    class Archetype
    {

    public:
        explicit Archetype(const Ref<FreyrOptions>& freyrOptions, const Ref<TaskManager>& taskManager) :
            mLatches(freyrOptions->MaxEntities), mInternalName("Archetype: "), mRegisteredComponents(512),
            mFreyrOptions(freyrOptions), mTaskManager(taskManager), running(false)
        {
        }

        ~Archetype()
        {
            for (const auto chunk : mArchetypeChunks)
            {
                delete chunk;
            }
        }

        ArchetypeChunk* AddEntity(const Entity entity)
        {
            if (!mArchetypeChunks.empty())
            {
                std::unique_lock lock(mMutex);

                if (const auto chunk = mArchetypeChunks.front(); chunk->IsFull())
                {
                    mArchetypeChunks.pop_front();
                    mArchetypeChunks.push_back(chunk);
                }
            }

            for (const auto& chunk : mArchetypeChunks)
            {
                if (chunk->IsFull())
                    continue;

                if (chunk->TryAddEntity(entity))
                    return chunk;
            }

            const auto chunk = CreateChunk();
            chunk->TryAddEntity(entity);

            return chunk;
        }

        void CopyEntity(Entity from, Entity to);

        template <typename T>
        void RegisterComponent()
        {
            FREYR_ASSERT(!mRegisteredComponents.contains(GetComponentId<T>()) &&
                "Registering component type more than once.");

            mSignature.AddComponent<T>();

            ComponentArrayFactory componentFactory = [](Archetype* archetype, ArchetypeChunk* chunk) {
                chunk->AddComponentArray<T>();
            };

            InternalRegisterComponent<T>(componentFactory);

            for (const auto& chunk : mArchetypeChunks)
            {
                componentFactory(this, chunk);
            }
        }

        template <typename T>
        void RemoveComponent(const Entity& entity)
        {
            GetComponentArray<T>()->RemoveData(entity);
        }

        template <typename T>
        [[nodiscard]] bool HasComponent() const
        {
            return mRegisteredComponents.contains(GetComponentId<T>());
        }

        template <typename... Ts>
        [[nodiscard]] bool HasComponents() const
        {
            return (mRegisteredComponents.contains(GetComponentId<Ts>()) && ...);
        }

        void StartTasks()
        {
            running = true;

            for (const auto chunk : mArchetypeChunks)
            {
                chunk->StartTasks();
            }
        }

        void WaitTasks()
        {
            while (!mLatches.empty())
            {
                if (Ref<std::latch> latch; mLatches.try_pop(latch))
                {
                    if (!latch->try_wait())
                        latch->wait();
                }
            }

            for (const auto chunk : mArchetypeChunks)
            {
                chunk->StopTasks();
            }

            running = false;
        }

        void GetRegisteredEntities(std::vector<Entity>& buffer) const
        {
            for (const auto& chunk : mArchetypeChunks)
            {
                chunk->GetRegisteredEntities(buffer);
            }
        }

        [[nodiscard]] const Signature& GetSignature() const { return mSignature; }

        [[nodiscard]] Ref<std::latch> CreateLatch() const { return skr::MakeRef<std::latch>(mArchetypeChunks.size()); }

        void ForEachChunk(auto&& function)
        {
            for (auto chunk : mArchetypeChunks)
            {
                function(chunk);
            }
        }

        template <typename... Components>
        void ForEach(std::string label, auto&& function)
        {
            for (auto chunk : mArchetypeChunks)
            {
                chunk->ForEach<Components...>(label, function);
            }
        }

        template <typename... Components>
        void ForEachAsync(std::string label, auto&& function)
        {
            auto latch = CreateLatch();
            mLatches.push(latch);

            for (auto chunk : mArchetypeChunks)
            {
                chunk->ForEachAsync<Components...>(label, function, latch);
            }
        }

        template <typename... Components>
        void ForEachParallel(std::string label, auto&& function, Entity index)
        {
            for (auto chunk : mArchetypeChunks)
            {
                chunk->ForEachParallel<Components...>(label, function, index);
            }
        }

        template <typename... Components>
        void Map(auto&&                                                                       mapFunction,
                 Entity                                                                       index,
                 std::vector<decltype(mapFunction(*(new Entity{}), *(new Components{})...))>& buffer)
        {
            for (auto chunk : mArchetypeChunks)
            {
                chunk->Map<Components...>(mapFunction, index, buffer);
            }
        }

        template <typename... Components>
        void ForEach(std::string label, SparseSet<Entity>& entities, auto&& function)
        {
            for (auto chunk : mArchetypeChunks)
            {
                chunk->ForEach<Components...>(label, entities, function);
            }
        }

        template <typename... Components>
        void ForEachParallel(std::string label, SparseSet<Entity>& entities, auto&& function)
        {
            for (auto chunk : mArchetypeChunks)
            {
                chunk->ForEachParallel<Components...>(label, entities, function);
            }
        }

        void EnsureCapacity(const size_t capacity)
        {
            const size_t chunkCount =
                std::ceil(static_cast<float>(capacity) / static_cast<float>(mFreyrOptions->ArchetypeChunkCapacity));

            if (chunkCount == 0)
                return;

            for (auto i = mArchetypeChunks.size(); i < chunkCount; ++i)
            {
                CreateChunk();
            }
        }

        [[nodiscard]] std::size_t Count() const
        {
            size_t result = 0;

            for (auto&& chunk : mArchetypeChunks)
                result += chunk->Count();

            return result;
        }

    protected:
        friend class ComponentManager;
        friend class Scene;

        template <typename T>
        void InternalRegisterComponent(const ComponentArrayFactory& componentArrayFactory)
        {
            if (mRegisteredComponents.contains(GetComponentId<T>()))
                return;

            if (mInternalName.size() > 12)
            {
                mInternalName += ", ";
            }

            mInternalName += skr::type_name<T>();

            mRegisteredComponents.insert(
                ComponentEntry{ .componentId = GetComponentId<T>(), .factory = componentArrayFactory });
        }

        void RegisterComponentsTo(const Ref<Archetype>& destination)
        {
            for (const auto& componentEntry : mRegisteredComponents)
            {
                destination->mRegisteredComponents.insert(componentEntry);

                destination->mSignature.AddComponent(componentEntry.componentId);

                destination->ForEachChunk([&](ArchetypeChunk* destinationChunk) {
                    componentEntry.factory(this, destinationChunk);
                });
            }
        }

        void MoveData(const Ref<Archetype>& destination)
        {
            for (auto chunk : mArchetypeChunks)
            {
                destination->mArchetypeChunks.push_back(chunk);

                chunk->mRegisteredComponents = &destination->mRegisteredComponents;
            }

            destination->EnsureCapacity(Count() + destination->Count());

            mArchetypeChunks.clear();
        };

        template <typename T>
        ComponentArray<T>* GetComponentArray()
        {
            FREYR_ASSERT(mRegisteredComponents.contains(GetComponentId<T>()) && "Component not registered before use.");

            return static_cast<ComponentArray<T>*>(
                mArchetypeChunks[mRegisteredComponents.getIndex(GetComponentId<T>())]);
        }

    private:
        ArchetypeChunk* CreateChunk()
        {
            const auto chunk = new ArchetypeChunk(&mInternalName, &mRegisteredComponents, mFreyrOptions, mTaskManager);

            if (running)
                chunk->StartTasks();

            for (const auto& [_, factory] : mRegisteredComponents)
            {
                factory(this, chunk);
            }

            {
                std::unique_lock lock(mMutex);
                mArchetypeChunks.push_back(chunk);
            }

            return chunk;
        }

        friend class ArchetypeChunk;

        rigtorp::MPMCQueue<Ref<std::latch>> mLatches;
        bool                                running = true;

        std::string mInternalName;
        Signature   mSignature;

        std::mutex                 mMutex;
        std::list<ArchetypeChunk*> mArchetypeChunks;
        SparseSet<ComponentEntry>  mRegisteredComponents;

        Ref<FreyrOptions> mFreyrOptions;
        Ref<TaskManager>  mTaskManager;
    };

} // namespace FREYR_NAMESPACE