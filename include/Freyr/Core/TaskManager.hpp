#pragma once

#include <Skirnir/Skirnir.hpp>

#include "Freyr/Containers/MPMCQueue.hpp"
#include "Freyr/Core/TaskCounter.hpp"

namespace FREYR_NAMESPACE
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

        struct alignas(64) WorkerState
        {
            std::mutex              mutex;
            std::condition_variable cv;
        };

      public:
        TaskManager(const Ref<FreyrOptions>& freyrOptions, const Ref<skr::Logger<TaskManager>>& logger,
                    const Ref<TaskCounter>& taskCounter) :
            mLogger(logger), mFreyrOptions(freyrOptions), mTaskCounter(taskCounter), mThreadLane(1), mQueueIndex(0),
            mState(State::Empty)
        {
            Resize(freyrOptions->ThreadCount);
        }

        static thread_local size_t ThreadId;

        ~TaskManager();

        void AddTask(auto&& func)
        {
            const auto nextQueue = mQueueIndex.fetch_add(1) % mWorkerQueues.size();

            mTaskCounter->AddTasks(1);
            mWorkerQueues[nextQueue]->push(std::forward<decltype(func)>(func));

            if (mState.load() == State::Running)
                NotifyWorkers();
        }

        bool Resize(std::uint32_t threadCount);

        void StartWorkers();
        void StopWorkers();

        void WaitForAllTasks() const { mTaskCounter->WaiForCompletion(); }

        void NotifyWorkers() const
        {
            for (auto& ws : mWorkerStates)
                ws->cv.notify_one();
        }

        void BeginProfiling();

        [[nodiscard]] bool IsRunning() const { return mState.load() == State::Running; }

      private:
        void workerLoop(TaskQueue* myQueue, WorkerState* myState);

        Ref<skr::Logger<TaskManager>> mLogger;
        Ref<FreyrOptions>             mFreyrOptions;
        Ref<TaskCounter>              mTaskCounter;

        std::vector<std::string> mWorkersDescriptions;
        std::vector<std::thread> mWorkers;
        std::atomic<int>         mThreadLane;

        std::atomic<std::uint32_t>                mQueueIndex;
        std::vector<std::unique_ptr<TaskQueue>>   mWorkerQueues;
        std::vector<std::unique_ptr<WorkerState>> mWorkerStates;

        std::atomic<State> mState;
    };
} // namespace FREYR_NAMESPACE