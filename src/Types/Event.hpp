#pragma once

#include <type_traits>

namespace FREYR_NAMESPACE
{
    using EventId = std::uint64_t;

    inline EventId EventCount = 0;

    struct Event
    {
    };

    template <typename T>
    concept IsEvent = std::is_base_of_v<Event, T>;

    template <typename T>
        requires IsEvent<T>
    constexpr auto GetEventId() -> EventId
    {
        const static auto id = EventCount++;

        return id;
    }
    /*
        constexpr std::uint32_t fnv1a_32(char const *s, std::uint64_t count)
        {
            return ((count ? fnv1a_32(s, count - 1) : 2166136261u) ^ s[count]) * 16777619u; // NOLINT (hicpp-signed-bitwise)
        }

        constexpr std::uint32_t operator"" _hash(char const *s, std::uint64_t count) { return fnv1a_32(s, count); }

        // Input
        enum class InputButtons
        {
            W,
            A,
            S,
            D,
            Q,
            E
        };

        // Events
        using EventId = std::uint32_t;
        using ParamId = std::uint32_t;

    #define METHOD_LISTENER(EventType, Listener) EventType, std::bind(&Listener, this, std::placeholders::_1)
    #define FUNCTION_LISTENER(EventType, Listener) EventType, std::bind(&Listener, std::placeholders::_1)

        namespace Events::Window
        {
            const EventId QUIT    = "Events::Window::QUIT"_hash;
            const EventId RESIZED = "Events::Window::RESIZED"_hash;
            const EventId INPUT   = "Events::Window::INPUT"_hash;
        } // namespace Events::Window

        namespace Events::Physics
        {
            const EventId Collision = "Events::Physics::Collision"_hash;
        }

        namespace Events::Physics::Params
        {
            const ParamId Collisor = "Events::Physics::Params::Collisor"_hash;
            const ParamId Target   = "Events::Physics::Params::Target"_hash;
        } // namespace Events::Physics::Params

        namespace Events::Window::Input
        {
            const ParamId INPUT = "Events::Window::Input::INPUT"_hash;
        }

        namespace Events::Window::Resized
        {
            const ParamId WIDTH  = "Events::Window::Resized::WIDTH"_hash;
            const ParamId HEIGHT = "Events::Window::Resized::HEIGHT"_hash;
        } // namespace Events::Window::Resized

        class Event
        {
          public:
            Event() = delete;

            explicit Event(EventId type) : mType(type) {}

            template<typename T>
            void SetParam(ParamId id, T value)
            {
                mData[id] = value;
            }

            template<typename T>
            T GetParam(ParamId id)
            {
                return std::any_cast<T>(mData[id]);
            }

            EventId GetType() const { return mType; }

          private:
            EventId mType{};
            std::unordered_map<ParamId, std::any> mData{};
        };
    */
} // namespace FREYR_NAMESPACE
