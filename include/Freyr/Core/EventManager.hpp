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
        struct Listener
        {
            fr::function<void(const TEvent&)> callback;
            skr::WeakArc<ListenerHandle>           handle;

            Listener(fr::function<void(const TEvent&)>&& cb, skr::Arc<ListenerHandle> handle) :
                callback(std::move(cb)), handle(handle)
            {
            }
        };

        alignas(64) mutable RwLock mLock;
        std::vector<Listener> mListeners;
        std::atomic<size_t>   mNextId { 1 };
        std::atomic<bool>     mNeedsCleanup { false };

        alignas(64) mutable RwLock mPendingLock;
        std::vector<Listener> mPendingListeners;

      public:
        ~Publisher() override = default;

        /**
         * @brief Subscribes a listener to this event type.
         *
         * @param listener  Callback function invoked when the event is published
         * @return ListenerHandle to manage subscription lifetime
         *
         * @note Listeners with expired handles are automatically cleaned up during Flush().
         */
        [[nodiscard]] skr::Arc<ListenerHandle> Subscribe(auto&& listener)
        {
            const size_t id = mNextId.fetch_add(1, std::memory_order_relaxed);

            auto handle = skr::MakeArc<ListenerHandle>(id);
            {
                auto write = mPendingLock.write();
                mPendingListeners.emplace_back(
                    fr::function<void(const TEvent&)>(std::forward<decltype(listener)>(listener)),
                    handle);
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
            auto read = mLock.read();

            for (Listener& listener : mListeners)
            {
                if (!listener.handle.expired()) [[likely]]
                {
                    listener.callback(event);
                    continue;
                }

                mNeedsCleanup.exchange(true, std::memory_order::release);
            }
        }

        // Overload for rvalue events
        void Publish(TEvent&& event) { Publish(static_cast<const TEvent&>(event)); }

        void ClearInactiveListeners() override
        {
            auto write = mLock.write();

            mListeners.erase(std::remove_if(mListeners.begin(),
                                            mListeners.end(),
                                            [](const Listener& l) { return l.handle.expired(); }),
                             mListeners.end());

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
            return std::count_if(mListeners.begin(), mListeners.end(), [](const Listener& l) {
                return !l.handle.expired();
            });
        }

        EventId GetEventId() const override { return fr::GetEventId<TEvent>(); }

        void MergePendingListeners() override
        {
            std::vector<Listener> toMerge;

            {
                auto write = mPendingLock.write();

                if (mPendingListeners.empty()) [[likely]]
                    return;

                toMerge = std::move(mPendingListeners);
                mPendingListeners.clear();
            }

            auto write = mLock.write();
            mListeners.reserve(mListeners.size() + toMerge.size());
            mListeners.insert(mListeners.end(),
                              std::make_move_iterator(toMerge.begin()),
                              std::make_move_iterator(toMerge.end()));
        }
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
        [[nodiscard]] skr::Arc<ListenerHandle> Subscribe(auto&& listener)
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

        alignas(64) mutable RwLock mLock;
        std::map<EventId, IPublisher*> mPublishers;
    };

} // namespace FREYR_NAMESPACE