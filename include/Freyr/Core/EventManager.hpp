#pragma once

#include "Freyr/Base/Event.hpp"
#include "Freyr/Core/RwLock.hpp"

namespace FREYR_NAMESPACE
{
    template <typename TEvent>
        requires IsEvent<TEvent>
    class Publisher;

    /**
     * @brief Handle for managing listener lifetime.
     *
     * A valid handle indicates the listener is still active.
     */
    struct ListenerHandle
    {
        size_t id { 0 };
        bool   IsValid() const { return id != 0; }
    };

    /**
     * @brief Abstract base class for event publishers.
     */
    class IPublisher
    {
      public:
        virtual ~IPublisher()                 = default;
        virtual void ClearInactiveListeners() = 0;
        virtual void MergePendingListeners()  = 0;

        operator size_t() const { return GetEventId(); }

        virtual EventId GetEventId() const = 0;
    };

    template <typename TEvent>
        requires IsEvent<TEvent>
    class Publisher final : public IPublisher
    {
      public:
        struct Listener
        {
            fr::function<void(const TEvent&)> callback;
            WeakRef<ListenerHandle>           handle;
        };

        Publisher() = default;

        ~Publisher() override = default;

        [[nodiscard]] Ref<ListenerHandle> Subscribe(auto&& listener)
        {
            const size_t id = mNextId.fetch_add(1, std::memory_order_relaxed);

            auto handle = skr::MakeRef<ListenerHandle>(id);
            {
                auto write = mPendingLock.write();
                mPendingCallbacks.emplace_back(fr::function<void(const TEvent&)>(std::forward<decltype(listener)>(listener)));
                mPendingHandles.emplace_back(handle);
            }

            return handle;
        }

        /**
         * @brief Dispatches an event to all active listeners.
         *
         * @param event  Event to publish; each listener receives a const reference
         *
         * @note Expired listeners are marked for cleanup; actual removal happens in ClearInactiveListeners().
         */
        void Publish(const TEvent& event)
        {
            bool needsCleanup = false;
            for (size_t i = 0; i < mCallbackCount; ++i)
            {
                if (!mHandles[i].expired()) [[likely]]
                {
                    mCallbacks[i](event);
                }
                else
                {
                    needsCleanup = true;
                }
            }
            if (needsCleanup)
            {
                mNeedsCleanup.store(true, std::memory_order_release);
            }
        }

        void Publish(TEvent&& event) { Publish(static_cast<const TEvent&>(event)); }

        void ClearInactiveListeners() override
        {
            auto write = mLock.write();

            size_t dst = 0;
            for (size_t src = 0; src < mCallbackCount; ++src)
            {
                if (!mHandles[src].expired())
                {
                    if (dst != src)
                    {
                        mCallbacks[dst] = std::move(mCallbacks[src]);
                        mHandles[dst]   = std::move(mHandles[src]);
                    }
                    ++dst;
                }
            }

            mCallbackCount = dst;
            mNeedsCleanup.store(false, std::memory_order_release);
        }

        /**
         * @brief Returns the number of active (non-expired) listeners.
         *
         * @return Count of listeners with valid handles
         */
        size_t ListenerCount() const
        {
            auto read = mLock.read();
            return std::count_if(mHandles.begin(), mHandles.begin() + mCallbackCount, [](const auto& h) {
                return !h.expired();
            });
        }

        EventId GetEventId() const override { return fr::GetEventId<TEvent>(); }

        void MergePendingListeners() override
        {
            {
                if (mPendingCallbacks.empty()) [[likely]]
                    return;
            }

            size_t pendingCount = mPendingCallbacks.size();

            {
                auto write = mPendingLock.write();
                mPendingLockCount = pendingCount;
                std::swap(mPendingCallbacks, mCallbacks);
                std::swap(mPendingHandles, mHandles);
            }

            mCallbackCount += mPendingLockCount;
            mPendingLockCount = 0;

            mPendingCallbacks.clear();
            mPendingHandles.clear();
        }

      private:
        mutable RwLock mLock;

        std::vector<fr::function<void(const TEvent&)>> mCallbacks;
        std::vector<WeakRef<ListenerHandle>>           mHandles;
        size_t                                         mCallbackCount = 0;

        std::atomic<size_t>  mNextId { 1 };
        std::atomic<bool>    mNeedsCleanup { false };

        mutable RwLock mPendingLock;
        std::vector<fr::function<void(const TEvent&)>> mPendingCallbacks;
        std::vector<WeakRef<ListenerHandle>>           mPendingHandles;
        size_t                                         mPendingLockCount = 0;
    };

    class EventManager
    {

      public:
        EventManager() = default;

        /**
         * @brief Destructor that cleans up all publishers.
         */
        ~EventManager()
        {
            for (auto [_, publisher] : mPublishers)
            {
                delete publisher;
            }
        };

        template <typename T>
            requires IsEvent<T>
        [[nodiscard]] Ref<ListenerHandle> Subscribe(auto&& listener)
        {
            return GetOrCreatePublisher<T>()->Subscribe(std::forward<decltype(listener)>(listener));
        }

        /**
         * @brief Sends an event to all registered listeners.
         *
         * @tparam T    Event type (must satisfy IsEvent)
         * @param event Event to dispatch; listeners receive a const reference
         */
        template <typename T>
            requires IsEvent<T>
        void Send(const T& event)
        {
            GetOrCreatePublisher<T>()->Publish(event);
        }

        template <typename T>
            requires IsEvent<T>
        void Send(T&& event)
        {
            GetOrCreatePublisher<T>()->Publish(std::forward<T>(event));
        }

        /**
         * @brief Merges pending listeners and removes expired handles across all publishers.
         *
         * @note Call this at the end of each frame to ensure clean listener state.
         */
        void Flush()
        {
            auto read = mLock.read();
            for (auto [_, publisher] : mPublishers)
            {
                if (publisher)
                {
                    publisher->MergePendingListeners();
                    publisher->ClearInactiveListeners();
                }
            }
        }

      private:
        template <typename T>
            requires IsEvent<T>
        Publisher<T>* GetOrCreatePublisher()
        {
            const size_t eventId = GetEventId<T>();

            if (auto it = mPublishers.find(eventId); it != mPublishers.end()) [[likely]]
            {
                return static_cast<Publisher<T>*>(it->second);
            }

            auto write = mLock.write();

            if (auto it = mPublishers.find(eventId); it != mPublishers.end()) [[unlikely]]
            {
                return static_cast<Publisher<T>*>(it->second);
            }

            mPublishers.insert({ eventId, new Publisher<T>() });

            return static_cast<Publisher<T>*>(mPublishers[eventId]);
        }

      private:
        mutable RwLock               mLock;
        std::map<EventId, IPublisher*> mPublishers;
    };

} // namespace FREYR_NAMESPACE