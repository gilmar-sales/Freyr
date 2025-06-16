#pragma once

#include <cmath>

#include "Freyr/Containers/ArchetypeChunk.hpp"
#include "Freyr/Containers/Signature.hpp"
#include "Freyr/Core/Profiling.hpp"
#include "Freyr/Core/TaskManager.hpp"

namespace FREYR_NAMESPACE
{
    struct EntityChunk
    {
        Entity          entity {};
        ArchetypeChunk* archetypeChunk {};
    };

    class Archetype
    {
      public:
        explicit Archetype(const Ref<FreyrOptions>& freyrOptions,
                           const Ref<TaskManager>&  taskManager) :
            mInternalName("Archetype: "), mFreyrOptions(freyrOptions),
            mTaskManager(taskManager),
            mRegisteredEntities(freyrOptions->MaxEntities),
            mRegisteredComponents(512)
        {
            mEntityToChunk.resize(freyrOptions->ArchetypeChunkCapacity);
            mArchetypeChunks.reserve(512);
        }

        ~Archetype()
        {
            for (const auto chunk : mArchetypeChunks)
            {
                delete chunk;
            }
        }

        void AddEntity(const Entity entity)
        {
            mRegisteredEntities.insert(entity);

            auto& entityToChunk = GetEntityToChunk(entity);

            if (entityToChunk.archetypeChunk)
                return;

            for (const auto& chunk : mArchetypeChunks)
            {
                if (chunk->IsFull())
                    continue;

                entityToChunk.entity         = entity;
                entityToChunk.archetypeChunk = chunk;
                chunk->AddEntity(entity);

                return;
            }

            const auto chunk = CreateChunk();
            chunk->AddEntity(entity);
            entityToChunk.entity         = entity;
            entityToChunk.archetypeChunk = chunk;
        }

        void CopyEntity(Entity from, Entity to);

        template <typename T>
        void RegisterComponent()
        {
            FREYR_ASSERT(!mRegisteredComponents.contains(GetComponentId<T>()) &&
                         "Registering component type more than once.");

            mSignature.AddComponent<T>();

            mRegisteredComponents.insert(GetComponentId<T>());

            mComponentArrayFactories.push_back([this](ArchetypeChunk* chunk) {
                chunk->AddComponentArray<T>();
            });

            for (const auto& chunk : mArchetypeChunks)
            {
                chunk->AddComponentArray<T>();
            }

            if (mInternalName.size() > 12)
            {
                mInternalName += ", ";
            }

            mInternalName += skr::type_name<T>();
        }

