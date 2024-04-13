#pragma once

namespace FREYR_NAMESPACE
{
    class TaskManager
    {
      public:
        TaskManager(std::uint64_t threadCount = std::thread::hardware_concurrency()) : mRunning(false), mThreadCount(threadCount)
        {
        }

        ~TaskManager()
        {
            StopTasks();
            WaitTasks();
        }

        template<typename Func>
        void AddTask(Func func)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            tasks.push(func);
        }

        void Resize(std::uint64_t threadCount)
        {
            std::unique_lock<std::mutex> lock(mMutex);
            mThreadCount = threadCount;

            if (mRunning)
            {
                StopTasks();
                WaitTasks();

                for (int i = 0; i < mThreadCount; ++i)
                {
                    workers.emplace_back(std::thread{[this] { workerLoop(); }});
                }

                StartTasks();
            }
            else
            {
                for (int i = 0; i < mThreadCount; ++i)
                {
                    workers.emplace_back(std::thread{[this] { workerLoop(); }});
                }
            }
        }

        void StartTasks()
        {
            Resize(mThreadCount);

            mRunning = true;

            mCondition.notify_all();
        }

        void StopTasks()
        {
            mRunning = false;
            mCondition.notify_all();
        }

        void WaitTasks()
        {
            if (!tasks.empty())
            {
                mCondition.notify_all();
                for (auto &worker : workers)
                {
                    if (worker.joinable())
                        worker.join();
                }
            }

            mRunning = false;
            mCondition.notify_all();
            for (auto &worker : workers)
            {
                if (worker.joinable())
                    worker.join();
            }
        }

      private:
        void workerLoop()
        {
            while (mRunning)
            {
                std::move_only_function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(mMutex);
                    mCondition.wait(lock, [this] { return mRunning || !tasks.empty(); });

                    if (!mRunning || tasks.empty()) return;

                    task = std::move(tasks.front());
                    tasks.pop();

                }
                task();
            }
        }

        std::vector<std::thread> workers;
        std::queue<std::move_only_function<void()>> tasks;
        std::mutex mMutex;
        std::condition_variable mCondition;

        bool mRunning;
        int mThreadCount;
    };
} // namespace FREYR_NAMESPACE