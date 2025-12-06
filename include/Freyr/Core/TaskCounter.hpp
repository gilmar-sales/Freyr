#include <atomic>
#include <thread>

namespace FREYR_NAMESPACE
{
    class TaskCounter
    {

      public:
        TaskCounter() : remaining_tasks(0) {};

        void addTasks(size_t count) { remaining_tasks.fetch_add(count, std::memory_order_release); }

        void taskCompleted()
        {
            int old_value = remaining_tasks.fetch_sub(1, std::memory_order_acq_rel);
            if (old_value == 1)
            {
                // Last task completed, notify waiters
                remaining_tasks.notify_all();
            }
        }

        void waitForCompletion()
        {
            int current = remaining_tasks.load(std::memory_order_acquire);
            while (current > 0)
            {
                remaining_tasks.wait(current, std::memory_order_acquire);
                current = remaining_tasks.load(std::memory_order_acquire);
            }
        }

        size_t getRemainingTasks() const { return remaining_tasks.load(std::memory_order_acquire); }

      private:
        std::atomic<size_t> remaining_tasks;
    };
} // namespace FREYR_NAMESPACE