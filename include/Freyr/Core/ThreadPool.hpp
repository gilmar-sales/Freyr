#pragma once

#include <Skirnir/Skirnir.hpp>

#include "Freyr/Containers/MPMCQueue.hpp"
#include "Freyr/Core/TaskCounter.hpp"

namespace FREYR_NAMESPACE
{
    using Task      = fr::function<void()>;
    using TaskQueue = rigtorp::mpmc::Queue<Task>;

    class ThreadPool
    {
        enum class State : unsigned
        {
            Empty,
            Resizing,
            Running,
            Idle
        };

        static constexpr std::uint32_t LCG_MULTIPLIER = 1103515245u;
        static constexpr std::uint32_t LCG_INCREMENT  = 12345u;

      public:
        ThreadPool(const skr::Arc<FreyrOptions>&            freyrOptions,
                   const skr::Arc<skr::Logger<ThreadPool>>& logger,
                   const skr::Arc<TaskCounter>&             taskCounter) :
            mLogger(logger), mFreyrOptions(freyrOptions), mTaskCounter(taskCounter), mThreadLane(1),
            mQueueIndex(0), mState(State::Empty)
        {
            Resize(freyrOptions->ThreadCount);
        }

        static thread_local size_t        ThreadId;
        static thread_local std::uint32_t mQueueLcgState;

        ~ThreadPool();

        void AddTask(auto&& func)
        {
            mTaskCounter->AddTasks(1);
            mQueueLcgState       = mQueueLcgState * LCG_MULTIPLIER + LCG_INCREMENT;
            const auto nextQueue = mQueueLcgState % mWorkerQueues.size();
            mWorkerQueues[nextQueue]->push(std::forward<decltype(func)>(func));
        }

        void Resize(std::uint32_t threadCount);

        void StartWorkers();
        void StopWorkers();

        void WaitForAllTasks() const { mTaskCounter->WaitForCompletion(); }

        void NotifyWorker() { mCondition.notify_one(); }

        void NotifyWorkers() { mCondition.notify_all(); }

        [[nodiscard]] bool IsRunning() const { return mState.load() == State::Running; }

      private:
        void workerLoop(TaskQueue* workerQueue);

        skr::Arc<skr::Logger<ThreadPool>> mLogger;
        skr::Arc<FreyrOptions>            mFreyrOptions;
        skr::Arc<TaskCounter>             mTaskCounter;

        std::vector<std::thread> mWorkers;
        std::atomic<int>         mThreadLane;
        std::vector<TaskQueue*>  mWorkerQueues;

        std::mutex              mMutex;
        std::condition_variable mCondition;
        std::atomic<State>      mState;

        alignas(64) std::atomic<std::uint32_t> mQueueIndex;
    };
} // namespace FREYR_NAMESPACE
