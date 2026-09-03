#include "../EmptyApp.hpp"

#include "gtest/gtest.h"
#include <Freyr/Freyr.hpp>

#include "../Components/ModelComponent.hpp"
#include "../Components/PositionComponent.hpp"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

class ArchetypeSpec : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        auto app = skr::ApplicationBuilder()
                       .WithExtension<fr::FreyrExtension>([](fr::FreyrExtension& freyr) {
                           freyr.WithOptions([](fr::FreyrOptionsBuilder& builder) {
                               builder.WithArchetypeChunkCapacity(2048);
                           });
                       })
                       .Build<EmptyApp>();

        const auto provider = app->GetRootServiceProvider();

        mArchetype = provider->GetService<fr::Archetype>();
    }

    void TearDown() override { mArchetype.reset(); }

    skr::Arc<fr::Archetype> mArchetype;
};

TEST_F(ArchetypeSpec, ArchetypeShouldRegisterComponent)
{
    mArchetype->RegisterComponent<PositionComponent>();

    ASSERT_TRUE(mArchetype->HasComponent<PositionComponent>());
}

TEST_F(ArchetypeSpec, GetNameIncludesRegisteredComponentTypeNames)
{
    mArchetype->RegisterComponent<PositionComponent>();
    mArchetype->RegisterComponent<ModelComponent>();

    const auto name = mArchetype->GetName();
    EXPECT_NE(name.find(refl::type_name<PositionComponent>()), std::string_view::npos);
    EXPECT_NE(name.find(refl::type_name<ModelComponent>()), std::string_view::npos);
}

TEST_F(ArchetypeSpec, ForEachComponentReportsRegisteredIdsAndNames)
{
    mArchetype->RegisterComponent<PositionComponent>();
    mArchetype->RegisterComponent<ModelComponent>();

    std::vector<fr::ComponentId> ids;
    std::vector<std::string>     names;
    mArchetype->ForEachComponent([&](fr::ComponentId id, std::string_view name) {
        ids.push_back(id);
        names.emplace_back(name);
    });

    ASSERT_EQ(ids.size(), 2u);
    EXPECT_NE(std::find(ids.begin(), ids.end(), fr::GetComponentId<PositionComponent>()),
              ids.end());
    EXPECT_NE(std::find(ids.begin(), ids.end(), fr::GetComponentId<ModelComponent>()), ids.end());
    EXPECT_NE(
        std::find(names.begin(), names.end(), std::string(refl::type_name<PositionComponent>())),
        names.end());
    EXPECT_NE(std::find(names.begin(), names.end(), std::string(refl::type_name<ModelComponent>())),
              names.end());
}

TEST_F(ArchetypeSpec, RegistryForEachArchetypeExposesLiveArchetypes)
{
    auto app = skr::ApplicationBuilder()
                   .WithExtension<fr::FreyrExtension>([](fr::FreyrExtension& freyr) {
                       freyr.WithComponent<PositionComponent>().WithComponent<ModelComponent>();
                   })
                   .Build<EmptyApp>();

    const auto registry = app->GetRootServiceProvider()->GetService<fr::Registry>();

    EXPECT_EQ(registry->ArchetypeCount(), 0u);

    registry->CreateEntity(PositionComponent { .x = 1.f });
    registry->CreateEntity(PositionComponent { .x = 2.f }, ModelComponent { .mesh = 3 });

    registry->ExecuteTasks();
    EXPECT_EQ(registry->ArchetypeCount(), 2u);

    std::size_t seen      = 0;
    std::size_t entitySum = 0;
    std::size_t chunkSum  = 0;
    registry->ForEachArchetype([&](const fr::Archetype* archetype) {
        ++seen;
        entitySum += archetype->Count();
        chunkSum += archetype->ChunkCount();
    });

    EXPECT_EQ(seen, 2u);
    EXPECT_EQ(entitySum, 2u);
    EXPECT_GE(chunkSum, 2u);
}
