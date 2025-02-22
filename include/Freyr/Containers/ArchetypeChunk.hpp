#pragma once

#include "Freyr/Containers/ComponentArray.hpp"
#include "Freyr/Core/Profiling.hpp"
#include "Freyr/Core/TaskManager.hpp"

namespace FREYR_NAMESPACE
{
    class Archetype;

    class ArchetypeChunk
    {
      public:
        explicit ArchetypeChunk(
            SparseSet<ComponentId>&              registeredComponents,
            const std::shared_ptr<FreyrOptions>& freyrOptions,
            const std::shared_ptr<TaskManager>&  taskManager) :
            mFreyrOptions(freyrOptions), mMutexes(registeredComponents.size()),
            mTaskManager(taskManager),
            mRegisteredEntities(freyrOptions->ArchetypeChunkCapacity),
            mRegisteredComponents(registeredComponents)
        {
        }

        ~ArchetypeChunk()
        {
            for (const auto& component : mRegisteredComponents)
            {
                delete (mComponentArrays[mRegisteredComponents.getIndex(
                    component)]);
            }
        }

        void AddEntity(const Entity entity)
        {

            for (auto const& component : mRegisteredComponents)
            {
                mComponentArrays[mRegisteredComponents.getIndex(component)]
                    ->AddEntity(entity);
            }
        }

        template <typename T>
        void AddComponent(const Entity& entity, T component)
        {
            GetComponentArray<T>()->InsertData(entity, component);
        }

        template <typename T>
        void RemoveComponent(const Entity& entity)
        {
            GetComponentArray<T>()->RemoveData(entity);
        }

        template <typename T>
        T& GetComponent(const Entity& entity)
        {
            return GetComponentArray<T>()->GetData(entity);
        }

        template <typename... Components>
        void ForEach(std::string_view label, auto&& function)
        {
            std::scoped_lock lock(mMutexes[GetComponentId<Components>()]...);

            const auto id =
                std::hash<std::thread::id> {}(std::this_thread::get_id());

            FREYR_PROFILING_BEGIN("FREYR",
                                  label.data(),
                                  perfetto::Track(id),
                                  "entity_count",
                                  mRegisteredEntities.size());
            for (const auto& entity : mRegisteredEntities)
            {
                function(entity,
                         GetComponentArray<Components>()->GetData(entity)...);
            }
            FREYR_PROFILING_END("FREYR", perfetto::Track(id));
        }

        template <typename... Components>
        void ForEachAsync(std::string_view label, auto&& function)
        {
            std::scoped_lock lock(mMutexes[GetComponentId<Components>()]...);

            mTaskManager->AddTask(
                [this,
                 label,
                 function = std::forward<decltype(function)>(function)] {
                    const auto id = std::hash<std::thread::id> {}(
                        std::this_thread::get_id());

                    TaskManager::StartProfiling();
                    FREYR_PROFILING_BEGIN(
                        "FREYR",
                        label.data(),
                        perfetto::Track(id),
                        "entity_count",
                        mRegisteredEntities.size(),
                        "ThreadId",
                        id);

                    std::for_each(
                        std::execution::par,
                        mRegisteredEntities.begin(),
                        mRegisteredEntities.end(),
                        [&](const auto& entity) {
                            function(entity,
                                     GetComponentArray<Components>()->GetData(
                                         entity)...);
                        });

                    FREYR_PROFILING_END("FREYR", perfetto::Track(id));
                    TaskManager::EndProfiling();
                });
        }

