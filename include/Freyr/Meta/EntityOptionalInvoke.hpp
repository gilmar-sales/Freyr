#pragma once

#include "Freyr/Base/Entity.hpp"

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

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

        template <typename F, typename... Ts>
        auto invoke_with_optional_entity_result(F&& callback, Entity entity, Ts&... components)
        {
            if constexpr (callback_takes_entity_v<F, Ts...>)
                return callback(entity, components...);
            else
                return callback(components...);
        }

        template <typename F, typename... Ts, std::size_t... Is>
        void invoke_at_component_pointers_impl(F&&                       callback,
                                               Entity                    entity,
                                               std::size_t               index,
                                               const std::tuple<Ts*...>& componentPtrs,
                                               std::index_sequence<Is...>)
        {
            if constexpr (callback_takes_entity_v<F, Ts...>)
                callback(entity, std::get<Is>(componentPtrs)[index]...);
            else
                callback(std::get<Is>(componentPtrs)[index]...);
        }

        template <typename F, typename... Ts>
        void invoke_at_component_pointers(
            F&& callback, Entity entity, std::size_t index, const std::tuple<Ts*...>& componentPtrs)
        {
            invoke_at_component_pointers_impl<F, Ts...>(
                std::forward<F>(callback),
                entity,
                index,
                componentPtrs,
                std::index_sequence_for<Ts...> {});
        }

    } // namespace meta
} // namespace FREYR_NAMESPACE
