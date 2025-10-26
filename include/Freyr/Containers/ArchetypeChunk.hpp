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
        explicit ArchetypeChunk(std::string*             internalName,
                                SparseSet<ComponentId>*  registeredComponents,
                                const Ref<FreyrOptions>& freyrOptions,
                                const Ref<TaskManager>&  taskManager) :
            mFreyrOptions(freyrOptions), mMutexes(registeredComponents->size()),
            mInternalName(internalName), mTaskManager(taskManager),
            mRegisteredEntities(freyrOptions->MaxEntities),
            mRegisteredComponents(registeredComponents)
        {
            mComponentArrays.resize(registeredComponents->size());
        }

        ~ArchetypeChunk()
        {
            for (const auto& componentId : *mRegisteredComponents)
            {
                delete GetComponentArray(componentId);
            }
        }

        void AddEntity(const Entity entity)
        {
            mRegisteredEntities.insert(entity);

            for (auto const& component : *mRegisteredComponents)
            {
                mComponentArrays[mRegisteredComponents->getIndex(component)]
                    ->AddEntity(entity);
            }
        }

        void RemoveEntity(const Entity entity)
        {
            for (auto const& component : *mRegisteredComponents)
            {
                mComponentArrays[mRegisteredComponents->getIndex(component)]
                    ->RemoveEntity(entity);
            }

            mRegisteredEntities.remove(entity);
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

        template <typename... Ts>
        std::tuple<Ts&...> GetComponents(const Entity& entity)
        {
            return std::tuple<Ts&...>(
                GetComponentArray<Ts>()->GetData(entity)...);
        }

        template <typename... Components>
        void ForEach(const std::string label, auto&& function)
        {
            FREYR_PROFILING_BEGIN("FREYR",
                                  "Lock",
                                  perfetto::Track(TaskManager::ThreadId),
                                  "Task",
                                  label.data());

            std::scoped_lock lock(GetMutex<Components>()...);

            FREYR_PROFILING_END("FREYR",
                                perfetto::Track(TaskManager::ThreadId));

            FREYR_PROFILING_BEGIN(
                "FREYR",
                label.data(),
                perfetto::Track(TaskManager::ThreadId),
                "Archetype",
                mInternalName->c_str(),
                "ArchetypeChunk",
                (size_t) this,
                "EntityCount",
                mRegisteredEntities.size(),
                "ThreadId",
                TaskManager::ThreadId);

            for (const auto& entity : mRegisteredEntities)
            {
                function(entity,
                         GetComponentArray<Components>()->GetData(entity)...);
            }

            FREYR_PROFILING_END("FREYR",
                                perfetto::Track(TaskManager::ThreadId));
        }

        template <typename... Components>
        void ForEachAsync(const std::string label,
                          auto&&            function,
                          Ref<std::latch>&  latch)
        {
            mTaskQueue.push([this, label, function, latch] {
                FREYR_PROFILING_BEGIN("FREYR",
                                      "Lock",
                                      perfetto::Track(TaskManager::ThreadId),
                                      "Task",
                                      label.data());

                std::scoped_lock lock(GetMutex<Components>()...);

                FREYR_PROFILING_END("FREYR",
                                    perfetto::Track(TaskManager::ThreadId));

                FREYR_PROFILING_BEGIN(
                    "FREYR",
                    label.data(),
                    perfetto::Track(TaskManager::ThreadId),
                    "Archetype",
                    mInternalName->c_str(),
                    "ArchetypeChunk",
                    (size_t) this,
                    "EntityCount",
                    mRegisteredEntities.size(),
                    "ThreadId",
                    TaskManager::ThreadId);

                for (const auto& entity : mRegisteredEntities)
                {
                    function(
                        entity,
                        GetComponentArray<Components>()->GetData(entity)...);
                }

                FREYR_PROFILING_END("FREYR",
                                    perfetto::Track(TaskManager::ThreadId));

                NextTask();

                latch->count_down();
            });
        }

        template <typename... Components>
        void ForEachParallel(const std::string label,
                             auto&&            function,
                             Entity            index)
        {
            FREYR_PROFILING_BEGIN("FREYR",
                                  "Lock",
                                  perfetto::Track((size_t) this),
                                  "Task",
                                  label.data());

            std::scoped_lock lock(GetMutex<Components>()...);

            FREYR_PROFILING_END("FREYR", perfetto::Track((size_t) this));

            FREYR_PROFILING_BEGIN("FREYR",
                                  label.data(),
                                  perfetto::Track((uint64_t) this),
                                  "EntityCount",
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
            FREYR_PROFILING_BEGIN("FREYR",
                                  "Lock",
                                  perfetto::Track((size_t) this),
                                  "Task",
                                  typeid(mapFunction).name());

            std::scoped_lock lock(GetMutex<Components>()...);

            FREYR_PROFILING_END("FREYR", perfetto::Track((size_t) this));

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
        void ForEach(const std::string  label,
                     SparseSet<Entity>& entities,
                     auto&&             function)
        {
            FREYR_PROFILING_BEGIN("FREYR",
                                  "Lock",
                                  perfetto::Track((size_t) this),
                                  "Task",
                                  label.data());

            std::scoped_lock lock(GetMutex<Components>()...);

            FREYR_PROFILING_END("FREYR", perfetto::Track((size_t) this));

            FREYR_PROFILING_BEGIN(
                "FREYR",
                label.data(),
                perfetto::Track((uint64_t) this),
                "Archetype",
                mInternalName->c_str(),
                "ArchetypeChunk",
                (size_t) this,
                "EntityCount",
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
        void ForEachParallel(std::string        label,
                             SparseSet<Entity>& entities,
                             auto&&             function)
        {
            FREYR_PROFILING_BEGIN("FREYR",
                                  "Lock",
                                  perfetto::Track((size_t) this),
                                  "Task",
                                  label.data());

            std::scoped_lock lock(GetMutex<Components>()...);

            FREYR_PROFILING_END("FREYR", perfetto::Track((size_t) this));

            FREYR_PROFILING_BEGIN(
                "FREYR",
                label.data(),
                perfetto::Track((uint64_t) this),
                "Archetype",
                mInternalName->c_str(),
                "ArchetypeChunk",
                (size_t) this,
                "EntityCount",
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

        bool IsFull()
        {
            return mRegisteredEntities.size() >=
                   mFreyrOptions->ArchetypeChunkCapacity;
        }

        template <typename T>
        void AddComponentArray()
        {
            const auto componentId =
                mRegisteredComponents->getIndex(GetComponentId<T>());

            if (mComponentArrays.size() < mRegisteredComponents->size())
            {
                mComponentArrays.resize(mRegisteredComponents->size());
                mMutexes =
                    std::vector<std::mutex>(mRegisteredComponents->size());
            }

            mComponentArrays[componentId] =
                new ComponentArray<T>(mFreyrOptions);
        }

        size_t Count() { return mRegisteredEntities.size(); }

        void GetRegisteredEntities(std::vector<std::uint32_t>& vector) const
        {
            for (const auto& entity : mRegisteredEntities)
            {
                vector.push_back(entity);
            }
        }

        void Swap(const Entity a, const Entity b)
        {

            for (auto component : *mRegisteredComponents)
            {
                mComponentArrays[mRegisteredComponents->getIndex(component)]
                    ->Swap(a, b);
            }

            mRegisteredEntities.swap(a, b);
        }

        inline void CopyEntity(const Entity          from,
                               const Entity          to,
                               const ArchetypeChunk* chunk) const
        {
            for (auto const& component : *mRegisteredComponents)
            {
                mComponentArrays[mRegisteredComponents->getIndex(component)]
                    ->CopyEntity(
                        from,
                        to,
                        chunk->mComponentArrays[chunk->mRegisteredComponents
                                                    ->getIndex(component)]);
            }
        }

        inline void MoveData(Entity entity, ArchetypeChunk* chunk)
        {
            for (auto const& component : *mRegisteredComponents)
            {
                mComponentArrays[mRegisteredComponents->getIndex(component)]
                    ->MoveData(
                        entity,
                        chunk->mComponentArrays[chunk->mRegisteredComponents
                                                    ->getIndex(component)]);
            }
        }

        void StartTasks()
        {
            std::unique_lock lock(mMutex);

            if (mTaskQueue.empty())
            {
                return;
            }

            mTaskManager->AddTask(std::move(mTaskQueue.front()));
            mTaskQueue.pop();
        }

        void NextTask()
        {
            std::unique_lock lock(mMutex);

            if (mTaskQueue.empty())
            {
                return;
            }

            mTaskManager->AddTask(std::move(mTaskQueue.front()));
            mTaskQueue.pop();
        }

      protected:
        template <typename T>
        ComponentArray<T>* GetComponentArray()
        {
            return static_cast<ComponentArray<T>*>(
                GetComponentArray(GetComponentId<T>()));
        }

        [[nodiscard]] IComponentArray* GetComponentArray(
            const ComponentId componentId) const
        {
            FREYR_ASSERT(mRegisteredComponents->contains(componentId) &&
                         "Component not registered before use.");

            return mComponentArrays[mRegisteredComponents->getIndex(
                componentId)];
        }

        template <typename TComponent>
        std::mutex& GetMutex()
        {
            return mMutexes[mRegisteredComponents->getIndex(
                GetComponentId<TComponent>())];
        }

      private:
        friend class Archetype;

        Ref<FreyrOptions> mFreyrOptions;

        TaskQueue  mTaskQueue;
        std::mutex mMutex;

        Ref<TaskManager> mTaskManager;

        std::vector<std::mutex> mMutexes;

        SparseSet<Entity>             mRegisteredEntities;
        SparseSet<ComponentId>*       mRegisteredComponents;
        std::string*                  mInternalName;
        std::vector<IComponentArray*> mComponentArrays;
    };
} // namespace FREYR_NAMESPACE
