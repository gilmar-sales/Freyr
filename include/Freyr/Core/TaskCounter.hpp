#include <atomic>
#include <thread>

namespace FREYR_NAMESPACE
{
    class TaskCounter
    {

      public:
        TaskCounter() : remaining_tasks(0) {};

        void AddTasks(const size_t count) { remaining_tasks.fetch_add(count, std::memory_order_release); }

        void TaskCompleted()
        {
            if (const int oldValue = remaining_tasks.fetch_sub(1, std::memory_order_acq_rel); oldValue == 1)
            {
                remaining_tasks.notify_all();
            }
        }

        void WaiForCompletion() const
        {
            int current = remaining_tasks.load(std::memory_order_acquire);
            while (current > 0)
            {
                remaining_tasks.wait(current, std::memory_order_acquire);
                current = remaining_tasks.load(std::memory_order_acquire);
            }
        }

        size_t GetRemainingTasks() const { return remaining_tasks.load(std::memory_order_acquire); }

      private:
        alignas(64) std::atomic<std::int64_t> remaining_tasks;
    };
} // namespace FREYR_NAMESPACE