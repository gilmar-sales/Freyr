#include <Freyr/Freyr.hpp>
#include <Freyr/Hierarchy/Policies/Affine2DTransformPolicy.hpp>
#include <Freyr/Hierarchy/Policies/Mat4TransformPolicy.hpp>
#include <gtest/gtest.h>

#include "../EmptyApp.hpp"

#include <algorithm>
#include <vector>

namespace
{
    class HierarchySpec : public ::testing::TestWithParam<fr::HierarchyStorageMode>
    {
      protected:
        void SetUp() override
        {
            mApp = skr::ApplicationBuilder()
                       .WithExtension<fr::FreyrExtension>([mode = GetParam()](fr::FreyrExtension& freyr) {
                           freyr.WithHierarchy().WithOptions([mode](fr::FreyrOptionsBuilder& options) {
                               options.WithMaxEntities(4096).WithThreadCount(4).WithHierarchyStorage(mode);
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

INSTANTIATE_TEST_SUITE_P(Storage, HierarchySpec,
                         ::testing::Values(fr::HierarchyStorageMode::Dense,
                                           fr::HierarchyStorageMode::Sparse),
                         [](const ::testing::TestParamInfo<fr::HierarchyStorageMode>& info) {
                             return info.param == fr::HierarchyStorageMode::Dense ? "Dense" : "Sparse";
                         });

TEST_P(HierarchySpec, SetParentShouldUpdateChildOfParentDepthAndChildrenOrder)
{
    const auto root  = mRegistry->CreateEntity();
    const auto child = mRegistry->CreateEntity();
    const auto mid   = mRegistry->CreateEntity();

    ASSERT_TRUE(mRegistry->SetParent(child, root));
    ASSERT_TRUE(mRegistry->SetParent(mid, root));
    mRegistry->ExecuteTasks();

    EXPECT_EQ(mRegistry->GetParent(child), root);
    EXPECT_EQ(mRegistry->GetDepth(child), 1);
    EXPECT_TRUE(mRegistry->HasComponent<fr::ChildOf>(child));
    EXPECT_TRUE(mRegistry->HasComponent<fr::ParentDepth>(child));

    const auto              range = mRegistry->Children(root);
    const std::vector<fr::Entity> children(range.begin(), range.end());
    ASSERT_EQ(children.size(), 2u);
    EXPECT_EQ(children[0], child);
    EXPECT_EQ(children[1], mid);

    ASSERT_TRUE(mRegistry->SetParent(child, mid));
    mRegistry->ExecuteTasks();

    EXPECT_EQ(mRegistry->GetParent(child), mid);
    EXPECT_EQ(mRegistry->GetDepth(child), 2);
    EXPECT_EQ(mRegistry->Children(root).size(), 1u);
    EXPECT_EQ(mRegistry->Children(root).front(), mid);
    EXPECT_EQ(mRegistry->Children(mid).size(), 1u);
}

TEST_P(HierarchySpec, SetParentShouldRejectCycles)
{
    const auto a = mRegistry->CreateEntity();
    const auto b = mRegistry->CreateEntity();
    ASSERT_TRUE(mRegistry->SetParent(b, a));
    EXPECT_FALSE(mRegistry->SetParent(a, b));
    EXPECT_EQ(mRegistry->GetParent(a), fr::NullEntity);
}

TEST_P(HierarchySpec, ClearParentShouldDetachChild)
{
    const auto root  = mRegistry->CreateEntity();
    const auto child = mRegistry->CreateEntity();
    ASSERT_TRUE(mRegistry->SetParent(child, root));
    ASSERT_TRUE(mRegistry->ClearParent(child));
    mRegistry->ExecuteTasks();

    EXPECT_EQ(mRegistry->GetParent(child), fr::NullEntity);
    EXPECT_TRUE(mRegistry->Children(root).empty());
    EXPECT_FALSE(mRegistry->HasComponent<fr::ChildOf>(child));
}

TEST_P(HierarchySpec, CascadeDestroyShouldDestroyDescendants)
{
    const auto root  = mRegistry->CreateEntity(fr::ParentDepth {});
    const auto child = mRegistry->CreateEntity();
    const auto leaf  = mRegistry->CreateEntity();
    ASSERT_TRUE(mRegistry->SetParent(child, root));
    ASSERT_TRUE(mRegistry->SetParent(leaf, child));
    mRegistry->ExecuteTasks();

    mRegistry->DestroyEntity(root);
    mRegistry->ExecuteTasks();

    EXPECT_FALSE(mRegistry->HasComponent<fr::ParentDepth>(root));
    EXPECT_FALSE(mRegistry->HasComponent<fr::ChildOf>(child));
    EXPECT_FALSE(mRegistry->HasComponent<fr::ChildOf>(leaf));
    EXPECT_TRUE(mRegistry->Children(root).empty());
}

TEST_P(HierarchySpec, DestroyLeafShouldNotAffectSibling)
{
    const auto root = mRegistry->CreateEntity();
    const auto a    = mRegistry->CreateEntity();
    const auto b    = mRegistry->CreateEntity();
    ASSERT_TRUE(mRegistry->SetParent(a, root));
    ASSERT_TRUE(mRegistry->SetParent(b, root));
    mRegistry->ExecuteTasks();

    mRegistry->DestroyEntity(a);
    mRegistry->ExecuteTasks();

    EXPECT_EQ(mRegistry->Children(root).size(), 1u);
    EXPECT_EQ(mRegistry->Children(root).front(), b);
    EXPECT_EQ(mRegistry->GetParent(b), root);
}

TEST_P(HierarchySpec, ChildrenOrderShouldStayStableWhenRemovingMiddle)
{
    const auto root = mRegistry->CreateEntity();
    const auto a    = mRegistry->CreateEntity();
    const auto b    = mRegistry->CreateEntity();
    const auto c    = mRegistry->CreateEntity();
    ASSERT_TRUE(mRegistry->SetParent(a, root));
    ASSERT_TRUE(mRegistry->SetParent(b, root));
    ASSERT_TRUE(mRegistry->SetParent(c, root));

    ASSERT_TRUE(mRegistry->ClearParent(b));

    const auto              range = mRegistry->Children(root);
    const std::vector<fr::Entity> children(range.begin(), range.end());
    ASSERT_EQ(children.size(), 2u);
    EXPECT_EQ(children[0], a);
    EXPECT_EQ(children[1], c);
}

TEST_P(HierarchySpec, ChildOfParentHandleShouldInvalidateAfterParentRecycle)
{
    const auto parent = mRegistry->CreateEntity();
    const auto child  = mRegistry->CreateEntity();
    ASSERT_TRUE(mRegistry->SetParent(child, parent));
    mRegistry->ExecuteTasks();

    fr::EntityHandle parentHandle = fr::NullHandle;
    ASSERT_TRUE(mRegistry->TryGetComponents<fr::ChildOf>(
        child, [&](fr::ChildOf& childOf) { parentHandle = childOf.parent; }));
    ASSERT_TRUE(mRegistry->IsAlive(parentHandle));
    EXPECT_EQ(parentHandle.entity, parent);

    ASSERT_TRUE(mRegistry->ClearParent(child));
    mRegistry->DestroyEntity(parent);
    mRegistry->ExecuteTasks();

    const auto recycled = mRegistry->CreateEntity();
    ASSERT_EQ(recycled, parent);
    ASSERT_FALSE(mRegistry->IsAlive(parentHandle));
    ASSERT_TRUE(mRegistry->IsAlive(mRegistry->HandleOf(recycled)));
    EXPECT_EQ(mRegistry->GetParent(child), fr::NullEntity);
}

TEST_P(HierarchySpec, RootsWithChildrenShouldTrackReparentingAndDestroy)
{
    const auto hierarchy = mRegistry->GetHierarchyManager();
    const auto contains  = [&](fr::Entity entity) {
        const auto roots = hierarchy->RootsWithChildren();
        return std::ranges::find(roots, entity) != roots.end();
    };

    const auto a     = mRegistry->CreateEntity();
    const auto b     = mRegistry->CreateEntity();
    const auto child = mRegistry->CreateEntity();
    const auto leaf  = mRegistry->CreateEntity();
    EXPECT_TRUE(hierarchy->RootsWithChildren().empty());

    ASSERT_TRUE(mRegistry->SetParent(child, a));
    ASSERT_TRUE(mRegistry->SetParent(leaf, child));
    EXPECT_TRUE(contains(a));
    EXPECT_FALSE(contains(child));
    EXPECT_EQ(hierarchy->RootsWithChildren().size(), 1u);

    ASSERT_TRUE(mRegistry->SetParent(a, b));
    EXPECT_FALSE(contains(a));
    EXPECT_TRUE(contains(b));

    ASSERT_TRUE(mRegistry->ClearParent(child));
    EXPECT_FALSE(contains(a));
    EXPECT_TRUE(contains(b));
    EXPECT_TRUE(contains(child));

    ASSERT_TRUE(mRegistry->ClearParent(a));
    EXPECT_FALSE(contains(b));

    mRegistry->DestroyEntity(child);
    mRegistry->ExecuteTasks();
    EXPECT_FALSE(contains(child));
    EXPECT_TRUE(hierarchy->RootsWithChildren().empty());
}

TEST_P(HierarchySpec, FirstParentShouldReportChildOfAndParentDepthAsAdded)
{
    const auto parent = mRegistry->CreateEntity();
    const auto child  = mRegistry->CreateEntity();
    mRegistry->ExecuteTasks();
    mRegistry->Update(0.016f);

    std::vector<fr::Entity> added;
    mRegistry->ObserveAdd<fr::ChildOf>([&](fr::Entity e) { added.push_back(e); });

    ASSERT_TRUE(mRegistry->SetParent(child, parent));
    mRegistry->FlushHierarchyComponents();
    mRegistry->ExecuteTasks();

    EXPECT_EQ(mRegistry->CreateQuery()->Added<fr::ChildOf>().Count<fr::ChildOf>(), 1u);
    EXPECT_EQ(mRegistry->CreateQuery()->Added<fr::ParentDepth>().Count<fr::ParentDepth>(), 1u);
    ASSERT_EQ(added.size(), 1u);
    EXPECT_EQ(added[0], child);
}

TEST_P(HierarchySpec, ReparentingShouldReportChildOfAsChangedNotAdded)
{
    const auto first  = mRegistry->CreateEntity();
    const auto second = mRegistry->CreateEntity();
    const auto child  = mRegistry->CreateEntity();
    ASSERT_TRUE(mRegistry->SetParent(child, first));
    mRegistry->ExecuteTasks();
    mRegistry->Update(0.016f);

    std::vector<fr::Entity> added;
    mRegistry->ObserveAdd<fr::ChildOf>([&](fr::Entity e) { added.push_back(e); });

    ASSERT_TRUE(mRegistry->SetParent(child, second));
    mRegistry->FlushHierarchyComponents();
    mRegistry->ExecuteTasks();

    fr::EntityHandle parent = fr::NullHandle;
    ASSERT_TRUE(mRegistry->TryGetComponents<fr::ChildOf>(
        child, [&](fr::ChildOf& childOf) { parent = childOf.parent; }));
    EXPECT_EQ(parent.entity, second);
    EXPECT_EQ(mRegistry->CreateQuery()->Changed<fr::ChildOf>().Count<fr::ChildOf>(), 1u);
    EXPECT_EQ(mRegistry->CreateQuery()->Added<fr::ChildOf>().Count<fr::ChildOf>(), 0u);
    EXPECT_TRUE(added.empty());
}

TEST_P(HierarchySpec, ReparentingAtSameDepthShouldNotTouchParentDepth)
{
    const auto first  = mRegistry->CreateEntity();
    const auto second = mRegistry->CreateEntity();
    const auto child  = mRegistry->CreateEntity();
    const auto leaf   = mRegistry->CreateEntity();
    ASSERT_TRUE(mRegistry->SetParent(child, first));
    ASSERT_TRUE(mRegistry->SetParent(leaf, child));
    mRegistry->ExecuteTasks();
    mRegistry->Update(0.016f);

    ASSERT_TRUE(mRegistry->SetParent(child, second));
    mRegistry->FlushHierarchyComponents();
    mRegistry->ExecuteTasks();

    EXPECT_EQ(mRegistry->CreateQuery()->Changed<fr::ParentDepth>().Count<fr::ParentDepth>(), 0u);
    EXPECT_EQ(mRegistry->CreateQuery()->Changed<fr::ChildOf>().Count<fr::ChildOf>(), 1u);
}

TEST_P(HierarchySpec, ReparentingToDeeperParentShouldReportSubtreeParentDepthAsChanged)
{
    const auto root   = mRegistry->CreateEntity();
    const auto middle = mRegistry->CreateEntity();
    const auto child  = mRegistry->CreateEntity();
    const auto leaf   = mRegistry->CreateEntity();
    ASSERT_TRUE(mRegistry->SetParent(middle, root));
    ASSERT_TRUE(mRegistry->SetParent(child, root));
    ASSERT_TRUE(mRegistry->SetParent(leaf, child));
    mRegistry->ExecuteTasks();
    mRegistry->Update(0.016f);

    ASSERT_TRUE(mRegistry->SetParent(child, middle));
    mRegistry->FlushHierarchyComponents();
    mRegistry->ExecuteTasks();

    std::uint16_t childDepth = 0;
    std::uint16_t leafDepth  = 0;
    ASSERT_TRUE(mRegistry->TryGetComponents<fr::ParentDepth>(
        child, [&](fr::ParentDepth& depth) { childDepth = depth.depth; }));
    ASSERT_TRUE(mRegistry->TryGetComponents<fr::ParentDepth>(
        leaf, [&](fr::ParentDepth& depth) { leafDepth = depth.depth; }));
    EXPECT_EQ(childDepth, 2u);
    EXPECT_EQ(leafDepth, 3u);
    EXPECT_EQ(mRegistry->CreateQuery()->Changed<fr::ParentDepth>().Count<fr::ParentDepth>(), 2u);
    EXPECT_EQ(mRegistry->CreateQuery()->Added<fr::ParentDepth>().Count<fr::ParentDepth>(), 0u);
}
