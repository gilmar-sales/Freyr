#pragma once

#include "Freyr/Base/Event.hpp"
#include "Freyr/Core/RwLock.hpp"

namespace FREYR_NAMESPACE
{
    template <typename TEvent>
        requires IsEvent<TEvent>
    class Publisher;

    struct ListenerHandle
    {
        size_t id { 0 };
        bool   IsValid() const { return id != 0; }
    };

    class IPublisher
    {
      public:
        virtual ~IPublisher()                 = default;
        virtual void ClearInactiveListeners() = 0;

        operator size_t() const { return GetEventId(); }

        virtual EventId GetEventId() const = 0;
    };

    template <typename TEvent>
        requires IsEvent<TEvent>
    class Publisher final : public IPublisher
    {
      private:
        struct Listener
        {
            fr::function<void(const TEvent&)> callback;
            std::weak_ptr<ListenerHandle>     handle;

            Listener(fr::function<void(const TEvent&)>&& cb, Ref<ListenerHandle> handle) :
                callback(std::move(cb)), handle(handle)
            {
            }
        };

        mutable RwLock<>      mLock;
        std::vector<Listener> mListeners;
        std::atomic<size_t>   mNextId { 1 };
        std::atomic<bool>     mNeedsCleanup { false };

        std::mutex            mPendingMutex;
        std::vector<Listener> mPendingListeners;

      public:
        ~Publisher() override = default;

        [[nodiscard]] Ref<ListenerHandle> Subscribe(auto&& listener)
        {
            const size_t id = mNextId.fetch_add(1, std::memory_order_relaxed);

            auto handle = skr::MakeRef<ListenerHandle>(id);
            {
                std::lock_guard lock(mPendingMutex);
                mPendingListeners.emplace_back(
                    fr::function<void(const TEvent&)>(std::forward<decltype(listener)>(listener)),
                    handle);
            }

            return handle;
        }

        void Publish(const TEvent& event)
        {
            MergePendingListeners();

            {
                auto read = mLock.read();

                for (Listener& listener : mListeners)
                {
                    if (!listener.handle.expired())
                    {
                        listener.callback(event);
                    }
                }
            }

            if (mNeedsCleanup.load(std::memory_order_acquire))
            {
                ClearInactiveListeners();
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

        size_t ListenerCount() const
        {
            auto read = mLock.read();
            return std::count_if(mListeners.begin(), mListeners.end(), [](const Listener& l) {
                return !l.handle.expired();
            });
        }

        EventId GetEventId() const override { return fr::GetEventId<TEvent>(); }

      private:
        void MergePendingListeners()
        {
            std::vector<Listener> toMerge;

            {
                std::lock_guard lock(mPendingMutex);
                if (mPendingListeners.empty())
                    return;
                toMerge = std::move(mPendingListeners);
                mPendingListeners.clear();
            }

            if (!toMerge.empty())
            {
                auto write = mLock.write();
                mListeners.reserve(mListeners.size() + toMerge.size());
                mListeners.insert(mListeners.end(),
                                  std::make_move_iterator(toMerge.begin()),
                                  std::make_move_iterator(toMerge.end()));
            }
        }
    };

    class EventManager
    {

      public:
        EventManager() { mPublishers.resize(256); }

        ~EventManager()
        {
            for (auto publisher : mPublishers)
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

        void Cleanup()
        {
            auto read = mLock.read();
            for (auto& publisher : mPublishers)
            {
                if (publisher)
                {
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

            {
                auto read = mLock.read();
                if (mPublishers.contains(eventId))
                {
                    return static_cast<Publisher<T>*>(mPublishers[eventId]);
                }
            }

            auto write = mLock.write();

            mPublishers.insert(new Publisher<T>());

            return static_cast<Publisher<T>*>(mPublishers[eventId]);
        }

      private:
        mutable RwLock<>       mLock;
        SparseSet<IPublisher*> mPublishers;
    };

} // namespace FREYR_NAMESPACE