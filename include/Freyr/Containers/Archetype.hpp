#pragma once

#include "Freyr/Containers/ArchetypeChunk.hpp"
#include "Freyr/Containers/Signature.hpp"
#include "Freyr/Core/Profiling.hpp"
#include "Freyr/Core/TaskManager.hpp"

namespace FREYR_NAMESPACE
{

    class Archetype
    {
      public:
        explicit Archetype(const std::shared_ptr<FreyrOptions>& freyrOptions,
                           const std::shared_ptr<TaskManager>&  taskManager) :
            internalName("Archetype: "), mFreyrOptions(freyrOptions),
            mTaskManager(taskManager)
        {
            mEntityToChunk.resize(freyrOptions->InitialCapacity);
            mRegisteredComponents.resize(512);
            mArchetypeChunks.resize(512);
        }

        // Archetype(const Archetype& other) :
        //     mRegisteredComponents(other.mRegisteredComponents),
        //     mSignature(other.mSignature), mMaxEntities(other.mMaxEntities),
        //     internalName(other.internalName)
        // {
        //     mEntityToChunk.resize(other.mMaxEntities);
        //     mArchetypeChunks.resize(512);
        //
        //     for (auto component : mRegisteredComponents)
        //     {
        //         mArchetypeChunks[mRegisteredComponents.getIndex(component)] =
        //             other
        //                 .mArchetypeChunks[mRegisteredComponents.getIndex(
        //                     component)]
        //                 ->Clone();
        //     }
        // }

        ~Archetype()
        {
            for (const auto& component : mRegisteredComponents)
            {
                delete (mArchetypeChunks[mRegisteredComponents.getIndex(
                    component)]);
            }
        }

        void StartTracing()
        {
            FREYR_PROFILING_BEGIN("FREYR",
                                  internalName.c_str(),
                                  perfetto::Track((uint64_t) this));
        }

        void EndTracing()
        {
            FREYR_PROFILING_END("FREYR", perfetto::Track((uint64_t) this));
        }

