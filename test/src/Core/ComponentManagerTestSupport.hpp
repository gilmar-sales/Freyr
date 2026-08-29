#pragma once

#include "../EmptyApp.hpp"

#include "gtest/gtest.h"

#include <Freyr/Freyr.hpp>

#include "../Components/DecayComponent.hpp"
#include "../Components/ModelComponent.hpp"
#include "../Components/NameComponent.hpp"
#include "../Components/PositionComponent.hpp"
#include "../Components/VelocityComponent.hpp"

#include <string>
#include <tuple>
#include <type_traits>

template <typename T>
T MakeSampleComponent(int seed);

template <>
inline PositionComponent MakeSampleComponent<PositionComponent>(int seed)
{
    return PositionComponent { .x = static_cast<float>(seed),
                               .y = static_cast<float>(seed + 1),
                               .z = static_cast<float>(seed + 2) };
}

template <>
inline VelocityComponent MakeSampleComponent<VelocityComponent>(int seed)
{
    return VelocityComponent { .x = static_cast<float>(seed), .y = static_cast<float>(seed + 1) };
}

template <>
inline NameComponent MakeSampleComponent<NameComponent>(int seed)
{
    return NameComponent { .name = "entity-" + std::to_string(seed) };
}

template <>
inline ModelComponent MakeSampleComponent<ModelComponent>(int seed)
{
    return ModelComponent { .mesh     = static_cast<unsigned>(seed),
                            .material = static_cast<unsigned>(seed + 1),
                            .texture  = static_cast<unsigned>(seed + 2) };
}

template <>
inline DecayComponent MakeSampleComponent<DecayComponent>(int seed)
{
    return DecayComponent { .timeToLive = static_cast<float>(seed) };
}

template <typename T>
void AssertSampleEquals(const T& actual, const T& expected);

template <>
inline void AssertSampleEquals<PositionComponent>(const PositionComponent& actual,
                                                  const PositionComponent& expected)
{
    ASSERT_FLOAT_EQ(actual.x, expected.x);
    ASSERT_FLOAT_EQ(actual.y, expected.y);
    ASSERT_FLOAT_EQ(actual.z, expected.z);
}

template <>
inline void AssertSampleEquals<VelocityComponent>(const VelocityComponent& actual,
                                                  const VelocityComponent& expected)
{
    ASSERT_FLOAT_EQ(actual.x, expected.x);
    ASSERT_FLOAT_EQ(actual.y, expected.y);
}

template <>
inline void AssertSampleEquals<NameComponent>(const NameComponent& actual,
                                              const NameComponent& expected)
{
    ASSERT_EQ(actual.name, expected.name);
}

template <>
inline void AssertSampleEquals<ModelComponent>(const ModelComponent& actual,
                                               const ModelComponent& expected)
{
    ASSERT_EQ(actual.mesh, expected.mesh);
    ASSERT_EQ(actual.material, expected.material);
    ASSERT_EQ(actual.texture, expected.texture);
}

template <>
inline void AssertSampleEquals<DecayComponent>(const DecayComponent& actual,
                                               const DecayComponent& expected)
{
    ASSERT_FLOAT_EQ(actual.timeToLive, expected.timeToLive);
}

template <typename... Components>
struct ComponentPack
{
    static constexpr std::size_t Count = sizeof...(Components);

    static void RegisterAll(fr::ComponentManager& componentManager)
    {
        (componentManager.RegisterComponent<Components>(), ...);
    }

    static void AssertDistinctIndexes(fr::ComponentManager& componentManager)
    {
        const fr::ComponentId indexes[] = { componentManager.GetComponentIndex<Components>()... };
        for (std::size_t i = 0; i < Count; ++i)
        {
            for (std::size_t j = i + 1; j < Count; ++j)
            {
                ASSERT_NE(indexes[i], indexes[j]);
            }
        }
    }

    static void ExerciseRemoveOnEmptyEntity(
        fr::ComponentManager& componentManager, fr::Registry& registry, const fr::Entity entity)
    {
        (componentManager.RemoveComponent<Components>(entity), ...);
        registry.ExecuteTasks();

        const bool anyPresent = (componentManager.HasComponent<Components>(entity) || ...);
        ASSERT_FALSE(anyPresent);
        ASSERT_EQ(componentManager.GetEntityIndex(entity).archetype, nullptr);
        ASSERT_EQ(componentManager.GetEntityIndex(entity).archetypeChunk, nullptr);
    }

    template <typename Present, typename Absent>
    static void AssertPresentLacksAbsent(fr::ComponentManager& componentManager,
                                         const fr::Entity      entity)
    {
        if constexpr (!std::is_same_v<Present, Absent>)
        {
            ASSERT_FALSE(componentManager.HasComponent<Absent>(entity));
            ASSERT_FALSE(
                (componentManager.TryGetComponents<Present, Absent>(entity, [](Present&, Absent&) {
                })));
        }
    }