        template <typename... Components>
        void ForEachParallel(std::string_view label,
                             auto&&           function,
                             Entity           index)
        {
            std::scoped_lock lock(mMutexes[GetComponentId<Components>()]...);

            FREYR_PROFILING_BEGIN("FREYR",
                                  label.data(),
                                  perfetto::Track((uint64_t) this),
                                  "entity_count",
                                  mRegisteredEntities.size());
            std::for_each(
                std::execution::par,
                mRegisteredEntities.begin(),
                mRegisteredEntities.end(),
                [&](const auto& entity) {
                    function(
                        entity,
                        index + mRegisteredEntities.getIndex(entity),
                        GetComponentArray<Components>()->GetData(entity)...);
                });
            FREYR_PROFILING_END("FREYR", perfetto::Track((uint64_t) this));
        }

        template <typename... Components>
        void Map(auto&&                                            mapFunction,
                 Entity                                            index,
                 std::vector<decltype(mapFunction(
                     *(new Entity {}), *(new Components {})...))>& buffer)
        {
            std::scoped_lock lock(mMutexes[GetComponentId<Components>()]...);

            std::for_each(
                std::execution::par,
                mRegisteredEntities.begin(),
                mRegisteredEntities.end(),
                [&](const auto& entity) {
                    buffer[index + mRegisteredEntities.getIndex(entity)] =
                        mapFunction(entity,
                                    GetComponentArray<Components>()->GetData(
                                        entity)...);
                });
        }

        template <typename... Components>
        void ForEach(std::string_view   label,
                     SparseSet<Entity>& entities,
                     auto&&             function)
        {
            std::scoped_lock lock(mMutexes[GetComponentId<Components>()]...);

            FREYR_PROFILING_BEGIN("FREYR",
                                  label.data(),
                                  perfetto::Track((uint64_t) this),
                                  "entity_count",
                                  entities.size());
            std::for_each(std::execution::seq,
                          entities.begin(),
                          entities.end(),
                          [&](const auto& entity) {
                              if (!mRegisteredEntities.contains(entity))
                                  return;
                              function(entity,
                                       GetComponentArray<Components>()->GetData(
                                           entity)...);
                          });
            FREYR_PROFILING_END("FREYR", perfetto::Track((uint64_t) this));
        }

        template <typename... Components>
        void ForEachParallel(std::string_view   label,
                             SparseSet<Entity>& entities,
                             auto&&             function)
        {
            std::scoped_lock lock(mMutexes[GetComponentId<Components>()]...);

            FREYR_PROFILING_BEGIN("FREYR",
                                  label.data(),
                                  perfetto::Track((uint64_t) this),
                                  "entity_count",
                                  entities.size());
            std::for_each(std::execution::par,
                          entities.begin(),
                          entities.end(),
                          [&](const auto& entity) {
                              if (!mRegisteredEntities.contains(entity))
                                  return;

                              function(entity,
                                       GetComponentArray<Components>()->GetData(
                                           entity)...);
                          });
            FREYR_PROFILING_END("FREYR", perfetto::Track((uint64_t) this));
        }

        bool IsFull() { return mRegisteredEntities.isFull(); }

        template <typename T>
        void AddComponentArray()
        {
            const auto componentId =
                mRegisteredComponents.getIndex(GetComponentId<T>());

            if (mComponentArrays[componentId])
                return;

            mComponentArrays[componentId] =
                new ComponentArray<T>(mFreyrOptions->ArchetypeChunkCapacity);
        }

      protected:
        template <typename T>
        ComponentArray<T>* GetComponentArray()
        {
            assert(mRegisteredComponents.contains(GetComponentId<T>()) &&
                   "Component not registered before use.");

            return static_cast<ComponentArray<T>*>(
                mComponentArrays[mRegisteredComponents.getIndex(
                    GetComponentId<T>())]);
        }

      private:
        std::shared_ptr<FreyrOptions> mFreyrOptions;

        std::vector<std::mutex>      mMutexes;
        std::shared_ptr<TaskManager> mTaskManager;

        SparseSet<Entity>             mRegisteredEntities;
        SparseSet<ComponentId>&       mRegisteredComponents;
        std::vector<IComponentArray*> mComponentArrays;
    };
} // namespace FREYR_NAMESPACE
