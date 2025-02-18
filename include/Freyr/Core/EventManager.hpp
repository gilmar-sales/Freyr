#pragma once

#include "Freyr/Base/Event.hpp"

namespace FREYR_NAMESPACE
{
    class IPublisher
    {
      public:
        virtual ~IPublisher() = default;
    };

    template <typename TEvent>
    class Publisher final : public IPublisher
    {
      public:
        ~Publisher() override = default;

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
        ~EventManager()
        {
            for (const auto publisher : mPublishers)
            {
                delete publisher;
            }
        }

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
            if (GetEventId<T>() + 1 > mPublishers.size())
                mPublishers.resize(GetEventId<T>() + 16);

            if (!mPublishers[GetEventId<T>()])
            {
                mPublishers[GetEventId<T>()] = new Publisher<T>();
            }

            return static_cast<Publisher<T>*>(mPublishers[GetEventId<T>()]);
        }

        std::vector<IPublisher*> mPublishers;
    };

} // namespace FREYR_NAMESPACE
