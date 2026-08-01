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
        explicit ArchetypeChunk(const std::string_view   internalName,
                                const skr::Arc<FreyrOptions>& freyrOptions,
                                const skr::Arc<ThreadPool>&   taskManager,
                                const skr::Arc<TaskCounter>&  taskCounter) :
            mFreyrOptions(freyrOptions), mQueue(freyrOptions->ArchetypeChunkCapacity * 2),
            mLocalTaskCounter(0), mThreadPool(taskManager), mTaskCounter(taskCounter),
            mRegisteredEntities(freyrOptions->ArchetypeChunkCapacity), mInternalName(internalName)
        {
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
            });
        }

        template <typename T>
        void RemoveComponent(const Entity entity)
        {
            GetComponentArray<T>()->Remove(entity, mRegisteredEntities.lastIndex());
        }

        template <typename T>
        T& GetComponent(const Entity entity)
        {
            return GetComponentArray<T>()->GetComponent(mRegisteredEntities.getIndex(entity));
        }

        template <typename... Ts>
        std::tuple<Ts&...> GetComponents(const Entity entity)
        {
            return std::tuple<Ts&...>(
                GetComponentArray<Ts>()->GetComponent(mRegisteredEntities.getIndex(entity))...);
        }

        template <typename... Components>
        inline void ForEach(const char* label, auto&& function)
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
                         std::get<ComponentArray<Components>*>(tuple)->GetComponent(entity)...);
            });
        }

        bool IsFull() const
        {
            return mRegisteredEntities.size() >= mFreyrOptions->ArchetypeChunkCapacity;
        }

        template <typename T>
        void AddComponentArray()
        {
            if (mComponentArrays.capacity() < GetComponentId<T>() + 1)
            {
                mComponentArrays.resize(GetComponentId<T>() + 1);
            }

            if (mComponentArrays.contains(GetComponentId<T>()))
                return;

            mComponentArrays.insert(new ComponentArray<T>(mFreyrOptions->ArchetypeChunkCapacity));
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
            const size_t indexA = mRegisteredEntities.getIndex(a);
            const size_t indexB = mRegisteredEntities.getIndex(b);

            for (const auto componentArray : mComponentArrays)
            {
                componentArray->Swap(indexA, indexB);
            }
            mRegisteredEntities.swap(a, b);
        }

        inline void CopyEntity(const Entity          from,
                               const Entity          to,
                               const ArchetypeChunk* chunk) const
        {
            for (auto component : mComponentArrays)
            {
                mComponentArrays[component]->CopyComponent(from,
                                                           to,
                                                           chunk->mComponentArrays[component]);
            }
        }

        inline void MoveData(const Entity entity, const ArchetypeChunk* chunk)
        {
            const auto index       = mRegisteredEntities.getIndex(entity);
            const auto targetIndex = chunk->mRegisteredEntities.getIndex(entity);

            for (auto const& component : mComponentArrays)
            {
                if (!chunk->mComponentArrays.contains(component->GetComponentId()))
                    continue;

                mComponentArrays[component]->CopyComponent(index,
                                                           targetIndex,
                                                           chunk->mComponentArrays[component]);
            }

            InternalRemoveEntity(entity);
        }

        void StartTasks()
        {
            mThreadPool->AddTask(Task { [this] {
                Task task;
                while (mQueue.try_pop(task))
                {
                    task();
                }
                mLocalTaskCounter.fetch_sub(1);
            } });
            mLocalTaskCounter.fetch_add(1);
        }

        void NextTask()
        {
            if (Task task; mQueue.try_pop(task))
            {
                mThreadPool->AddTask(std::move(task));
                mLocalTaskCounter.fetch_add(1);
            }
        }

        void EnqueueTask(auto&& task)
        {
            mQueue.push(std::move(Task(task)));

            if (mThreadPool->IsRunning() && mLocalTaskCounter.load() <= 0)
                StartTasks();
        }

      protected:
        void InternalRemoveEntity(Entity entity)
        {
            const size_t indexToRemove = mRegisteredEntities.getIndex(entity);
            const size_t lastIndex     = mRegisteredEntities.size() - 1;

            for (const auto componentArray : mComponentArrays)
            {
                componentArray->Remove(indexToRemove, lastIndex);
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
            FREYR_ASSERT(mComponentArrays.contains(componentId) &&
                         "Component not registered before use.");

            return mComponentArrays[componentId];
        }

      private:
        friend class Archetype;
        friend class ArchetypeBuilder;

        alignas(64) std::atomic<int> mLocalTaskCounter;
        alignas(64) skr::Arc<TaskCounter> mTaskCounter;

        rigtorp::MPMCQueue<Task> mQueue;

        skr::Arc<ThreadPool>   mThreadPool;
        skr::Arc<FreyrOptions> mFreyrOptions;

        SparseSet<Entity>           mRegisteredEntities;
        std::string_view            mInternalName;
        SparseSet<IComponentArray*> mComponentArrays;
    };
} // namespace FREYR_NAMESPACE
