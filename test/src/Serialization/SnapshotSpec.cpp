#include <Freyr/Freyr.hpp>
#include <Freyr/Serialization/EntityRemapper.hpp>
#include <Freyr/Serialization/Snapshot.hpp>
#include <gtest/gtest.h>

#include "../EmptyApp.hpp"

#include <sstream>
#include <string>

namespace
{
    struct SnapshotPosition : fr::Component
    {
        float x = 0.f;
        float y = 0.f;
    };

    struct SnapshotVelocity : fr::Component
    {
        float dx = 0.f;
        float dy = 0.f;
    };

    struct SnapshotTarget : fr::Component
    {
        fr::EntityHandle handle = fr::NullHandle;
    };

    struct SnapshotNonPod : fr::Component
    {
        std::string name;
    };
} // namespace

template <>
struct fr::EntityRemapper<SnapshotTarget>
{
    static constexpr bool kEnabled = true;

    template <typename Map>
    static void Remap(SnapshotTarget& target, Map&& map)
    {
        target.handle = map(target.handle);
    }
};

namespace
{
    class SnapshotSpec : public ::testing::Test
    {
      protected:
        void SetUp() override
        {
            mApp = skr::ApplicationBuilder()
                       .WithExtension<fr::FreyrExtension>(
                           [](fr::FreyrExtension& freyr)
                           {
                               freyr.WithHierarchy()
                                   .WithComponent<SnapshotPosition>()
                                   .WithComponent<SnapshotVelocity>()
                                   .WithComponent<SnapshotTarget>()
                                   .WithOptions(
                                       [](fr::FreyrOptionsBuilder& options)
                                       {
                                           options.WithMaxEntities(4096)
                                               .WithThreadCount(4)
                                               .WithArchetypeChunkCapacity(64);
                                       });
                           })
                       .Build<EmptyApp>();
            mRegistry = mApp->GetRootServiceProvider()->GetService<fr::Registry>();
        }

        void TearDown() override
        {
            mRegistry.reset();
            mApp.reset();
        }

        skr::Arc<fr::Registry> mRegistry;
        skr::Arc<EmptyApp>     mApp;
    };
} // namespace

TEST_F(SnapshotSpec, RoundTripShouldPreservePodComponents)
{
    mRegistry->CreateEntity(SnapshotPosition {.x = 1.f, .y = 2.f},
                            SnapshotVelocity {.dx = 3.f, .dy = 4.f});
    mRegistry->CreateEntity(SnapshotPosition {.x = 5.f, .y = 6.f});
    mRegistry->ExecuteTasks();

    std::stringstream buffer;
    fr::SnapshotWriter {}.Save(*mRegistry, buffer);

    buffer.seekg(0);
    fr::SnapshotReader {}.Load(*mRegistry, buffer);

    std::uint32_t positionCount = 0;
    mRegistry->CreateQuery()->ForEachChunk<SnapshotPosition>(
        [&](fr::ChunkView view)
        {
            for (const auto& pos : view.Column<SnapshotPosition>())
            {
                ++positionCount;
                if (pos.x == 1.f)
                    EXPECT_FLOAT_EQ(pos.y, 2.f);
                else
                {
                    EXPECT_FLOAT_EQ(pos.x, 5.f);
                    EXPECT_FLOAT_EQ(pos.y, 6.f);
                }
            }
        });
    EXPECT_EQ(positionCount, 2u);

    std::uint32_t velocityCount = 0;
    mRegistry->CreateQuery()->ForEachChunk<SnapshotVelocity>(
        [&](fr::ChunkView view)
        {
            for (const auto& vel : view.Column<SnapshotVelocity>())
            {
                ++velocityCount;
                EXPECT_FLOAT_EQ(vel.dx, 3.f);
                EXPECT_FLOAT_EQ(vel.dy, 4.f);
            }
        });
    EXPECT_EQ(velocityCount, 1u);
}

TEST_F(SnapshotSpec, LoadShouldFailWhenComponentTypeMissing)
{
    mRegistry->CreateEntity(SnapshotPosition {.x = 1.f, .y = 2.f});
    mRegistry->ExecuteTasks();

    std::stringstream buffer;
    fr::SnapshotWriter {}.Save(*mRegistry, buffer);

    auto otherApp =
        skr::ApplicationBuilder()
            .WithExtension<fr::FreyrExtension>(
                [](fr::FreyrExtension& freyr)
                {
                    freyr.WithComponent<SnapshotVelocity>().WithOptions(
                        [](fr::FreyrOptionsBuilder& options)
                        { options.WithMaxEntities(4096).WithThreadCount(2); });
                })
            .Build<EmptyApp>();
    auto otherRegistry = otherApp->GetRootServiceProvider()->GetService<fr::Registry>();

    buffer.seekg(0);
    EXPECT_THROW(fr::SnapshotReader {}.Load(*otherRegistry, buffer), fr::SnapshotError);
}

