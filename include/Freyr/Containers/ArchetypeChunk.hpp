#pragma once

#include "Freyr/Base/Entity.hpp"
#include "Freyr/Containers/ComponentArray.hpp"
#include "Freyr/Core/Profiling.hpp"
#include "Freyr/Core/ThreadPool.hpp"
#include "Freyr/Meta/Iteration.hpp"

namespace FREYR_NAMESPACE
{
    class ArchetypeChunk
    {
      public:
        explicit ArchetypeChunk(std::string_view              internalName,
                                const skr::Arc<FreyrOptions>& freyrOptions,
                                const skr::Arc<ThreadPool>&   taskManager,
                                const skr::Arc<TaskCounter>&  taskCounter);

        ~ArchetypeChunk();

        bool TryAddEntity(Entity entity);

        void RemoveEntity(Entity entity);

        template <typename T>
        void AddComponent(const Entity entity, T component)
        {
            (*GetComponentArray<T>())[mRegisteredEntities.getIndex(entity)] = component;
        }

        template <typename... Ts>
        void AddComponents(const Entity entity, const Ts&... components, auto&& callback)
        {
            EnqueueTask([this, entity, components..., callback = std::forward<decltype(callback)>(callback)] {
                meta::forEach(
                    [&]<typename TComponent>(TComponent&& component) {
                        using T = std::remove_reference_t<TComponent>;
                        (*GetComponentArray<T>())[mRegisteredEntities.getIndex(entity)] = component;
                    },
                    std::make_tuple(components...));

                callback(entity, GetComponent<Ts>(entity)...);
            });
        }

        template <typename T>
        void RemoveComponent(const Entity entity)
        {
            GetComponentArray<T>()->Remove(mRegisteredEntities.getIndex(entity),
                                           mRegisteredEntities.lastIndex());
        }

        template <typename T>
        T& GetComponent(const Entity entity)
        {
            return GetComponentArray<T>()->GetComponent(mRegisteredEntities.getIndex(entity));
        }

        template <typename T>
        T& GetComponentAt(const size_t index)
        {
            return GetComponentArray<T>()->GetComponent(index);
        }

        [[nodiscard]] Entity GetEntityAt(const size_t index) const
        {
            return mRegisteredEntities.getDense()[index];
        }

        [[nodiscard]] const Entity* GetEntitiesData() const
        {
            return mRegisteredEntities.getDense().data();
        }

        template <typename... Ts>
        std::tuple<Ts&...> GetComponents(const Entity entity)
        {
            return std::tuple<Ts&...>(
                GetComponentArray<Ts>()->GetComponent(mRegisteredEntities.getIndex(entity))...);
        }

        template <typename... Components>
        void ForEach(const char* label, auto&& function)
        {
            FREYR_TRACE("FREYR", label);

            constexpr bool takesEntity =
                std::is_invocable_v<decltype(function), Entity, Components&...>;

            auto tuple = std::make_tuple(&GetComponentArray<Components>()->GetComponent(0)...);
            const size_t count = mRegisteredEntities.size();

            if constexpr (takesEntity)
            {
                auto entityPtr = mRegisteredEntities.getDense().data();
                for (size_t index = 0; index < count; index++)
                {
                    function(entityPtr[index], std::get<Components*>(tuple)[index]...);
                }
            }
            else
            {
                for (size_t index = 0; index < count; index++)
                {
                    function(std::get<Components*>(tuple)[index]...);
                }
            }
        }

        template <typename... Components>
        void ForEachAsync(const char* label, auto&& function)
        {
            EnqueueTask([this, label, function] { ForEach<Components...>(label, function); });
        }

        template <typename... Components>
        void Map(auto&&                                                             mapFunction,
                 const Entity                                                       index,
                 std::vector<decltype(mapFunction(std::declval<Entity>(),
                                                  std::declval<Components&>()...))>& buffer)
        {
            auto tuple    = std::make_tuple(GetComponentArray<Components>()...);
            auto entities = mRegisteredEntities.getDense().data();

            for (auto i = 0; i < mRegisteredEntities.size(); i++)
            {
                buffer[index + i] =
                    mapFunction(entities[i],
                                std::get<ComponentArray<Components>*>(tuple)->GetComponent(i)...);
            }
        }

        template <typename... Components>
        void ForEach(const char* label, SparseSet<Entity>& entities, auto&& function)
        {
            FREYR_TRACE("FREYR", label);

            auto tuple = std::make_tuple(GetComponentArray<Components>()...);

            std::for_each(entities.begin(), entities.end(), [&](const auto& entity) {
                if (!mRegisteredEntities.contains(entity))
                    return;
                function(entity,
                         std::get<ComponentArray<Components>*>(tuple)->GetComponent(
                             mRegisteredEntities.getIndex(entity))...);
            });
        }

        bool IsFull() const;

        template <typename T>
        void AddComponentArray()
        {
            if (mComponentArrays.contains(GetComponentId<T>()))
                return;

            mComponentArrays.insert(new ComponentArray<T>(mFreyrOptions->ArchetypeChunkCapacity));
        }

        [[nodiscard]] size_t Count() const;

        void GetRegisteredEntities(std::vector<std::uint32_t>& vector) const;

        void Swap(Entity a, Entity b);

        void CopyEntity(Entity from, Entity to, const ArchetypeChunk* chunk) const;

        void MoveData(Entity entity, const ArchetypeChunk* chunk);

        void StartTasks();

        void NextTask();

        void EnqueueTask(Task task);

      protected:
        void InternalRemoveEntity(Entity entity);

        template <typename T>
        ComponentArray<T>* GetComponentArray()
        {
            return static_cast<ComponentArray<T>*>(GetComponentArray(GetComponentId<T>()));
        }

        [[nodiscard]] IComponentArray* GetComponentArray(ComponentId componentId) const;

      private:
        friend class Archetype;
        friend class ArchetypeBuilder;

        alignas(64) std::atomic<int> mLocalTaskCounter;
        alignas(64) skr::Arc<TaskCounter> mTaskCounter;

        rigtorp::MPMCQueue<Task> mQueue;

        skr::Arc<ThreadPool>   mThreadPool;
        skr::Arc<FreyrOptions> mFreyrOptions;

        LocalSparseSet<Entity>           mRegisteredEntities;
        std::string                      mInternalName;
        LocalSparseSet<IComponentArray*> mComponentArrays;
    };
} // namespace FREYR_NAMESPACE
