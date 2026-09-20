#include <Freyr/Freyr.hpp>
#include <gtest/gtest.h>

namespace
{
    struct WorldPos : fr::Component
    {
        float x = 0.f;
    };
} // namespace

TEST(WorldSpec, TwoWorldsShouldBeIsolated)
{
    auto combat = fr::World::Create([](fr::FreyrExtension& freyr) {
        freyr.WithComponent<WorldPos>().WithOptions(
            [](fr::FreyrOptionsBuilder& options) { options.WithMaxEntities(64).WithThreadCount(1); });
    });

    auto lobby = fr::World::Create([](fr::FreyrExtension& freyr) {
        freyr.WithComponent<WorldPos>().WithOptions(
            [](fr::FreyrOptionsBuilder& options) { options.WithMaxEntities(64).WithThreadCount(1); });
    });

    const auto a = combat.Get().CreateEntity(WorldPos {.x = 1.f});
    combat.Get().ExecuteTasks();

    EXPECT_EQ(combat.Get().CreateQuery()->Count<WorldPos>(), 1u);
    EXPECT_EQ(lobby.Get().CreateQuery()->Count<WorldPos>(), 0u);
    EXPECT_TRUE(combat.Get().IsAlive(a));
    EXPECT_EQ(lobby.Get().CreateQuery()->Count<WorldPos>(), 0u);
}

TEST(WorldSpec, UnloadShouldNotAffectOtherWorld)
{
    auto primary = fr::World::Create([](fr::FreyrExtension& freyr) {
        freyr.WithComponent<WorldPos>().WithOptions(
            [](fr::FreyrOptionsBuilder& options) { options.WithMaxEntities(64).WithThreadCount(1); });
    });

    {
        auto temp = fr::World::Create([](fr::FreyrExtension& freyr) {
            freyr.WithComponent<WorldPos>().WithOptions([](fr::FreyrOptionsBuilder& options) {
                options.WithMaxEntities(64).WithThreadCount(1);
            });
        });
        temp.Get().CreateEntity(WorldPos {.x = 2.f});
        temp.Get().ExecuteTasks();
        EXPECT_EQ(temp.Get().CreateQuery()->Count<WorldPos>(), 1u);
    }

    primary.Get().CreateEntity(WorldPos {.x = 3.f});
    primary.Get().ExecuteTasks();
    EXPECT_EQ(primary.Get().CreateQuery()->Count<WorldPos>(), 1u);
}
