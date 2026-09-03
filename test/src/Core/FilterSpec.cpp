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
        mRegistry->ExecuteTasks();
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

TEST_F(FilterSpec, ForEachMatchingArchetypeShouldFindIncludeMatches)
{
    fr::Filter filter;
    filter.Including<PositionComponent>();

    std::size_t matches = 0;
    mComponentManager->ForEachMatchingArchetype(filter, [&](fr::Archetype*) { ++matches; });

    EXPECT_EQ(matches, 2);
}

TEST_F(FilterSpec, IncludeSignatureIndexShouldReturnStableCacheReference)
{
    const auto includeSignature = fr::Signature::Make<PositionComponent>();

    const auto& first  = mComponentManager->ArchetypesMatchingInclude(includeSignature);
    const auto& second = mComponentManager->ArchetypesMatchingInclude(includeSignature);

    EXPECT_EQ(&first, &second);
    EXPECT_EQ(first.size(), 2u);
}

TEST_F(FilterSpec, NewArchetypeShouldAppendToMatchingIncludeCaches)
{
    fr::Filter filter;
    filter.Including<PositionComponent>();
    mComponentManager->ForEachMatchingArchetype(filter, [&](fr::Archetype*) {});

    const auto includeSignature = fr::Signature::Make<PositionComponent>();
    EXPECT_EQ(mComponentManager->ArchetypesMatchingInclude(includeSignature).size(), 2u);

    mRegistry->CreateEntity(PositionComponent {}, NameComponent {});
    mRegistry->ExecuteTasks();

    EXPECT_EQ(mComponentManager->ArchetypesMatchingInclude(includeSignature).size(), 3u);
}

TEST_F(FilterSpec, ForEachMatchingArchetypeShouldSeeArchetypeCreatedByMigration)
{
    fr::Filter filter;
    filter.Including<PositionComponent>();

    std::size_t matchesBefore = 0;
    mComponentManager->ForEachMatchingArchetype(filter, [&](fr::Archetype*) { ++matchesBefore; });

    const auto entity = mRegistry->CreateEntity(PositionComponent {});
    mRegistry->AddComponent(entity, NameComponent {});
    mRegistry->ExecuteTasks();

    std::size_t matchesAfter = 0;
    mComponentManager->ForEachMatchingArchetype(filter, [&](fr::Archetype*) { ++matchesAfter; });

    EXPECT_EQ(matchesBefore, 2u);
    EXPECT_EQ(matchesAfter, 3u);
}

TEST_F(FilterSpec, ExcludeFilterShouldRejectCachedIncludeArchetypes)
{
    fr::Filter filter;
    filter.Including<PositionComponent>();
    filter.Excluding<VelocityComponent>();

    std::size_t matches = 0;
    mComponentManager->ForEachMatchingArchetype(filter, [&](fr::Archetype*) { ++matches; });

    EXPECT_EQ(matches, 1u);
}

TEST_F(FilterSpec, MergedArchetypeShouldNotDuplicateIncludeIndexEntry)
{
    const auto includeSignature = fr::Signature::Make<PositionComponent>();

    fr::Filter filter;
    filter.Including<PositionComponent>();
    mComponentManager->ForEachMatchingArchetype(filter, [&](fr::Archetype*) {});

    const auto countBefore = mComponentManager->ArchetypesMatchingInclude(includeSignature).size();

    mRegistry->CreateArchetypeBuilder()
        .WithComponent(PositionComponent {})
        .WithEntities(10)
        .Build();

    mRegistry->CreateArchetypeBuilder()
        .WithComponent(PositionComponent {})
        .WithEntities(20)
        .Build();

    EXPECT_EQ(mComponentManager->ArchetypesMatchingInclude(includeSignature).size(), countBefore);
}

TEST_F(FilterSpec, FilterIndexShouldReturnStableCacheReference)
{
    fr::Filter filter;
    filter.Including<PositionComponent>();
    filter.Excluding<VelocityComponent>();

    const auto& first  = mComponentManager->ArchetypesMatchingFilter(filter);
    const auto& second = mComponentManager->ArchetypesMatchingFilter(filter);

    EXPECT_EQ(&first, &second);
    EXPECT_EQ(first.size(), 1u);
}

TEST_F(FilterSpec, ExcludeOnlyFilterShouldUseFilterCache)
{
    fr::Filter filter;
    filter.Excluding<VelocityComponent>();

    mComponentManager->ForEachMatchingArchetype(filter, [&](fr::Archetype*) {});

    const auto& cached = mComponentManager->ArchetypesMatchingFilter(filter);
    EXPECT_EQ(cached.size(), 1u);

    mRegistry->CreateEntity(PositionComponent {}, NameComponent {});

    mRegistry->ExecuteTasks();

    EXPECT_EQ(mComponentManager->ArchetypesMatchingFilter(filter).size(), 2u);
}
