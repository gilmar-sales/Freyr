#pragma once

#include "Freyr/Containers/ComponentArray.hpp"
#include "Freyr/Core/Profiling.hpp"
#include "Freyr/Core/TaskManager.hpp"
#include "Freyr/Meta/Iteration.hpp"

namespace FREYR_NAMESPACE
{
    class Archetype;

    class ArchetypeChunk
    {
      public:
        explicit ArchetypeChunk(std::string*               internalName,
                                SparseSet<ComponentEntry>* registeredComponents,
                                const Ref<FreyrOptions>&   freyrOptions,
                                const Ref<TaskManager>&    taskManager,
                                const Ref<TaskCounter>&    taskCounter) :
            mFreyrOptions(freyrOptions), mQueue(freyrOptions->ArchetypeChunkCapacity * 2), mInternalName(internalName),
            mTaskManager(taskManager), mRegisteredEntities(freyrOptions->ArchetypeChunkCapacity),
            mRegisteredComponents(registeredComponents), mLocalTaskCounter(0), mTaskCounter(taskCounter)
        {
            if (registeredComponents)
                mComponentArrays.resize(registeredComponents->size());
        }

        ~ArchetypeChunk()
        {
            for (const auto& componentArray : mComponentArrays)
            {
                delete componentArray;
            }
        }

        bool TryAddEntity(const Entity entity)
        {
            mRegisteredEntities.insert(entity);

            if (mRegisteredEntities.getIndex(entity) < mFreyrOptions->ArchetypeChunkCapacity)
                return true;

            mRegisteredEntities.remove(entity);

            return false;
        }

        void RemoveEntity(const Entity entity) { InternalRemoveEntity(entity); }

        template <typename T>
        void AddComponent(const Entity entity, T component)
        {
            (*GetComponentArray<T>())[mRegisteredEntities.getIndex(entity)] = component;
        }

        template <typename... Ts>
        void AddComponents(const Entity entity, const Ts&... components, auto&& callback)
        {
            EnqueueTask([this, entity, components..., callback] {
                meta::forEach(
                    [&]<typename TComponent>(TComponent&& component) {
                        using T = std::remove_reference_t<TComponent>;
                        (*GetComponentArray<T>())[mRegisteredEntities.getIndex(entity)] = component;
                    },
                    std::make_tuple(components...));

                callback(entity, GetComponent<Ts>(entity)...);

                mLocalTaskCounter.fetch_sub(1);

                NextTask();

                mTaskCounter->taskCompleted();
            });
        }

        template <typename T>
        void RemoveComponent(const Entity entity)
        {
            GetComponentArray<T>()->RemoveData(entity);
        }

        template <typename T>
        T& GetComponent(const Entity entity)
        {
            return GetComponentArray<T>()->GetComponent(mRegisteredEntities.getIndex(entity));
        }

        template <typename... Ts>
        std::tuple<Ts&...> GetComponents(const Entity entity)
        {
            return std::tuple<Ts&...>(GetComponentArray<Ts>()->GetData(entity)...);
        }

        template <typename... Components>
        void ForEach(const std::string label, auto&& function)
        {
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

            auto tuple = std::make_tuple(GetComponentArray<Components>()...);

            for (auto index = mRegisteredEntities.lastIndex(); index + 1 != 0; index--)
            {
                const auto entity = mRegisteredEntities.getDense()[index];
                function(entity, std::get<ComponentArray<Components>*>(tuple)->GetComponent(index)...);
            }

            FREYR_PROFILING_END("FREYR", perfetto::Track(TaskManager::ThreadId));
        }

        template <typename... Components>
        void ForEachAsync(const std::string label, auto&& function)
        {
            EnqueueTask([this, label, function] {
                ForEach<Components...>(label, function);

                mLocalTaskCounter.fetch_sub(1);
                NextTask();

                mTaskCounter->taskCompleted();
            });
        }

        template <typename... Components>
        void ForEachParallel(const std::string label, auto&& function, Entity index)
        {
            FREYR_PROFILING_BEGIN("FREYR",
                                  label.data(),
                                  perfetto::Track((uint64_t) this),
                                  "EntityCount",
                                  mRegisteredEntities.size());
            auto tuple = std::make_tuple(GetComponentArray<Components>()...);
#if __cpp_lib_parallel_algorithm >= 201603L

            std::for_each(std::execution::par,
                          mRegisteredEntities.begin(),
                          mRegisteredEntities.end(),
                          [&](const auto& entity) {
                              function(entity,
                                       index + mRegisteredEntities.getIndex(entity),
                                       std::get<ComponentArray<Components>*>(tuple)->GetComponent(
                                           mRegisteredEntities.getIndex(entity))...);
                          });
#else

            std::for_each(mRegisteredEntities.begin(), mRegisteredEntities.end(), [&](const auto& entity) {
                function(entity,
                         index + mRegisteredEntities.getIndex(entity),
                         std::get<ComponentArray<Components>*>(tuple)->GetComponent(
                             mRegisteredEntities.getIndex(entity))...);
            });
#endif
            FREYR_PROFILING_END("FREYR", perfetto::Track((uint64_t) this));
        }