    template <typename C>
    static void ExerciseOneComponent(
        fr::ComponentManager& componentManager, fr::Registry& registry, const fr::Entity entity)
    {
        componentManager.AddComponent(entity, MakeSampleComponent<C>(static_cast<int>(entity)));
        registry.ExecuteTasks();

        ASSERT_TRUE(componentManager.HasComponent<C>(entity));
        AssertSampleEquals(componentManager.GetComponent<C>(entity),
                           MakeSampleComponent<C>(static_cast<int>(entity)));

        if constexpr (Count > 1)
        {
            ASSERT_FALSE((componentManager.HasComponents<Components...>(entity)));
        }

        bool visited = false;
        ASSERT_TRUE(componentManager.TryGetComponents<C>(entity, [&](C&) { visited = true; }));
        ASSERT_TRUE(visited);

        const auto [archetypeBefore, chunkBefore] = componentManager.GetEntityIndex(entity);
        componentManager.AddComponent(entity,
                                      MakeSampleComponent<C>(static_cast<int>(entity) + 1000));
        registry.ExecuteTasks();
        const auto [archetypeAfter, chunkAfter] = componentManager.GetEntityIndex(entity);
        ASSERT_EQ(archetypeBefore, archetypeAfter);
        ASSERT_EQ(chunkBefore, chunkAfter);
        AssertSampleEquals(componentManager.GetComponent<C>(entity),
                           MakeSampleComponent<C>(static_cast<int>(entity) + 1000));

        (AssertPresentLacksAbsent<C, Components>(componentManager, entity), ...);

        std::size_t foreachCount = 0;
        componentManager.ForEach<C>("ComponentPackSingular", [&](fr::Entity, C&) {
            ++foreachCount;
        });
        ASSERT_GE(foreachCount, 1u);

        componentManager.RemoveComponent<C>(entity);
        registry.ExecuteTasks();
        ASSERT_FALSE(componentManager.HasComponent<C>(entity));
    }

    static void ExerciseSingularLifecycle(
        fr::ComponentManager& componentManager, fr::Registry& registry, const fr::Entity baseEntity)
    {
        fr::Entity next = baseEntity;
        auto       run  = [&]<typename C>() {
            ExerciseOneComponent<C>(componentManager, registry, next++);
        };
        (run.template operator()<Components>(), ...);
    }

    static void ExerciseMultiAddRemove(
        fr::ComponentManager& componentManager, fr::Registry& registry, const fr::Entity entity)
    {
        componentManager.AddComponents<Components...>(
            entity,
            MakeSampleComponent<Components>(static_cast<int>(entity))...,
            [](fr::Entity, Components&...) {});
        registry.ExecuteTasks();

        ASSERT_TRUE((componentManager.HasComponents<Components...>(entity)));

        bool visited = false;
        ASSERT_TRUE(componentManager.TryGetComponents<Components...>(entity, [&](Components&...) {
            visited = true;
        }));
        ASSERT_TRUE(visited);

        std::size_t foreachCount = 0;
        componentManager.ForEach<Components...>(
            "ComponentPackMulti",
            [&](fr::Entity, Components&...) { ++foreachCount; });
        ASSERT_EQ(foreachCount, 1u);

        componentManager.RemoveComponents<Components...>(entity);
        registry.ExecuteTasks();

        const bool anyPresent = (componentManager.HasComponent<Components>(entity) || ...);
        ASSERT_FALSE(anyPresent);
        ASSERT_EQ(componentManager.GetEntityIndex(entity).archetype, nullptr);
    }
};

using AllTestComponents =
    ComponentPack<PositionComponent,
                  VelocityComponent,
                  NameComponent,
                  ModelComponent,
                  DecayComponent>;

using CoreTestComponents =
    ComponentPack<PositionComponent, VelocityComponent, NameComponent, ModelComponent>;

constexpr auto kComponentManagerChunkCapacity = 128;
constexpr auto kComponentManagerChunkCount    = 4;
constexpr auto kComponentManagerEntityCount =
    kComponentManagerChunkCapacity * kComponentManagerChunkCount;

class ComponentManagerSpec : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        auto app = skr::ApplicationBuilder()
                       .WithExtension<fr::FreyrExtension>([](fr::FreyrExtension& freyr) {
                           freyr.WithOptions([](fr::FreyrOptionsBuilder& options) {
                               options.WithArchetypeChunkCapacity(kComponentManagerChunkCapacity);
                           });
                       })
                       .Build<EmptyApp>();

        mServiceProvider  = app->GetRootServiceProvider();
        mComponentManager = mServiceProvider->GetService<fr::ComponentManager>();
        mRegistry         = mServiceProvider->GetService<fr::Registry>();
    }

    void TearDown() override { mComponentManager.reset(); }

    skr::Arc<fr::ComponentManager> mComponentManager;
    skr::Arc<fr::Registry>         mRegistry;
    skr::Arc<skr::ServiceProvider> mServiceProvider;
};
