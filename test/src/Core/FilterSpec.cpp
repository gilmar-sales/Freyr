#include <gtest/gtest.h>

#include <Freyr/Freyr.hpp>

#include "../Components/NameComponent.hpp"
#include "../Components/PositionComponent.hpp"
#include "../Components/VelocityComponent.hpp"
#include "../EmptyApp.hpp"

class FilterSpec : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        mApp = skr::ApplicationBuilder()
                   .WithExtension<fr::FreyrExtension>([](fr::FreyrExtension& freyr) {
                       freyr.WithComponent<PositionComponent>()
                           .WithComponent<VelocityComponent>()
                           .WithComponent<NameComponent>();
                   })
                   .Build<EmptyApp>();

        mRegistry         = mApp->GetRootServiceProvider()->GetService<fr::Registry>();
        mComponentManager = mApp->GetRootServiceProvider()->GetService<fr::ComponentManager>();

        mRegistry->CreateEntity(PositionComponent {});
        mRegistry->CreateEntity(PositionComponent {}, VelocityComponent {});
    }

    skr::Arc<EmptyApp>             mApp;
    skr::Arc<fr::Registry>         mRegistry;
    skr::Arc<fr::ComponentManager> mComponentManager;
};

TEST_F(FilterSpec, EmptyFilterMatchesAnyArchetype)
{
    fr::Filter  filter;
    std::size_t matches = 0;

    mComponentManager->ForEachArchetype([&](fr::Archetype* archetype) {
        if (filter.MatchArchetype(archetype))
            ++matches;
    });

    EXPECT_EQ(matches, 2);
}

TEST_F(FilterSpec, IncludingRequiresAllComponents)
{
    fr::Filter filter;
    filter.Including<PositionComponent, VelocityComponent>();

    std::size_t matches = 0;
    mComponentManager->ForEachArchetype([&](fr::Archetype* archetype) {
        if (filter.MatchArchetype(archetype))
            ++matches;
    });

    EXPECT_EQ(matches, 1);
}

TEST_F(FilterSpec, ExcludingRejectsMatchingArchetypes)
{
    fr::Filter filter;
    filter.Including<PositionComponent>();
    filter.Excluding<VelocityComponent>();

    std::size_t matches = 0;
    mComponentManager->ForEachArchetype([&](fr::Archetype* archetype) {
        if (filter.MatchArchetype(archetype))
            ++matches;
    });

    EXPECT_EQ(matches, 1);
}

TEST_F(FilterSpec, ExcludingAloneRejectsWhenPresent)
{
    fr::Filter filter;
    filter.Excluding<VelocityComponent>();

    std::size_t matches = 0;
    mComponentManager->ForEachArchetype([&](fr::Archetype* archetype) {
        if (filter.MatchArchetype(archetype))
            ++matches;
    });

    EXPECT_EQ(matches, 1);
}

TEST_F(FilterSpec, ExcludingRejectsWhenAnyExcludedComponentIsPresent)
{
    fr::Filter filter;
    filter.Including<PositionComponent>();
    filter.Excluding<VelocityComponent, NameComponent>();

    mRegistry->CreateEntity(PositionComponent {}, NameComponent {});

    std::size_t matches = 0;
    mComponentManager->ForEachArchetype([&](fr::Archetype* archetype) {
        if (filter.MatchArchetype(archetype))
            ++matches;
    });

    EXPECT_EQ(matches, 1);
}