TEST_F(SnapshotSpec, EntityHandleRemapShouldPreserveLogicalTarget)
{
    const auto target = mRegistry->CreateEntity(SnapshotPosition {.x = 9.f, .y = 9.f});
    mRegistry->CreateEntity(SnapshotTarget {.handle = mRegistry->HandleOf(target)});
    mRegistry->ExecuteTasks();

    std::stringstream buffer;
    fr::SnapshotWriter {}.Save(*mRegistry, buffer);
    buffer.seekg(0);
    fr::SnapshotReader {}.Load(*mRegistry, buffer);

    fr::EntityHandle remapped = fr::NullHandle;
    mRegistry->CreateQuery()->ForEachChunk<SnapshotTarget>(
        [&](fr::ChunkView view)
        {
            for (const auto& targetComp : view.Column<SnapshotTarget>())
                remapped = targetComp.handle;
        });
    ASSERT_TRUE(mRegistry->IsAlive(remapped));

    SnapshotPosition targetPos {};
    ASSERT_TRUE(mRegistry->TryGetComponents<SnapshotPosition>(
        remapped.entity, [&](SnapshotPosition& pos) { targetPos = pos; }));
    EXPECT_FLOAT_EQ(targetPos.x, 9.f);
    EXPECT_FLOAT_EQ(targetPos.y, 9.f);
}

TEST_F(SnapshotSpec, HierarchyRoundTripShouldRebuildParents)
{
    const auto root  = mRegistry->CreateEntity(SnapshotPosition {.x = 0.f, .y = 0.f});
    const auto child = mRegistry->CreateEntity(SnapshotPosition {.x = 1.f, .y = 1.f});
    ASSERT_TRUE(mRegistry->SetParent(child, root));
    mRegistry->ExecuteTasks();

    std::stringstream buffer;
    fr::SnapshotWriter {}.Save(*mRegistry, buffer);
    buffer.seekg(0);
    fr::SnapshotReader {}.Load(*mRegistry, buffer);

    fr::Entity loadedRoot  = fr::NullEntity;
    fr::Entity loadedChild = fr::NullEntity;
    mRegistry->CreateQuery()->ForEachChunk<SnapshotPosition>(
        [&](fr::ChunkView view)
        {
            auto positions = view.Column<SnapshotPosition>();
            auto entities  = view.Entities();
            for (std::size_t i = 0; i < positions.size(); ++i)
            {
                if (positions[i].x == 0.f)
                    loadedRoot = entities[i];
                if (positions[i].x == 1.f)
                    loadedChild = entities[i];
            }
        });

    ASSERT_NE(loadedRoot, fr::NullEntity);
    ASSERT_NE(loadedChild, fr::NullEntity);
    EXPECT_EQ(mRegistry->GetParent(loadedChild), loadedRoot);
    EXPECT_EQ(mRegistry->Children(loadedRoot).size(), 1u);
}

TEST_F(SnapshotSpec, SaveShouldRejectNonTriviallyCopyableComponents)
{
    auto app = skr::ApplicationBuilder()
                   .WithExtension<fr::FreyrExtension>(
                       [](fr::FreyrExtension& freyr)
                       {
                           freyr.WithComponent<SnapshotNonPod>().WithOptions(
                               [](fr::FreyrOptionsBuilder& options)
                               { options.WithMaxEntities(128).WithThreadCount(2); });
                       })
                   .Build<EmptyApp>();
    auto registry = app->GetRootServiceProvider()->GetService<fr::Registry>();
    registry->CreateEntity(SnapshotNonPod {.name = "nope"});
    registry->ExecuteTasks();

    std::stringstream buffer;
    EXPECT_THROW(fr::SnapshotWriter {}.Save(*registry, buffer), fr::SnapshotError);
}

TEST_F(SnapshotSpec, ParallelChunkRoundTripShouldPreserveManyEntities)
{
    constexpr std::uint32_t kCount = 300;
    for (std::uint32_t i = 0; i < kCount; ++i)
    {
        mRegistry->CreateEntity(
            SnapshotPosition {.x = static_cast<float>(i), .y = static_cast<float>(i * 2)});
    }
    mRegistry->ExecuteTasks();

    std::stringstream buffer;
    fr::SnapshotWriter {}.Save(*mRegistry, buffer);
    buffer.seekg(0);
    fr::SnapshotReader {}.Load(*mRegistry, buffer);

    std::uint32_t count = 0;
    float         sumX  = 0.f;
    mRegistry->CreateQuery()->ForEachChunk<SnapshotPosition>(
        [&](fr::ChunkView view)
        {
            for (const auto& pos : view.Column<SnapshotPosition>())
            {
                ++count;
                sumX += pos.x;
            }
        });
    EXPECT_EQ(count, kCount);
    EXPECT_FLOAT_EQ(sumX, static_cast<float>((kCount - 1) * kCount / 2));
}