        template <typename T>
        void AddComponent(const Entity& entity, T component)
        {
            FREYR_ASSERT(mRegisteredComponents.contains(GetComponentId<T>()) &&
                         "Component not registered before use.");

            AddEntity(entity);

            auto& entityChunk = GetEntityToChunk(entity);

            if (!entityChunk.archetypeChunk)
            {
                entityChunk.archetypeChunk = CreateChunk();
                entityChunk.archetypeChunk->AddComponentArray<T>();
            }

            entityChunk.archetypeChunk->AddEntity(entity);
            entityChunk.entity = entity;

            entityChunk.archetypeChunk->AddComponent<T>(entity, component);
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

        template <typename T>
        T& GetComponent(const Entity& entity)
        {
            const auto chunk = GetChunk(entity);

            FREYR_ASSERT(chunk && "Retrieving non-existent component.");

            return chunk->GetComponent<T>(entity);
        }

        template <typename... Ts>
        std::tuple<Ts&...> GetComponents(const Entity& entity)
        {
            const auto chunk = GetChunk(entity);

            FREYR_ASSERT(chunk != nullptr &&
                         "Retrieving non-existent component.");

            return chunk->GetComponents<Ts...>(entity);
        }

        void RemoveEntity(const Entity& entity)
        {
            auto& entityChunk = GetEntityToChunk(entity);

            if (!entityChunk.archetypeChunk)
                return;

            entityChunk.archetypeChunk->RemoveEntity(entity);
        }

        void Resize(const size_t size) { mEntityToChunk.resize(size); }

        void StartTasks()
        {
            for (const auto chunk : mArchetypeChunks)
            {
                chunk->StartTasks();
            }
        }

        void WaitTasks()
        {
            for (const auto& latch : mLatches)
            {
                if (!latch->try_wait())
                    latch->wait();
            }

            mLatches.clear();
        }

        void GetRegisteredEntities(std::vector<Entity>& buffer)
        {
            for (const auto& chunk : mArchetypeChunks)
            {
                chunk->GetRegisteredEntities(buffer);
            }
        }

        [[nodiscard]] const Signature& GetSignature() const
        {
            return mSignature;
        }

        [[nodiscard]] Ref<std::latch> CreateLatch() const
        {
            return skr::MakeRef<std::latch>(mArchetypeChunks.size());
        }

        template <typename... Components>
        void ForEach(std::string label, auto&& function, Ref<std::latch>& latch)
        {
            for (auto chunk : mArchetypeChunks)
            {
                chunk->ForEach<Components...>(label, function, latch);
            }
        }

        template <typename... Components>
        void ForEachAsync(std::string label, auto&& function)
        {
            auto& latch = mLatches.emplace_back(CreateLatch());

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
        void Map(auto&&                                            mapFunction,
                 Entity                                            index,
                 std::vector<decltype(mapFunction(
                     *(new Entity {}), *(new Components {})...))>& buffer)
        {
            for (auto chunk : mArchetypeChunks)
            {
                chunk->Map<Components...>(mapFunction, index, buffer);
            }
        }

        template <typename... Components>
        void ForEach(std::string        label,
                     SparseSet<Entity>& entities,
                     auto&&             function)
        {
            for (auto chunk : mArchetypeChunks)
            {
                chunk->ForEach<Components...>(label, entities, function);
            }
        }

        template <typename... Components>
        void ForEachParallel(std::string        label,
                             SparseSet<Entity>& entities,
                             auto&&             function)
        {
            for (auto chunk : mArchetypeChunks)
            {
                chunk->ForEachParallel<Components...>(label,
                                                      entities,
                                                      function);
            }
        }

        void EnsureCapacity(const size_t capacity)
        {
            const auto chunkCount = std::ceil(
                static_cast<float>(capacity) /
                static_cast<float>(mFreyrOptions->ArchetypeChunkCapacity));

            if (chunkCount == 0)
                return;

            mArchetypeChunks.reserve(chunkCount);

            for (auto i = mArchetypeChunks.size(); i < chunkCount; ++i)
            {
                CreateChunk();
            }

            const auto entityCount =
                chunkCount * mFreyrOptions->ArchetypeChunkCapacity;
            mEntityToChunk.resize(entityCount);
        }

        std::size_t Count()
        {
            size_t result = 0;

            for (auto&& chunk : mArchetypeChunks)
                result += chunk->Count();

            return result;
        }

        void Swap(const Entity& a, const Entity& b)
        {
            auto& entityToChunk = GetEntityToChunk(a);

            mRegisteredEntities.swap(a, b);

            entityToChunk.entity = b;
            entityToChunk.archetypeChunk->Swap(a, b);
        }

      protected:
        friend class ComponentManager;
        friend class Scene;
        void MoveData(const Entity& entity, const Ref<Archetype>& other)
        {
            if (const auto chunk = GetChunk(entity); !chunk)
                return;

            other->AddEntity(entity);

            const auto otherChunk = other->GetChunk(entity);

            for (const auto component : mRegisteredComponents)
            {
                other->mRegisteredComponents.insert(component);
                other->mSignature.AddComponent(component);

                auto& factory =
                    mComponentArrayFactories[mRegisteredComponents.getIndex(
                        component)];

                factory(otherChunk);

                other->mComponentArrayFactories.push_back(factory);
            }

            RemoveEntity(entity);
        };

        void MoveData(const Ref<Archetype>& other)
        {
            for (auto chunk : mArchetypeChunks)
            {
                other->mArchetypeChunks.push_back(chunk);
                chunk->mRegisteredComponents = &other->mRegisteredComponents;
            }

            other->EnsureCapacity(
                mRegisteredEntities.size() + other->mRegisteredEntities.size());

            for (auto entity : mRegisteredEntities.getDense())
            {
                other->mRegisteredEntities.insert(entity);
            }

            for (auto& entity : mRegisteredEntities.getDense())
            {
                auto& entityToChunk        = GetEntityToChunk(entity);
                other->mEntityToChunk[other->mRegisteredEntities.getIndex(
                    entityToChunk.entity)] = entityToChunk;
            }

            mArchetypeChunks.clear();
        };

        template <typename T>
        ComponentArray<T>* GetComponentArray()
        {
            FREYR_ASSERT(mRegisteredComponents.contains(GetComponentId<T>()) &&
                         "Component not registered before use.");

            return static_cast<ComponentArray<T>*>(
                mArchetypeChunks[mRegisteredComponents.getIndex(
                    GetComponentId<T>())]);
        }

        [[nodiscard]] EntityChunk& GetEntityToChunk(const Entity entity)
        {
            return mEntityToChunk[mRegisteredEntities.getIndex(entity)];
        }

        [[nodiscard]] ArchetypeChunk* GetChunk(const Entity entity)
        {
            return Contains(entity) ? GetEntityToChunk(entity).archetypeChunk
                                    : nullptr;
        }

        [[nodiscard]] bool Contains(const Entity entity) const
        {
            return mRegisteredEntities.contains(entity);
        }

      private:
        ArchetypeChunk* CreateChunk()
        {
            const auto chunk =
                new ArchetypeChunk(&mInternalName, &mRegisteredComponents,
                                   mFreyrOptions, mTaskManager);

            for (auto component : mRegisteredComponents)
            {
                mComponentArrayFactories.at(
                    mRegisteredComponents.getIndex(component))(chunk);
            }

            mArchetypeChunks.push_back(chunk);

            return chunk;
        }

        friend class ArchetypeChunk;

        std::vector<Ref<std::latch>> mLatches;

        std::string mInternalName;
        Signature   mSignature;

        SparseSet<Entity>            mRegisteredEntities;
        std::vector<EntityChunk>     mEntityToChunk;
        std::vector<ArchetypeChunk*> mArchetypeChunks;

        SparseSet<ComponentId> mRegisteredComponents;
        std::vector<std::function<void(ArchetypeChunk*)>>
                          mComponentArrayFactories;
        Ref<FreyrOptions> mFreyrOptions;
        Ref<TaskManager>  mTaskManager;
    };

    inline void Archetype::CopyEntity(const Entity from, const Entity to)
    {
        const auto toChunk = GetChunk(to);
        GetChunk(from)->CopyEntity(from, to, toChunk);
        //
        // for (auto const& component : mRegisteredComponents)
        // {
        //     mArchetypeChunks[mRegisteredComponents.getIndex(component)]
        //         ->CopyEntity(from, to);
        // }
    }

} // namespace FREYR_NAMESPACE
