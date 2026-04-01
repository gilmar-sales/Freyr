#pragma once

#include "Freyr/Containers/MPMCQueue.hpp"
#include "Freyr/Core/TaskCounter.hpp"
#include <Skirnir/Skirnir.hpp>
#include <immintrin.h>

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

      public:
        TaskManager(const Ref<FreyrOptions>&             freyrOptions,
                    const Ref<skr::Logger<TaskManager>>& logger,
                    const Ref<TaskCounter>&              taskCounter) :
            mLogger(logger), mFreyrOptions(freyrOptions), mTaskCounter(taskCounter), mThreadLane(1), mQueueIndex(0),
            mState(State::Empty)
        {
            Resize(freyrOptions->ThreadCount);
        }

        static thread_local size_t ThreadId;

        ~TaskManager();
        void AddTask(auto&& func)
        {
            mTaskCounter->AddTasks(1);

            if (ThreadId > 0)
            {
                mWorkerQueues[ThreadId - 1]->push(std::forward<decltype(func)>(func));
                return;
            }

            size_t minSize  = std::numeric_limits<size_t>::max();
            size_t minIndex = 0;

            for (size_t i = 0; i < mWorkerQueues.size(); ++i)
            {
                const size_t s = mWorkerQueues[i]->size();

                if (s == 0)
                {
                    minIndex = i;
                    break;
                }

                if (s < minSize)
                {
                    minSize  = s;
                    minIndex = i;
                }
            }

            mWorkerQueues[minIndex]->push(std::forward<decltype(func)>(func));
        }

        void Resize(std::uint32_t threadCount);
        void StartWorkers();
        void StopWorkers();

        void WaitForAllTasks() const { mTaskCounter->WaiForCompletion(); }
        void NotifyWorker() { mCondition.notify_one(); }
        void NotifyWorkers() { mCondition.notify_all(); }
        void BeginProfiling();

        bool IsRunning() const { return mState.load() == State::Running; }

      private:
        void workerLoop(TaskQueue* workerQueue);

        Ref<skr::Logger<TaskManager>> mLogger;
        Ref<FreyrOptions>             mFreyrOptions;
        Ref<TaskCounter>              mTaskCounter;

        std::vector<std::string> mWorkersDescriptions;
        std::vector<std::thread> mWorkers;
        std::atomic<int>         mThreadLane;
        std::vector<TaskQueue*>  mWorkerQueues;

        std::mutex              mMutex;
        std::condition_variable mCondition;
        std::atomic<State>      mState;

        alignas(64) std::atomic<std::uint32_t> mQueueIndex;
    };

} // namespace FREYR_NAMESPACE