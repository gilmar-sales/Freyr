#include <Freyr/Freyr.hpp>
#include <Freyr/Hierarchy/Policies/Affine2DTransformPolicy.hpp>
#include <Freyr/Hierarchy/Policies/Mat4TransformPolicy.hpp>
#include <gtest/gtest.h>

#include "../EmptyApp.hpp"

#include <array>
#include <chrono>
#include <cmath>
#include <vector>

namespace
{
    struct ScalePolicy
    {
        using Local = fr::LocalTransform3D;
        using World = fr::WorldTransform3D;

        void OnRoot(fr::ComponentManager& cm, fr::Entity entity) const
        {
            const auto& local = cm.GetComponent<Local>(entity);
            auto&       world = cm.GetComponent<World>(entity);
            for (int i = 0; i < 16; ++i)
                world.matrix[i] = local.matrix[i];
            world.matrix[0] *= 2.f;
            world.matrix[5] *= 2.f;
            world.matrix[10] *= 2.f;
        }

        void Propagate(fr::ComponentManager& cm, fr::Entity parent, fr::Entity child) const
        {
            fr::Mat4TransformPolicy {}.Propagate(cm, parent, child);
            auto& world = cm.GetComponent<World>(child);
            world.matrix[0] *= 2.f;
            world.matrix[5] *= 2.f;
            world.matrix[10] *= 2.f;
        }

        bool HasChildrenInterest(fr::ComponentManager&, fr::Entity) const { return true; }
    };

