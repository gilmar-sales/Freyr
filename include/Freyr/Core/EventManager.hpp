#pragma once

#include <any>

#include "Freyr/Base/Event.hpp"

namespace FREYR_NAMESPACE
{
    class EventManager
    {
      public:
        template <typename T>
            requires IsEvent<T>
        void AddListener(std::function<void(T)> const& listener)
        {
            mListeners[GetEventId<T>()].push_back(listener);
        }

        template <typename T>
            requires IsEvent<T>
        void SendEvent(T event)
        {
            auto eventId = GetEventId<T>();

            for (std::any const& listener : mListeners[eventId])
            {
                auto listenerPtr = std::any_cast<std::function<void(T)>>(listener);

                if (listenerPtr)
                    listenerPtr(event);
            }
        }

      private:
        std::unordered_map<EventId, std::vector<std::any>> mListeners;
    };

} // namespace FREYR_NAMESPACE
