#pragma once

#include "Freyr/Core/Processor.hpp"

namespace FREYR_NAMESPACE
{

    constexpr uint32_t kWriterFlag = 1u << 31;
    constexpr uint32_t kReaderMask = ~kWriterFlag;

    class RwLock
    {
      public:
        RwLock() : state_(0) {}

        RwLock(const RwLock&)            = delete;
        RwLock& operator=(const RwLock&) = delete;
        struct ReadGuard
        {
            explicit ReadGuard(RwLock& l) : lock_(l) { lock_.lock_shared(); }
            ~ReadGuard() { lock_.unlock_shared(); }
            ReadGuard(const ReadGuard&)            = delete;
            ReadGuard& operator=(const ReadGuard&) = delete;

          private:
            RwLock& lock_;
        };

        struct WriteGuard
        {
            explicit WriteGuard(RwLock& l) : lock_(l) { lock_.lock(); }
            ~WriteGuard() { lock_.unlock(); }
            WriteGuard(const WriteGuard&)            = delete;
            WriteGuard& operator=(const WriteGuard&) = delete;

          private:
            RwLock& lock_;
        };

        ReadGuard  read() { return ReadGuard(*this); }
        WriteGuard write() { return WriteGuard(*this); }

      private:
        void lock_shared()
        {
            while (true)
            {
                uint32_t s = state_.load(std::memory_order_relaxed);

                if (s & kWriterFlag)
                {
                    Processor::Pause();
                    continue;
                }

                if (state_.compare_exchange_weak(
                        s, s + 1, std::memory_order_acquire, std::memory_order_relaxed))
                {
                    return;
                }
            }
        }

        void unlock_shared() { state_.fetch_sub(1, std::memory_order_release); }

        void lock()
        {
            while (true)
            {
                uint32_t s = state_.load(std::memory_order_relaxed);

                if (s & kWriterFlag)
                {
                    Processor::Pause();
                    continue;
                }

                if (state_.compare_exchange_weak(
                        s, s | kWriterFlag, std::memory_order_acquire, std::memory_order_relaxed))
                {
                    break;
                }
            }

            while (state_.load(std::memory_order_acquire) & kReaderMask)
            {
                Processor::Pause();
            }
        }

        void unlock() { state_.fetch_and(~kWriterFlag, std::memory_order_release); }

        alignas(64) std::atomic<uint32_t> state_;
    };

} // namespace FREYR_NAMESPACE