        template <typename... Components>
        void Map(auto&&                                                                         mapFunction,
                 Entity                                                                         index,
                 std::vector<decltype(mapFunction(*(new Entity {}), *(new Components {})...))>& buffer)
        {
            auto tuple = std::make_tuple(GetComponentArray<Components>()...);

#if __cpp_lib_parallel_algorithm >= 201603L
            std::for_each(std::execution::par,
                          mRegisteredEntities.begin(),
                          mRegisteredEntities.end(),
                          [&](const auto& entity) {
                              buffer[index + mRegisteredEntities.getIndex(entity)] =
                                  mapFunction(entity,
                                              std::get<ComponentArray<Components>*>(tuple)->GetComponent(
                                                  mRegisteredEntities.getIndex(entity))...);
                          });
#else
            std::for_each(mRegisteredEntities.begin(), mRegisteredEntities.end(), [&](const auto& entity) {
                buffer[index + mRegisteredEntities.getIndex(entity)] =
                    mapFunction(entity,
                                std::get<ComponentArray<Components>*>(tuple)->GetComponent(
                                    mRegisteredEntities.getIndex(entity))...);
            });
#endif
        }

        template <typename... Components>
        void ForEach(const std::string label, SparseSet<Entity>& entities, auto&& function)
        {
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
            auto tuple = std::make_tuple(GetComponentArray<Components>()...);

#if __cpp_lib_execution >= 201603L
            std::for_each(std::execution::seq, entities.begin(), entities.end(), [&](const auto& entity) {
                if (!mRegisteredEntities.contains(entity))
                    return;
                function(entity,
                         std::get<ComponentArray<Components>*>(tuple)->GetComponent(
                             mRegisteredEntities.getIndex(entity))...);
            });
#else
            std::for_each(entities.begin(), entities.end(), [&](const auto& entity) {
                if (!mRegisteredEntities.contains(entity))
                    return;
                function(entity, std::get<ComponentArray<Components>*>(tuple)->GetComponent(entity)...);
            });
#endif
            FREYR_PROFILING_END("FREYR", perfetto::Track((uint64_t) this));
        }

        template <typename... Components>
        void ForEachParallel(std::string label, SparseSet<Entity>& entities, auto&& function)
        {
            FREYR_PROFILING_BEGIN("FREYR", "Lock", perfetto::Track((size_t) this), "Task", label.data());

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

#if __cpp_lib_parallel_algorithm >= 201603L
            std::for_each(std::execution::par, entities.begin(), entities.end(), [&](const auto& entity) {
                if (!mRegisteredEntities.contains(entity))
                    return;

                function(entity, GetComponentArray<Components>()->GetData(entity)...);
            });
#else
            std::for_each(entities.begin(), entities.end(), [&](const auto& entity) {
                if (!mRegisteredEntities.contains(entity))
                    return;

                function(entity, GetComponentArray<Components>()->GetData(entity)...);
            });
#endif
            FREYR_PROFILING_END("FREYR", perfetto::Track((uint64_t) this));
        }

        bool IsFull() { return mRegisteredEntities.size() >= mFreyrOptions->ArchetypeChunkCapacity; }

        template <typename T>
        void AddComponentArray()
        {
            if (mComponentArrays.capacity() < GetComponentId<T>() + 1)
            {
                mComponentArrays.resize(GetComponentId<T>() + 1);
            }

            if (mComponentArrays.contains(GetComponentId<T>()))
                return;

            mComponentArrays.insert(new ComponentArray<T>(mFreyrOptions));
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
                mComponentArrays[component]->Swap(a, b);
            }

            mRegisteredEntities.swap(a, b);
        }

        inline void CopyEntity(const Entity from, const Entity to, const ArchetypeChunk* chunk) const
        {
            for (auto component : *mRegisteredComponents)
            {
                mComponentArrays[component]->CopyComponent(from, to, chunk->mComponentArrays[component]);
            }
        }

        inline void MoveData(Entity entity, ArchetypeChunk* chunk)
        {
            for (auto const& component : *mRegisteredComponents)
            {
                mComponentArrays[component]->CopyComponent(mRegisteredEntities.getIndex(entity),
                                                           chunk->mRegisteredEntities.getIndex(entity),
                                                           chunk->mComponentArrays[component]);
            }

            InternalRemoveEntity(entity);
        }

        void StartTasks() { NextTask(); }

        void NextTask()
        {
            if (Task task; mQueue.try_pop(task))
            {
                mTaskManager->AddTask(std::move(task));
                mLocalTaskCounter.fetch_add(1);
            }
        }

      protected:
        void InternalRemoveEntity(Entity entity)
        {
            for (const auto componentArray : mComponentArrays)
            {
                componentArray->Remove(mRegisteredEntities.getIndex(entity), mRegisteredEntities.size() - 1);
            }

            mRegisteredEntities.remove(entity);
        }

        template <typename T>
        ComponentArray<T>* GetComponentArray()
        {
            return static_cast<ComponentArray<T>*>(GetComponentArray(GetComponentId<T>()));
        }

        [[nodiscard]] IComponentArray* GetComponentArray(const ComponentId componentId) const
        {
            FREYR_ASSERT(mComponentArrays.contains(componentId) && "Component not registered before use.");

            return mComponentArrays[componentId];
        }

        void EnqueueTask(auto&& task)
        {
            mQueue.push(std::move(Task(task)));

            if (mTaskManager->IsRunning() && mLocalTaskCounter.load() <= 0)
                NextTask();
        }

      private:
        friend class Archetype;

        Ref<FreyrOptions> mFreyrOptions;

        rigtorp::MPMCQueue<Task> mQueue;
        std::atomic<int>         mLocalTaskCounter;

        Ref<TaskManager> mTaskManager;
        Ref<TaskCounter> mTaskCounter;

        SparseSet<Entity>           mRegisteredEntities;
        SparseSet<ComponentEntry>*  mRegisteredComponents;
        std::string*                mInternalName;
        SparseSet<IComponentArray*> mComponentArrays;
    };
} // namespace FREYR_NAMESPACE