        void AddEntity(const Entity entity)
        {
            auto& entityToChunk = mEntityToChunk[entity];

            if (entityToChunk.archetypeChunk)
                return;

            for (const auto& chunk : mArchetypeChunks)
            {
                if (chunk->IsFull())
                    continue;

                chunk->AddEntity(entity);
                entityToChunk.entity         = entity;
                entityToChunk.archetypeChunk = chunk;

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
            assert(!mRegisteredComponents.contains(GetComponentId<T>()) &&
                   "Registering component type more than once.");

            mSignature.AddComponent<T>();

            mRegisteredComponents.insert(GetComponentId<T>());
            for (const auto& chunk : mArchetypeChunks)
            {
                chunk->AddComponentArray<T>();
            }

            if (internalName.size() > 12)
            {
                internalName += ", ";
            }

            internalName += typeid(T).name();
        }

        template <typename T>
        void AddComponent(const Entity& entity, T component)
        {
            if (mEntityToChunk.size() == mMaxEntities - 10)
            {
                Resize(static_cast<size_t>(mMaxEntities * 1.25));
            }

            mRegisteredEntities.insert(entity);

            GetComponentArray<T>()->InsertData(entity, component);
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

            assert(chunk && "Retrieving non-existent component.");

            return chunk->GetComponent<T>(entity);
        }

        void RemoveEntity(const Entity& entity)
        {
            for (auto const& component : mRegisteredComponents)
            {
                mArchetypeChunks[mRegisteredComponents.getIndex(component)]
                    ->RemoveEntity(entity);
            }

            mRegisteredEntities.remove(entity);
        }

        void Resize(const size_t size) { mEntityToChunk.resize(size); }

        const SparseSet<Entity>& GetRegisteredEntities()
        {
            return mRegisteredEntities;
        }

        [[nodiscard]] const Signature& GetSignature() const
        {
            return mSignature;
        }

        template <typename... Components>
        void ForEach(std::string_view label, auto&& function)
        {
            for (auto chunk : mArchetypeChunks)
            {
                chunk->ForEach<Components...>(label, function);
            }
        }

        template <typename... Components>
        void ForEachAsync(std::string_view label, auto&& function)
        {
            for (auto chunk : mArchetypeChunks)
            {
                chunk->ForEachParallel<Components...>(label, function);
            }
        }

        template <typename... Components>
        void ForEachParallel(std::string_view label,
                             auto&&           function,
                             Entity           index)
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
        void ForEach(std::string_view   label,
                     SparseSet<Entity>& entities,
                     auto&&             function)
        {
            for (auto chunk : mArchetypeChunks)
            {
                chunk->ForEach<Components...>(label, entities, function);
            }
        }

        template <typename... Components>
        void ForEachParallel(std::string_view   label,
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

        std::size_t Count() { return mEntityToChunk.size(); }

        void Swap(const Entity& a, const Entity& b)
        {
            // mEntityToChunk.swap(a, b);
            //
            // for (auto component : mRegisteredComponents)
            // {
            //     mArchetypeChunks[mRegisteredComponents.getIndex(component)]
            //         ->Swap(a, b);
            // }
        }

      protected:
        friend class ComponentManager;
        friend class Scene;
        void MoveData(const Entity&                     entity,
                      const std::shared_ptr<Archetype>& other)
        {

            if (const auto chunk = GetChunk(entity); !chunk)
                return;

            other->AddEntity(entity);

            for (const auto component : mRegisteredComponents)
            {
                other->AddComponent(entity, component);
            }

            RemoveEntity(entity);
        };

        void MoveData(const std::shared_ptr<Archetype>& other)
        {
            for (auto chunk : mArchetypeChunks)
            {
                other->mArchetypeChunks.push_back(chunk);
            }

            for (auto entity_chunk : mEntityToChunk)
            {
                other->mEntityToChunk.insert(entity_chunk);
            }
        };

        template <typename T>
        ComponentArray<T>* GetComponentArray()
        {
            assert(mRegisteredComponents.contains(GetComponentId<T>()) &&
                   "Component not registered before use.");

            return static_cast<ComponentArray<T>*>(
                mArchetypeChunks[mRegisteredComponents.getIndex(
                    GetComponentId<T>())]);
        }

        ArchetypeChunk* GetChunk(const Entity entity) const
        {
            return mEntityToChunk.contains(EntityChunk { entity })
                       ? mEntityToChunk[entity].archetypeChunk
                       : nullptr;
        }

      private:
        ArchetypeChunk* CreateChunk()
        {
            const auto chunk = new ArchetypeChunk(mRegisteredComponents,
                                                  mFreyrOptions,
                                                  mTaskManager);

            mArchetypeChunks.push_back(chunk);

            return chunk;
        }

        friend class ArchetypeChunk;

        struct EntityChunk
        {
            Entity          entity {};
            ArchetypeChunk* archetypeChunk;

            operator size_t() const { return entity; }
            operator Entity() const { return entity; }
        };

        std::string internalName;

        Signature mSignature;

        SparseSet<EntityChunk>       mEntityToChunk;
        std::vector<ArchetypeChunk*> mArchetypeChunks;

        SparseSet<ComponentId>        mRegisteredComponents;
        std::shared_ptr<FreyrOptions> mFreyrOptions;
        std::shared_ptr<TaskManager>  mTaskManager;
    };

    inline void Archetype::CopyEntity(const Entity from, const Entity to)
    {
        // TODO: refactor to use chunks
        // GetChunk(from)->CopyEntity(from, to);
        //
        // for (auto const& component : mRegisteredComponents)
        // {
        //     mArchetypeChunks[mRegisteredComponents.getIndex(component)]
        //         ->CopyEntity(from, to);
        // }
    }

} // namespace FREYR_NAMESPACE
