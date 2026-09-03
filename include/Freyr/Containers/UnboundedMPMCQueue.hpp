#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <mutex>
#include <new>
#include <type_traits>
#include <utility>
#include <vector>

namespace rigtorp
{
    namespace mpmc
    {
        template <typename T, std::size_t SegmentCapacity = 1024>
        class UnboundedQueue
        {
            static_assert(SegmentCapacity > 1);

            static constexpr std::size_t CacheLineSize = 64;

            struct alignas(CacheLineSize) Slot
            {
                alignas(CacheLineSize) std::atomic<std::uint64_t> sequence { 0 };
                alignas(T) std::byte storage[sizeof(T)];

                template <typename... Args>
                void construct(Args&&... args) noexcept
                {
                    static_assert(std::is_nothrow_constructible_v<T, Args&&...>);
                    new (&storage) T(std::forward<Args>(args)...);
                }

                void destroy() noexcept
                {
                    static_assert(std::is_nothrow_destructible_v<T>);
                    reinterpret_cast<T*>(&storage)->~T();
                }

                T&& move() noexcept { return reinterpret_cast<T&&>(storage); }
            };

            struct Segment
            {
                explicit Segment(const std::uint64_t first) : first(first)
                {
                    for (std::size_t i = 0; i < SegmentCapacity; ++i)
                    {
                        new (&slots[i]) Slot;
                        slots[i].sequence.store(first + i, std::memory_order_relaxed);
                    }
                }

                ~Segment() noexcept
                {
                    for (std::size_t i = 0; i < SegmentCapacity; ++i)
                    {
                        const auto sequence = slots[i].sequence.load(std::memory_order_relaxed);
                        if (sequence == first + i + 1)
                            slots[i].destroy();
                        slots[i].~Slot();
                    }
                }

                const std::uint64_t first;
                alignas(CacheLineSize) std::atomic<Segment*> next { nullptr };
                Slot slots[SegmentCapacity];
            };

          public:
            UnboundedQueue() : mProducerSegment(new Segment(0)), mConsumerSegment(mProducerSegment.load())
            {
                mSegments.push_back(std::unique_ptr<Segment>(mProducerSegment.load()));
            }

            ~UnboundedQueue() noexcept
            = default;

            UnboundedQueue(const UnboundedQueue&)            = delete;
            UnboundedQueue& operator=(const UnboundedQueue&) = delete;

            template <typename... Args>
            void emplace(Args&&... args)
            {
                static_assert(std::is_nothrow_constructible_v<T, Args&&...>);
                const auto reservation = reserveProducer();
                auto&      slot        = reservation.segment->slots[reservation.position - reservation.segment->first];
                while (slot.sequence.load(std::memory_order_acquire) != reservation.position)
                    ;
                slot.construct(std::forward<Args>(args)...);
                slot.sequence.store(reservation.position + 1, std::memory_order_release);
            }

            template <typename... Args>
            bool try_emplace(Args&&... args) noexcept
            {
                static_assert(std::is_nothrow_constructible_v<T, Args&&...>);
                std::uint64_t position;
                Segment*      segment;
                try
                {
                    const auto reservation = reserveProducer();
                    position               = reservation.position;
                    segment                = reservation.segment;
                }
                catch (const std::exception&)
                {
                    return false;
                }

                auto& slot    = segment->slots[position - segment->first];
                while (slot.sequence.load(std::memory_order_acquire) != position)
                    ;
                slot.construct(std::forward<Args>(args)...);
                slot.sequence.store(position + 1, std::memory_order_release);
                return true;
            }

            void push(const T& value)
            {
                static_assert(std::is_nothrow_copy_constructible_v<T>);
                emplace(value);
            }

            template <typename P>
                requires std::is_nothrow_constructible_v<T, P&&>
            void push(P&& value)
            {
                emplace(std::forward<P>(value));
            }

            bool try_push(const T& value) noexcept
            {
                static_assert(std::is_nothrow_copy_constructible_v<T>);
                return try_emplace(value);
            }

            template <typename P>
                requires std::is_nothrow_constructible_v<T, P&&>
            bool try_push(P&& value) noexcept
            {
                return try_emplace(std::forward<P>(value));
            }

            void pop(T& value) noexcept
            {
                const auto reservation = reserveConsumer();
                auto&      slot        = reservation.segment->slots[reservation.position - reservation.segment->first];
                while (slot.sequence.load(std::memory_order_acquire) != reservation.position + 1)
                    ;
                value = slot.move();
                slot.destroy();
                slot.sequence.store(reservation.position + SegmentCapacity, std::memory_order_release);
            }

