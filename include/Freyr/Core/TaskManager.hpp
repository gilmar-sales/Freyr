#pragma once

#include <Skirnir/Skirnir.hpp>

#include "Freyr/Containers/MPMCQueue.hpp"

namespace FREYR_NAMESPACE
{
    using Task      = std::function<void()>;
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
            mLogger(logger), mThreadCount(1), mAvaiableTasks(freyrOptions->MaxEntities), mState(State::Empty)
        {
            Resize(freyrOptions->ThreadCount);
        }

        static thread_local size_t ThreadId;

        ~TaskManager();

        void AddTask(auto&& func) { mAvaiableTasks.push(std::forward<decltype(func)>(func)); }

        void Resize(std::uint32_t threadCount);

        void StartWorkers();
        void StopWorkers();

        void NotifyWorker() { mCondition.notify_one(); }

        void NotifyWorkers() { mCondition.notify_all(); }

        void BeginProfiling();

      private:
        void workerLoop();

        Ref<skr::Logger<TaskManager>> mLogger;
        std::vector<std::string>      mWorkersDescriptions;
        std::vector<std::thread>      mWorkers;
        std::atomic<int>              mThreadCount;
        TaskQueue                     mAvaiableTasks;

        std::mutex              mMutex;
        std::condition_variable mCondition;
        std::atomic<State>      mState;
    };
} // namespace FREYR_NAMESPACE