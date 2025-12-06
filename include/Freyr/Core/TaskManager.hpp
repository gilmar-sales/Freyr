#pragma once

#include <Skirnir/Skirnir.hpp>

#include "Freyr/Containers/MPMCQueue.hpp"

namespace
FREYR_NAMESPACE
{
    using Task      = fr::function<void()>;
    using TaskQueue = rigtorp::mpmc::Queue<Task>;

    class TaskManager
    {
        enum class State : unsigned
        {
            Empty,
            Resizing,
            Running,
            Idle
        };

    public:
        TaskManager(const Ref<FreyrOptions>& freyrOptions, const Ref<skr::Logger<TaskManager>>& logger) :
            mLogger(logger), mThreadLane(1), mFreyrOptions(freyrOptions), mState(State::Empty), mQueueIndex(0)
        {
            Resize(freyrOptions->ThreadCount);
        }

        static thread_local size_t ThreadId;

        ~TaskManager();

        void AddTask(auto&& func)
        {
            const auto nextQueue = mQueueIndex.fetch_add(1) % mWorkerQueues.size();

            mWorkerQueues[nextQueue]->push(std::forward<decltype(func)>(func));
        }

        void Resize(std::uint32_t threadCount);

        void StartWorkers();
        void StopWorkers();

        void NotifyWorker() { mCondition.notify_one(); }

        void NotifyWorkers() { mCondition.notify_all(); }

        void BeginProfiling();

    private:
        void workerLoop(TaskQueue* workerQueue);

        Ref<skr::Logger<TaskManager>> mLogger;
        Ref<FreyrOptions>             mFreyrOptions;
        std::vector<std::string>      mWorkersDescriptions;
        std::vector<std::thread>      mWorkers;
        std::atomic<int>              mThreadLane;

        std::atomic<std::uint32_t> mQueueIndex;
        std::vector<TaskQueue*>    mWorkerQueues;

        std::mutex              mMutex;
        std::condition_variable mCondition;
        std::atomic<State>      mState;
    };
} // namespace FREYR_NAMESPACE