    class HierarchyPropagationSpec : public ::testing::Test
    {
      protected:
        void SetUpMat4()
        {
            mApp = skr::ApplicationBuilder()
                       .WithExtension<fr::FreyrExtension>([](fr::FreyrExtension& freyr) {
                           freyr.WithHierarchyPropagation<fr::Mat4TransformPolicy>().WithOptions(
                               [](fr::FreyrOptionsBuilder& options) {
                                   options.WithMaxEntities(4096).WithThreadCount(4);
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

TEST_F(HierarchyPropagationSpec, Mat4PropagationShouldComposeParentWorldTimesLocal)
{
    SetUpMat4();

    const auto a = mRegistry->CreateEntity(fr::TranslationLocal3D(1.f, 0.f, 0.f),
                                           fr::WorldTransform3D {});
    const auto b = mRegistry->CreateEntity(fr::TranslationLocal3D(0.f, 2.f, 0.f),
                                           fr::WorldTransform3D {});
    const auto c = mRegistry->CreateEntity(fr::TranslationLocal3D(0.f, 0.f, 3.f),
                                           fr::WorldTransform3D {});

    ASSERT_TRUE(mRegistry->SetParent(b, a));
    ASSERT_TRUE(mRegistry->SetParent(c, b));
    mRegistry->ExecuteTasks();
    mRegistry->Update(0.016f);

    float cx = 0.f, cy = 0.f, cz = 0.f;
    ASSERT_TRUE(mRegistry->TryGetComponents<fr::WorldTransform3D>(c, [&](fr::WorldTransform3D& w) {
        cx = w.matrix[12];
        cy = w.matrix[13];
        cz = w.matrix[14];
    }));

    EXPECT_FLOAT_EQ(cx, 1.f);
    EXPECT_FLOAT_EQ(cy, 2.f);
    EXPECT_FLOAT_EQ(cz, 3.f);
}

TEST_F(HierarchyPropagationSpec, Affine2DPropagationShouldComposeTranslation)
{
    mApp = skr::ApplicationBuilder()
               .WithExtension<fr::FreyrExtension>([](fr::FreyrExtension& freyr) {
                   freyr.WithHierarchyPropagation<fr::Affine2DTransformPolicy>().WithOptions(
                       [](fr::FreyrOptionsBuilder& options) {
                           options.WithMaxEntities(4096).WithThreadCount(2);
                       });
               })
               .Build<EmptyApp>();
    mRegistry = mApp->GetRootServiceProvider()->GetService<fr::Registry>();

    const auto root = mRegistry->CreateEntity(fr::LocalTransform2D {.x = 10.f, .y = 0.f},
                                              fr::WorldTransform2D {});
    const auto child =
        mRegistry->CreateEntity(fr::LocalTransform2D {.x = 5.f, .y = 2.f}, fr::WorldTransform2D {});
    ASSERT_TRUE(mRegistry->SetParent(child, root));
    mRegistry->ExecuteTasks();
    mRegistry->Update(0.016f);

    float x = 0.f, y = 0.f;
    ASSERT_TRUE(mRegistry->TryGetComponents<fr::WorldTransform2D>(
        child, [&](fr::WorldTransform2D& w) {
            x = w.m02;
            y = w.m12;
        }));
    EXPECT_FLOAT_EQ(x, 15.f);
    EXPECT_FLOAT_EQ(y, 2.f);
}

TEST_F(HierarchyPropagationSpec, CustomPolicyShouldBeInvoked)
{
    mApp = skr::ApplicationBuilder()
               .WithExtension<fr::FreyrExtension>([](fr::FreyrExtension& freyr) {
                   freyr.WithHierarchyPropagation<ScalePolicy>().WithOptions(
                       [](fr::FreyrOptionsBuilder& options) {
                           options.WithMaxEntities(4096).WithThreadCount(2);
                       });
               })
               .Build<EmptyApp>();
    mRegistry = mApp->GetRootServiceProvider()->GetService<fr::Registry>();

    const auto root =
        mRegistry->CreateEntity(fr::LocalTransform3D {}, fr::WorldTransform3D {});
    mRegistry->ExecuteTasks();
    mRegistry->Update(0.016f);

    float sx = 0.f;
    ASSERT_TRUE(mRegistry->TryGetComponents<fr::WorldTransform3D>(
        root, [&](fr::WorldTransform3D& w) { sx = w.matrix[0]; }));
    EXPECT_FLOAT_EQ(sx, 2.f);
}

TEST_F(HierarchyPropagationSpec, ParallelPropagationShouldBeDeterministic)
{
    SetUpMat4();

    const auto root =
        mRegistry->CreateEntity(fr::TranslationLocal3D(0.f, 0.f, 0.f), fr::WorldTransform3D {});
    std::vector<fr::Entity> leaves;
    for (int i = 0; i < 64; ++i)
    {
        const auto branch =
            mRegistry->CreateEntity(fr::TranslationLocal3D(1.f, 0.f, 0.f), fr::WorldTransform3D {});
        const auto leaf =
            mRegistry->CreateEntity(fr::TranslationLocal3D(0.f, 1.f, 0.f), fr::WorldTransform3D {});
        ASSERT_TRUE(mRegistry->SetParent(branch, root));
        ASSERT_TRUE(mRegistry->SetParent(leaf, branch));
        leaves.push_back(leaf);
    }

    mRegistry->ExecuteTasks();
    mRegistry->Update(0.016f);

    for (const auto leaf : leaves)
    {
        float x = 0.f, y = 0.f;
        ASSERT_TRUE(mRegistry->TryGetComponents<fr::WorldTransform3D>(
            leaf, [&](fr::WorldTransform3D& w) {
                x = w.matrix[12];
                y = w.matrix[13];
            }));
        EXPECT_FLOAT_EQ(x, 1.f);
        EXPECT_FLOAT_EQ(y, 1.f);
    }
}

TEST_F(HierarchyPropagationSpec, WorkSharingDoesNotHangOnDeepChain)
{
    SetUpMat4();

    fr::Entity prev =
        mRegistry->CreateEntity(fr::TranslationLocal3D(1.f, 0.f, 0.f), fr::WorldTransform3D {});
    fr::Entity leaf = prev;
    for (int i = 0; i < 63; ++i)
    {
        leaf = mRegistry->CreateEntity(fr::TranslationLocal3D(1.f, 0.f, 0.f),
                                       fr::WorldTransform3D {});
        ASSERT_TRUE(mRegistry->SetParent(leaf, prev));
        prev = leaf;
        if ((i % 16) == 0)
            mRegistry->ExecuteTasks();
    }
    mRegistry->ExecuteTasks();

    const auto start = std::chrono::steady_clock::now();
    mRegistry->Update(0.016f);
    const auto elapsed = std::chrono::steady_clock::now() - start;
    EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(), 500);

    float x = 0.f;
    ASSERT_TRUE(mRegistry->TryGetComponents<fr::WorldTransform3D>(
        leaf, [&](fr::WorldTransform3D& w) { x = w.matrix[12]; }));
    EXPECT_FLOAT_EQ(x, 64.f);
}

TEST_F(HierarchyPropagationSpec, WorkSharingDoesNotHangOnWideLeaves)
{
    mApp = skr::ApplicationBuilder()
               .WithExtension<fr::FreyrExtension>([](fr::FreyrExtension& freyr) {
                   freyr.WithHierarchyPropagation<fr::Mat4TransformPolicy>().WithOptions(
                       [](fr::FreyrOptionsBuilder& options) {
                           options.WithMaxEntities(16'384).WithThreadCount(4);
                       });
               })
               .Build<EmptyApp>();
    mRegistry = mApp->GetRootServiceProvider()->GetService<fr::Registry>();

    const auto root =
        mRegistry->CreateEntity(fr::TranslationLocal3D(0.f, 0.f, 0.f), fr::WorldTransform3D {});
    for (int i = 0; i < 10'000; ++i)
    {
        const auto leaf =
            mRegistry->CreateEntity(fr::TranslationLocal3D(1.f, 0.f, 0.f), fr::WorldTransform3D {});
        ASSERT_TRUE(mRegistry->SetParent(leaf, root));
        if ((i % 256) == 0)
            mRegistry->ExecuteTasks();
    }
    mRegistry->ExecuteTasks();

    const auto start = std::chrono::steady_clock::now();
    mRegistry->Update(0.016f);
    const auto elapsed = std::chrono::steady_clock::now() - start;
    EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(), 2'000);
}

TEST_F(HierarchyPropagationSpec, WorkSharingMatchesLevelSyncWorlds)
{
    auto buildAndCapture = [](fr::HierarchyPropagationMode mode,
                              std::vector<std::array<float, 4>>& worlds) -> bool {
        auto app = skr::ApplicationBuilder()
                       .WithExtension<fr::FreyrExtension>([](fr::FreyrExtension& freyr) {
                           freyr.WithHierarchyPropagation<fr::Mat4TransformPolicy>().WithOptions(
                               [](fr::FreyrOptionsBuilder& options) {
                                   options.WithMaxEntities(8192).WithThreadCount(4);
                               });
                       })
                       .Build<EmptyApp>();
        auto registry = app->GetRootServiceProvider()->GetService<fr::Registry>();
        registry->GetHierarchyManager()->SetPropagationMode(mode);

        const auto root =
            registry->CreateEntity(fr::TranslationLocal3D(0.f, 0.f, 0.f), fr::WorldTransform3D {});
        std::vector<fr::Entity> entities { root };

        for (int i = 0; i < 32; ++i)
        {
            const auto deep = registry->CreateEntity(fr::TranslationLocal3D(1.f, 0.f, 0.f),
                                                     fr::WorldTransform3D {});
            if (!registry->SetParent(deep, entities.back()))
                return false;
            entities.push_back(deep);
        }

        for (int i = 0; i < 128; ++i)
        {
            const auto branch = registry->CreateEntity(fr::TranslationLocal3D(0.f, 1.f, 0.f),
                                                       fr::WorldTransform3D {});
            const auto leaf   = registry->CreateEntity(fr::TranslationLocal3D(0.f, 0.f, 1.f),
                                                       fr::WorldTransform3D {});
            if (!registry->SetParent(branch, root) || !registry->SetParent(leaf, branch))
                return false;
            entities.push_back(branch);
            entities.push_back(leaf);
            if ((i % 32) == 0)
                registry->ExecuteTasks();
        }
        registry->ExecuteTasks();
        registry->Update(0.016f);

        worlds.clear();
        worlds.reserve(entities.size());
        for (const auto entity : entities)
        {
            std::array<float, 4> sample {};
            const bool ok = registry->TryGetComponents<fr::WorldTransform3D>(
                entity, [&](fr::WorldTransform3D& w) {
                    sample = { w.matrix[12], w.matrix[13], w.matrix[14], w.matrix[0] };
                });
            if (!ok)
                return false;
            worlds.push_back(sample);
        }
        return true;
    };

    std::vector<std::array<float, 4>> levelSync;
    std::vector<std::array<float, 4>> workSharing;
    ASSERT_TRUE(buildAndCapture(fr::HierarchyPropagationMode::LevelSync, levelSync));
    ASSERT_TRUE(buildAndCapture(fr::HierarchyPropagationMode::WorkSharing, workSharing));
    ASSERT_EQ(levelSync.size(), workSharing.size());
    for (std::size_t i = 0; i < levelSync.size(); ++i)
    {
        EXPECT_FLOAT_EQ(levelSync[i][0], workSharing[i][0]) << i;
        EXPECT_FLOAT_EQ(levelSync[i][1], workSharing[i][1]) << i;
        EXPECT_FLOAT_EQ(levelSync[i][2], workSharing[i][2]) << i;
        EXPECT_FLOAT_EQ(levelSync[i][3], workSharing[i][3]) << i;
    }
}

TEST_F(HierarchyPropagationSpec, DirtySubtreeSkipsCleanBranches)
{
    SetUpMat4();

    const auto root =
        mRegistry->CreateEntity(fr::TranslationLocal3D(0.f, 0.f, 0.f), fr::WorldTransform3D {});
    const auto clean =
        mRegistry->CreateEntity(fr::TranslationLocal3D(1.f, 0.f, 0.f), fr::WorldTransform3D {});
    const auto dirty =
        mRegistry->CreateEntity(fr::TranslationLocal3D(0.f, 1.f, 0.f), fr::WorldTransform3D {});
    ASSERT_TRUE(mRegistry->SetParent(clean, root));
    ASSERT_TRUE(mRegistry->SetParent(dirty, root));
    mRegistry->ExecuteTasks();
    mRegistry->Update(0.016f);

    float cleanX = 0.f;
    ASSERT_TRUE(mRegistry->TryGetComponents<fr::WorldTransform3D>(
        clean, [&](fr::WorldTransform3D& w) { cleanX = w.matrix[12]; }));
    EXPECT_FLOAT_EQ(cleanX, 1.f);

    ASSERT_TRUE(mRegistry->TryGetComponents<fr::LocalTransform3D>(
        dirty, [&](fr::LocalTransform3D& local) { local.matrix[13] = 5.f; }));
    mRegistry->MarkHierarchyDirty(dirty);
    mRegistry->Update(0.016f);

    float dirtyY = 0.f;
    float cleanXAfter = 0.f;
    ASSERT_TRUE(mRegistry->TryGetComponents<fr::WorldTransform3D>(
        dirty, [&](fr::WorldTransform3D& w) { dirtyY = w.matrix[13]; }));
    ASSERT_TRUE(mRegistry->TryGetComponents<fr::WorldTransform3D>(
        clean, [&](fr::WorldTransform3D& w) { cleanXAfter = w.matrix[12]; }));
    EXPECT_FLOAT_EQ(dirtyY, 5.f);
    EXPECT_FLOAT_EQ(cleanXAfter, cleanX);
}

