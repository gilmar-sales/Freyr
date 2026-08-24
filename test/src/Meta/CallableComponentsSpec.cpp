#include <gtest/gtest.h>

#include "Freyr/Meta/CallableComponents.hpp"

#include "../Components/PositionComponent.hpp"
#include "../Components/VelocityComponent.hpp"

#include <tuple>
#include <type_traits>

TEST(CallableComponentsSpec, DeduceFromTypedEntityAndConcreteComponents)
{
    using Lambda = decltype([](fr::Entity, PositionComponent&, VelocityComponent&) {});
    using Tuple  = fr::meta::components_tuple_t<Lambda>;
    static_assert(std::is_same_v<Tuple, std::tuple<PositionComponent, VelocityComponent>>);
}

TEST(CallableComponentsSpec, DeduceFromTypedEntity)
{
    using Lambda = decltype([](fr::Entity, PositionComponent&) {});
    using Tuple  = fr::meta::components_tuple_t<Lambda>;
    static_assert(std::is_same_v<Tuple, std::tuple<PositionComponent>>);
}

TEST(CallableComponentsSpec, DeduceWithoutEntity)
{
    using Lambda = decltype([](PositionComponent&) {});
    using Tuple  = fr::meta::components_tuple_t<Lambda>;
    static_assert(std::is_same_v<Tuple, std::tuple<PositionComponent>>);
}
