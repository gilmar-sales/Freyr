#include "Freyr/Containers/ArchetypeChunk.hpp"

namespace FREYR_NAMESPACE
{
    ArchetypeChunk::ArchetypeChunk(const std::string_view        internalName,
                                   const skr::Arc<FreyrOptions>& freyrOptions,
                                   const skr::Arc<ThreadPool>&   taskManager,
                                   const skr::Arc<TaskCounter>&  taskCounter) :
        mFreyrOptions(freyrOptions), mQueue(), mLocalTaskCounter(0), mThreadPool(taskManager),
        mTaskCounter(taskCounter), mRegisteredEntities(freyrOptions->ArchetypeChunkCapacity),
        mInternalName(internalName)
    {
    }

    ArchetypeChunk::~ArchetypeChunk()
    {
        for (const auto& componentArray : mComponentArrays)
        {
            delete componentArray;
        }
    }

    bool ArchetypeChunk::TryAddEntity(const Entity entity)
    {
        mRegisteredEntities.insert(entity);

        if (mRegisteredEntities.getIndex(entity) < mFreyrOptions->ArchetypeChunkCapacity)
            return true;

        mRegisteredEntities.remove(entity);

        return false;
    }

    void ArchetypeChunk::RemoveEntity(const Entity entity)
    {
        InternalRemoveEntity(entity);
    }

    bool ArchetypeChunk::IsFull() const
    {
        return mRegisteredEntities.size() >= mFreyrOptions->ArchetypeChunkCapacity;
    }

    size_t ArchetypeChunk::Count() const
    {
        return mRegisteredEntities.size();
    }

    void ArchetypeChunk::GetRegisteredEntities(std::vector<std::uint32_t>& vector) const
    {
        for (const auto& entity : mRegisteredEntities)
        {
            vector.push_back(entity);
        }
    }

    void ArchetypeChunk::Swap(const Entity a, const Entity b)
    {
        const size_t indexA = mRegisteredEntities.getIndex(a);
        const size_t indexB = mRegisteredEntities.getIndex(b);

        for (const auto componentArray : mComponentArrays)
        {
            componentArray->Swap(indexA, indexB);
        }
        mRegisteredEntities.swap(a, b);
    }

    void ArchetypeChunk::CopyEntity(
        const Entity from, const Entity to, const ArchetypeChunk* chunk) const
    {
        const auto fromIndex = mRegisteredEntities.getIndex(from);
        const auto toIndex   = chunk->mRegisteredEntities.getIndex(to);

        for (auto component : mComponentArrays)
        {
            mComponentArrays[component]->CopyComponent(fromIndex,
                                                       toIndex,
                                                       chunk->mComponentArrays[component]);
        }
    }

    void ArchetypeChunk::MoveData(const Entity entity, const ArchetypeChunk* chunk)
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

    void ArchetypeChunk::StartTasks()
    {
        int expected = 0;
        if (!mLocalTaskCounter.compare_exchange_strong(expected, 1))
            return;

        mThreadPool->AddTask(Task { [this] {
            Task task;
            while (mQueue.try_pop(task))
            {
                task();
            }
            mLocalTaskCounter.fetch_sub(1);
        } });
    }

    void ArchetypeChunk::NextTask()
    {
        if (Task task; mQueue.try_pop(task))
        {
            mThreadPool->AddTask(std::move(task));
            mLocalTaskCounter.fetch_add(1);
        }
    }

    void ArchetypeChunk::EnqueueTask(Task task)
    {
        mQueue.push(std::move(task));

        if (mThreadPool->IsRunning() && mLocalTaskCounter.load() <= 0)
            StartTasks();
    }

    void ArchetypeChunk::InternalRemoveEntity(const Entity entity)
    {
        const size_t indexToRemove = mRegisteredEntities.getIndex(entity);
        const size_t lastIndex     = mRegisteredEntities.size() - 1;

        for (const auto componentArray : mComponentArrays)
        {
            componentArray->Remove(indexToRemove, lastIndex);
        }

        mRegisteredEntities.remove(entity);
    }

    IComponentArray* ArchetypeChunk::GetComponentArray(const ComponentId componentId) const
    {
        FREYR_ASSERT(mComponentArrays.contains(componentId) &&
                     "Component not registered before use.");

        return mComponentArrays[componentId];
    }
} // namespace FREYR_NAMESPACE