            bool try_pop(T& value) noexcept
            {
                auto position = mTail.load(std::memory_order_acquire);
                for (;;)
                {
                    auto* segment = segmentForConsumer(position);
                    if (segment == nullptr)
                        return false;
                    if (position < segment->first)
                    {
                        position = mTail.load(std::memory_order_acquire);
                        continue;
                    }

                    auto& slot     = segment->slots[position - segment->first];
                    const auto seq = slot.sequence.load(std::memory_order_acquire);
                    if (seq != position + 1)
                    {
                        if (mTail.load(std::memory_order_acquire) == position)
                            return false;
                        position = mTail.load(std::memory_order_acquire);
                        continue;
                    }

                    if (mTail.compare_exchange_weak(position, position + 1, std::memory_order_relaxed))
                    {
                        value = slot.move();
                        slot.destroy();
                        slot.sequence.store(position + SegmentCapacity, std::memory_order_release);
                        return true;
                    }
                }
            }

            [[nodiscard]] std::ptrdiff_t size() const noexcept
            {
                return static_cast<std::ptrdiff_t>(
                    mHead.load(std::memory_order_relaxed) - mTail.load(std::memory_order_relaxed));
            }

            [[nodiscard]] bool empty() const noexcept { return size() <= 0; }

          private:
            struct Reservation
            {
                std::uint64_t position;
                Segment*       segment;
            };

            Reservation reserveProducer()
            {
                auto position = mHead.load(std::memory_order_relaxed);
                for (;;)
                {
                    auto* segment = mProducerSegment.load(std::memory_order_acquire);
                    if (position < segment->first)
                    {
                        position = mHead.load(std::memory_order_relaxed);
                        continue;
                    }
                    if (position >= segment->first + SegmentCapacity)
                    {
                        ensureNext(segment);
                        mProducerSegment.compare_exchange_weak(
                            segment, segment->next.load(std::memory_order_acquire), std::memory_order_release);
                        position = mHead.load(std::memory_order_relaxed);
                        continue;
                    }
                    if (mHead.compare_exchange_weak(position, position + 1, std::memory_order_relaxed))
                        return { position, segment };
                }
            }

            Reservation reserveConsumer()
            {
                auto position = mTail.load(std::memory_order_relaxed);
                for (;;)
                {
                    auto* segment = mConsumerSegment.load(std::memory_order_acquire);
                    if (position < segment->first)
                    {
                        position = mTail.load(std::memory_order_relaxed);
                        continue;
                    }
                    if (position >= segment->first + SegmentCapacity)
                    {
                        auto* next = segment->next.load(std::memory_order_acquire);
                        while (next == nullptr)
                            next = segment->next.load(std::memory_order_acquire);
                        mConsumerSegment.compare_exchange_weak(segment, next, std::memory_order_release);
                        position = mTail.load(std::memory_order_relaxed);
                        continue;
                    }
                    if (mTail.compare_exchange_weak(position, position + 1, std::memory_order_relaxed))
                        return { position, segment };
                }
            }

            Segment* segmentForConsumer(const std::uint64_t position) const noexcept
            {
                auto* segment = mConsumerSegment.load(std::memory_order_acquire);
                while (position >= segment->first + SegmentCapacity)
                {
                    segment = segment->next.load(std::memory_order_acquire);
                    if (segment == nullptr)
                        return nullptr;
                }
                return segment;
            }

            void ensureNext(Segment* segment)
            {
                if (segment->next.load(std::memory_order_acquire) != nullptr)
                    return;

                std::lock_guard lock(mSegmentMutex);
                if (segment->next.load(std::memory_order_relaxed) == nullptr)
                {
                    auto next = std::make_unique<Segment>(segment->first + SegmentCapacity);
                    auto* raw = next.get();
                    mSegments.push_back(std::move(next));
                    segment->next.store(raw, std::memory_order_release);
                }
            }

            alignas(CacheLineSize) std::atomic<std::uint64_t> mHead { 0 };
            alignas(CacheLineSize) std::atomic<std::uint64_t> mTail { 0 };
            std::atomic<Segment*> mProducerSegment;
            std::atomic<Segment*> mConsumerSegment;
            std::mutex mSegmentMutex;
            std::vector<std::unique_ptr<Segment>> mSegments;
        };
    } // namespace mpmc

    template <typename T, std::size_t SegmentCapacity = 1024>
    using UnboundedMPMCQueue = mpmc::UnboundedQueue<T, SegmentCapacity>;
} // namespace rigtorp
