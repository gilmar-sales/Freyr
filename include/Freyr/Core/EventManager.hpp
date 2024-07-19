#pragma once

#include <any>

#include "Freyr/Base/Event.hpp"
#include <boost/container/flat_map.hpp>

namespace FREYR_NAMESPACE
{
    class IPublisher
    {
      public:
        virtual ~IPublisher() = default;
    };

    template <typename TEvent>
    class Publisher : public IPublisher
    {
      public:
        void Subscribe(auto&& listener)
        {
            mListeners.emplace_back(std::move(listener));
        }

        void Publish(TEvent event)
        {
            for (auto i = 0; i < mListeners.size(); i++)
            {
                mListeners[i](event);
            }
        }

      private:
        std::vector<std::move_only_function<void(TEvent)>> mListeners;
    };

    class EventManager
    {
      public:
        template <typename T>
            requires IsEvent<T>
        void Subscribe(auto&& listener)
        {
            GetPublisher<T>()->Subscribe(listener);
        }

        template <typename T>
            requires IsEvent<T>
        void Send(T event)
        {
            GetPublisher<T>()->Publish(event);
        }

      private:
        template <typename T>
            requires IsEvent<T>
        Publisher<T>* GetPublisher()
        {
            if (!mPublishers.contains(GetEventId<T>()))
            {
                mPublishers[GetEventId<T>()] = new Publisher<T>();
            }

            return (Publisher<T>*) mPublishers[GetEventId<T>()];
        }

        boost::container::flat_map<EventId, IPublisher*> mPublishers;
    };

} // namespace FREYR_NAMESPACE
