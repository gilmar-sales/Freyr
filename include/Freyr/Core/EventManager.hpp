#pragma once

#include "Freyr/Base/Event.hpp"
#include "Freyr/Core/RwLock.hpp"

namespace FREYR_NAMESPACE
{
    template <typename TEvent>
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
    };

    template <typename TEvent>
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
      private:
        mutable RwLock<>                         mLock;
        std::vector<std::unique_ptr<IPublisher>> mPublishers;
        std::unordered_map<size_t, size_t>       mEventIdToIndex;

      public:
        EventManager() { mPublishers.reserve(256); }

        ~EventManager() = default;

        template <typename T>
            requires IsEvent<T>
        [[nodiscard]] Ref<ListenerHandle> Subscribe(auto&& listener)
        {
            return GetPublisher<T>()->Subscribe(std::forward<decltype(listener)>(listener));
        }

        template <typename T>
            requires IsEvent<T>
        void Send(const T& event)
        {
            GetPublisher<T>()->Publish(event);
        }

        template <typename T>
            requires IsEvent<T>
        void Send(T&& event)
        {
            GetPublisher<T>()->Publish(std::forward<T>(event));
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
        Publisher<T>* GetPublisher()
        {
            const size_t eventId = GetEventId<T>();

            {
                auto read = mLock.read();
                auto it   = mEventIdToIndex.find(eventId);
                if (it != mEventIdToIndex.end())
                {
                    return static_cast<Publisher<T>*>(mPublishers[it->second].get());
                }
            }

            auto write = mLock.write();

            auto it = mEventIdToIndex.find(eventId);
            if (it != mEventIdToIndex.end())
            {
                return static_cast<Publisher<T>*>(mPublishers[it->second].get());
            }

            size_t index = mPublishers.size();
            mPublishers.push_back(std::make_unique<Publisher<T>>());
            mEventIdToIndex[eventId] = index;

            return static_cast<Publisher<T>*>(mPublishers[index].get());
        }
    };

} // namespace FREYR_NAMESPACE