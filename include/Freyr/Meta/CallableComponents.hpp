#pragma once

#include "Freyr/Base/Component.hpp"
#include "Freyr/Base/Entity.hpp"

#include <meta>
#include <tuple>
#include <type_traits>
#include <vector>

namespace FREYR_NAMESPACE
{
    namespace meta
    {
        namespace detail
        {
            template <typename F>
            consteval auto FindCallOperator() -> std::meta::info
            {
                const auto cls = ^^std::remove_cvref_t<F>;
                for (const auto member :
                     std::meta::members_of(cls, std::meta::access_context::unchecked()))
                {
                    const bool isCallOp =
                        (std::meta::is_operator_function(member) ||
                         std::meta::is_operator_function_template(member)) &&
                        std::meta::operator_of(member) == std::meta::operators::op_parentheses;
                    if (isCallOp)
                        return member;
                }

                throw std::meta::exception("callable has no call operator", cls);
            }

            template <typename F>
            consteval auto ConcreteCallOperator() -> std::meta::info
            {
                const auto callOp = FindCallOperator<F>();
                if (std::meta::is_operator_function_template(callOp) ||
                    std::meta::is_function_template(callOp))
                {
                    throw std::meta::exception(
                        "callable parameters must be concrete types; use Entity (not auto) and "
                        "concrete component references",
                        callOp);
                }

                return callOp;
            }

            consteval auto IsComponentType(std::meta::info type) -> bool
            {
                return std::meta::is_type(type) && std::meta::is_complete_type(type) &&
                       std::meta::is_base_of_type(^^Component, type);
            }

            template <typename F>
            consteval auto ComponentsTupleInfo() -> std::meta::info
            {
                const auto callOp = ConcreteCallOperator<F>();
                const auto params = std::meta::parameters_of(callOp);

                std::vector<std::meta::info> components;
                bool                         skippedEntity = false;

                for (const auto param : params)
                {
                    const auto type = std::meta::remove_cvref(std::meta::type_of(param));

                    if (IsComponentType(type))
                    {
                        components.push_back(type);
                        continue;
                    }

                    if (!skippedEntity && components.empty() &&
                        std::meta::is_same_type(type, ^^Entity))
                    {
                        skippedEntity = true;
                        continue;
                    }

                    throw std::meta::exception(
                        "unexpected callable parameter; expected optional Entity then concrete "
                        "component references",
                        type);
                }

                if (components.empty())
                {
                    throw std::meta::exception("callable must accept at least one component", ^^F);
                }

                return std::meta::substitute(^^std::tuple, components);
            }

            template <typename F>
            consteval auto ComponentsTupleAfterFirstInfo() -> std::meta::info
            {
                const auto callOp = ConcreteCallOperator<F>();
                const auto params = std::meta::parameters_of(callOp);

                if (params.size() < 2)
                {
                    throw std::meta::exception(
                        "callable must accept an accumulator followed by at least one component",
                        ^^F);
                }

                std::vector<std::meta::info> components;
                for (std::size_t index = 1; index < params.size(); ++index)
                {
                    const auto type = std::meta::remove_cvref(std::meta::type_of(params[index]));
                    if (!IsComponentType(type))
                    {
                        throw std::meta::exception("parameters after the accumulator must be "
                                                   "concrete component references",
                                                   type);
                    }
                    components.push_back(type);
                }

                return std::meta::substitute(^^std::tuple, components);
            }
        } // namespace detail

        template <typename F>
        using components_tuple_t = [:detail::ComponentsTupleInfo<std::remove_cvref_t<F>>():];

        template <typename F>
        using components_tuple_after_first_t = [:detail::ComponentsTupleAfterFirstInfo<
                                                     std::remove_cvref_t<F>>():];
    } // namespace meta
} // namespace FREYR_NAMESPACE
