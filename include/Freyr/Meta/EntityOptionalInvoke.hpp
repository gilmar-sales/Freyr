#pragma once

#include "Freyr/Base/Entity.hpp"

#include <type_traits>

namespace FREYR_NAMESPACE
{
    namespace meta
    {

        template <typename F, typename... Ts>
        constexpr bool callback_takes_entity_v =
            std::is_invocable_v<std::decay_t<F>, Entity, Ts&...>;

        template <typename F, typename... Ts>
        constexpr bool callback_invocable_v =
            callback_takes_entity_v<F, Ts...> || std::is_invocable_v<std::decay_t<F>, Ts&...>;

        template <typename F, typename... Ts>
        void invoke_with_optional_entity(F&& callback, Entity entity, Ts&... components)
        {
            if constexpr (callback_takes_entity_v<F, Ts...>)
                callback(entity, components...);
            else
                callback(components...);
        }

    } // namespace meta
} // namespace FREYR_NAMESPACE
