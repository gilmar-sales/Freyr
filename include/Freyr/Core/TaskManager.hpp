#pragma once

#include <latch>

namespace FREYR_NAMESPACE
{
    using Task      = std::move_only_function<void()>;
    using TaskQueue = std::queue<Task>;

    class TaskManager
    {
      public:
        explicit TaskManager(
            const std::shared_ptr<FreyrOptions>& freyrOptions) :
            mRunning(true), mReservedTasks(0)
        {
            Resize(freyrOptions->ThreadCount);
        }

        ~TaskManager()
        {

            mRunning = false;
            mCondition.notify_all();

            for (auto& worker : mWorkers)
            {
                if (worker.joinable())
                {
                    worker.join();
                }
            }
        }

        void ReserveTask()
        {
            mTasksCompleted =
                std::make_shared<std::latch>(mReservedTasks.fetch_add(1));
        }

        template <typename Func>
        void AddTask(Func&& func)
        {
            std::lock_guard lock(mMutex);
            mAvaiableTasks.push(std::forward<Func>(func));
            mCondition.notify_one();
        }

        void Resize(const std::uint32_t threadCount)
        {
            std::unique_lock lock(mMutex);

            if (mRunning)
            {
                WaitTasks();

                for (int i = 0; i < threadCount; ++i)
                {
                    mWorkers.emplace_back([this] { workerLoop(); });
                }
            }
            else
            {
                for (int i = 0; i < threadCount; ++i)
                {
                    mWorkers.emplace_back([this] { workerLoop(); });
                }
            }
        }

        void WaitTasks()
        {
            if (!mReservedTasks.load())
                return;

            {
                std::unique_lock lock(mMutex);
                mTasksCompleted =
                    std::make_shared<std::latch>(mReservedTasks.load());
            }

            mCondition.notify_all();

            if (!mTasksCompleted->try_wait())
            {
                mTasksCompleted->wait();
            }

            mReservedTasks.store(0);
        }

        void NotifyWorker() { mCondition.notify_one(); }

        static void StartProfiling()
        {
            const auto id =
                std::hash<std::thread::id> {}(std::this_thread::get_id());

            const auto label = std::format("Thread: {}", id);

            FREYR_PROFILING_BEGIN("FREYR",
                                  label.c_str(),
                                  perfetto::Track(id),
                                  "ThreadId",
                                  id);
        }

        static void EndProfiling()
        {
            const auto id =
                std::hash<std::thread::id> {}(std::this_thread::get_id());

            FREYR_PROFILING_END("FREYR", perfetto::Track(id));
        }

      private:
        void workerLoop()
        {
            StartProfiling();
            while (true)
            {
                Task task;
                {
                    const auto id = std::hash<std::thread::id> {}(
                        std::this_thread::get_id());

                    FREYR_PROFILING_BEGIN("FREYR",
                                          "Idle Lock",
                                          perfetto::Track(id),
                                          "ThreadId",
                                          id);
                    std::unique_lock lock(mMutex);
                    FREYR_PROFILING_END("FREYR", perfetto::Track(id));

                    FREYR_PROFILING_BEGIN("FREYR",
                                          "Idle Wait",
                                          perfetto::Track(id),
                                          "ThreadId",
                                          id);
                    mCondition.wait(lock, [this] {
                        return !mRunning || !mAvaiableTasks.empty();
                    });
                    FREYR_PROFILING_END("FREYR", perfetto::Track(id));

                    if (!mRunning)
                    {
                        EndProfiling();
                        return;
                    }

                    task = std::move(mAvaiableTasks.front());
                    mAvaiableTasks.pop();
                }

                task();

                mReservedTasks.fetch_sub(1);
                mTasksCompleted->count_down();
                mCondition.notify_one();
            }
        }

        std::vector<std::thread>   mWorkers;
        TaskQueue                  mAvaiableTasks;
        std::atomic<unsigned long> mReservedTasks;

        std::mutex                  mMutex;
        std::condition_variable     mCondition;
        std::shared_ptr<std::latch> mTasksCompleted;

        bool mRunning;
    };
} // namespace FREYR_NAMESPACE