#include <Freyr/Freyr.hpp>
#include <Freyr/Hierarchy/Policies/Affine2DTransformPolicy.hpp>
#include <Freyr/Hierarchy/Policies/Mat4TransformPolicy.hpp>
#include <gtest/gtest.h>

#include "../EmptyApp.hpp"

#include <algorithm>
#include <vector>

namespace
{
    struct Marker
    {
    };

    class HierarchySpec : public ::testing::TestWithParam<fr::HierarchyStorageMode>
    {
      protected:
        void SetUp() override
        {
            mApp = skr::ApplicationBuilder()
                       .WithExtension<fr::FreyrExtension>([mode = GetParam()](fr::FreyrExtension& freyr) {
                           freyr.WithHierarchy()
                               .WithComponent<Marker>()
                               .WithOptions([mode](fr::FreyrOptionsBuilder& options) {
                                   options.WithMaxEntities(4096).WithThreadCount(4).WithHierarchyStorage(
                                       mode);
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

// ── ForEachRoot ───────────────────────────────────────────────────────────────

TEST_P(HierarchySpec, ForEachRootShouldReturnOnlyRootEntities)
{
    const auto root       = mRegistry->CreateEntity();
    const auto child      = mRegistry->CreateEntity();
    const auto grandchild = mRegistry->CreateEntity();
    ASSERT_TRUE(mRegistry->SetParent(child, root));
    ASSERT_TRUE(mRegistry->SetParent(grandchild, child));

    std::vector<fr::Entity> visited;
    mRegistry->ForEachRoot([&](fr::Entity e) { visited.push_back(e); });

    ASSERT_EQ(visited.size(), 1u);
    EXPECT_EQ(visited[0], root);
}

TEST_P(HierarchySpec, ForEachRootShouldUpdateWhenSetParentOrClearParent)
{
    const auto a    = mRegistry->CreateEntity();
    const auto b    = mRegistry->CreateEntity();
    const auto newP = mRegistry->CreateEntity();
    ASSERT_TRUE(mRegistry->SetParent(b, a));

    {
        std::vector<fr::Entity> roots;
        mRegistry->ForEachRoot([&](fr::Entity e) { roots.push_back(e); });
        ASSERT_EQ(roots.size(), 1u);
        EXPECT_EQ(roots[0], a);
    }

    ASSERT_TRUE(mRegistry->SetParent(a, newP));

    {
        std::vector<fr::Entity> roots;
        mRegistry->ForEachRoot([&](fr::Entity e) { roots.push_back(e); });
        ASSERT_EQ(roots.size(), 1u);
        EXPECT_EQ(roots[0], newP);
    }

    ASSERT_TRUE(mRegistry->ClearParent(a));

    {
        std::vector<fr::Entity> roots;
        mRegistry->ForEachRoot([&](fr::Entity e) { roots.push_back(e); });
        EXPECT_EQ(roots.size(), 2u);
        EXPECT_TRUE(std::ranges::find(roots, a) != roots.end());
        EXPECT_TRUE(std::ranges::find(roots, newP) != roots.end());
    }
}

TEST_P(HierarchySpec, ForEachRootShouldRemoveDestroyedEntity)
{
    const auto root  = mRegistry->CreateEntity();
    const auto child = mRegistry->CreateEntity();
    ASSERT_TRUE(mRegistry->SetParent(child, root));

    {
        std::vector<fr::Entity> roots;
        mRegistry->ForEachRoot([&](fr::Entity e) { roots.push_back(e); });
        EXPECT_EQ(roots.size(), 1u);
    }

    mRegistry->DestroyEntity(root);
    mRegistry->ExecuteTasks();

    std::vector<fr::Entity> roots;
    mRegistry->ForEachRoot([&](fr::Entity e) { roots.push_back(e); });
    EXPECT_TRUE(roots.empty());
}

// ── IsAncestorOf / IsDescendantOf ─────────────────────────────────────────────

TEST_P(HierarchySpec, IsAncestorOfShouldBeTrueForDirectParent)
{
    const auto parent = mRegistry->CreateEntity();
    const auto child  = mRegistry->CreateEntity();
    ASSERT_TRUE(mRegistry->SetParent(child, parent));

    EXPECT_TRUE(mRegistry->IsAncestorOf(parent, child));
    EXPECT_TRUE(mRegistry->IsDescendantOf(child, parent));
    EXPECT_FALSE(mRegistry->IsAncestorOf(child, parent));
    EXPECT_FALSE(mRegistry->IsDescendantOf(parent, child));
}

TEST_P(HierarchySpec, IsAncestorOfShouldBeTrueForGrandparent)
{
    const auto grandparent = mRegistry->CreateEntity();
    const auto parent      = mRegistry->CreateEntity();
    const auto child       = mRegistry->CreateEntity();
    ASSERT_TRUE(mRegistry->SetParent(parent, grandparent));
    ASSERT_TRUE(mRegistry->SetParent(child, parent));

    EXPECT_TRUE(mRegistry->IsAncestorOf(grandparent, child));
    EXPECT_TRUE(mRegistry->IsDescendantOf(child, grandparent));
}

TEST_P(HierarchySpec, IsAncestorOfShouldBeFalseForSiblings)
{
    const auto root = mRegistry->CreateEntity();
    const auto a    = mRegistry->CreateEntity();
    const auto b    = mRegistry->CreateEntity();
    ASSERT_TRUE(mRegistry->SetParent(a, root));
    ASSERT_TRUE(mRegistry->SetParent(b, root));

    EXPECT_FALSE(mRegistry->IsAncestorOf(a, b));
    EXPECT_FALSE(mRegistry->IsAncestorOf(b, a));
    EXPECT_FALSE(mRegistry->IsDescendantOf(a, b));
    EXPECT_FALSE(mRegistry->IsDescendantOf(b, a));
}

TEST_P(HierarchySpec, IsAncestorOfShouldBeFalseForSelf)
{
    const auto entity = mRegistry->CreateEntity();
    EXPECT_FALSE(mRegistry->IsAncestorOf(entity, entity));
    EXPECT_FALSE(mRegistry->IsDescendantOf(entity, entity));
}

TEST_P(HierarchySpec, IsAncestorOfShouldBeFalseForNullEntity)
{
    const auto entity = mRegistry->CreateEntity();
    EXPECT_FALSE(mRegistry->IsAncestorOf(fr::NullEntity, entity));
    EXPECT_FALSE(mRegistry->IsAncestorOf(entity, fr::NullEntity));
    EXPECT_FALSE(mRegistry->IsDescendantOf(entity, fr::NullEntity));
    EXPECT_FALSE(mRegistry->IsDescendantOf(fr::NullEntity, entity));
}

// ── FindAncestorWith<T> ───────────────────────────────────────────────────────

TEST_P(HierarchySpec, FindAncestorWithShouldReturnDirectParentIfItHasComponent)
{
    const auto parent = mRegistry->CreateEntity();
    const auto child  = mRegistry->CreateEntity();
    ASSERT_TRUE(mRegistry->SetParent(child, parent));
    mRegistry->AddComponent<Marker>(parent);
    mRegistry->ExecuteTasks();

    EXPECT_EQ(mRegistry->FindAncestorWith<Marker>(child), parent);
}

TEST_P(HierarchySpec, FindAncestorWithShouldReturnGrandparentIfOnlyItHasComponent)
{
    const auto grandparent = mRegistry->CreateEntity();
    const auto parent      = mRegistry->CreateEntity();
    const auto child       = mRegistry->CreateEntity();
    ASSERT_TRUE(mRegistry->SetParent(parent, grandparent));
    ASSERT_TRUE(mRegistry->SetParent(child, parent));
    mRegistry->AddComponent<Marker>(grandparent);
    mRegistry->ExecuteTasks();

    EXPECT_EQ(mRegistry->FindAncestorWith<Marker>(child), grandparent);
}

TEST_P(HierarchySpec, FindAncestorWithShouldReturnNullEntityIfNoAncestorHasComponent)
{
    const auto root  = mRegistry->CreateEntity();
    const auto child = mRegistry->CreateEntity();
    ASSERT_TRUE(mRegistry->SetParent(child, root));

    EXPECT_EQ(mRegistry->FindAncestorWith<Marker>(child), fr::NullEntity);
}

TEST_P(HierarchySpec, FindAncestorWithShouldNotCheckEntityItself)
{
    const auto root  = mRegistry->CreateEntity();
    const auto child = mRegistry->CreateEntity();
    ASSERT_TRUE(mRegistry->SetParent(child, root));
    mRegistry->AddComponent<Marker>(child);
    mRegistry->ExecuteTasks();

    EXPECT_EQ(mRegistry->FindAncestorWith<Marker>(child), fr::NullEntity);
}

// ── ForEachDescendant filtered ────────────────────────────────────────────────

TEST_P(HierarchySpec, ForEachDescendantFilteredShouldSkipSubtreeWhenPredicateFalse)
{
    const auto root      = mRegistry->CreateEntity();
    const auto a         = mRegistry->CreateEntity();
    const auto b         = mRegistry->CreateEntity();
    const auto childOfA  = mRegistry->CreateEntity();
    const auto childOfB  = mRegistry->CreateEntity();
    ASSERT_TRUE(mRegistry->SetParent(a, root));
    ASSERT_TRUE(mRegistry->SetParent(b, root));
    ASSERT_TRUE(mRegistry->SetParent(childOfA, a));
    ASSERT_TRUE(mRegistry->SetParent(childOfB, b));

    std::vector<fr::Entity> visited;
    mRegistry->ForEachDescendant(
        root, [&](fr::Entity e) { return e != b; }, [&](fr::Entity e) { visited.push_back(e); });

    EXPECT_TRUE(std::ranges::find(visited, a) != visited.end());
    EXPECT_TRUE(std::ranges::find(visited, childOfA) != visited.end());
    EXPECT_FALSE(std::ranges::find(visited, b) != visited.end());
    EXPECT_FALSE(std::ranges::find(visited, childOfB) != visited.end());
}

TEST_P(HierarchySpec, ForEachDescendantFilteredShouldVisitInPreOrder)
{
    const auto root     = mRegistry->CreateEntity();
    const auto a        = mRegistry->CreateEntity();
    const auto childOfA = mRegistry->CreateEntity();
    ASSERT_TRUE(mRegistry->SetParent(a, root));
    ASSERT_TRUE(mRegistry->SetParent(childOfA, a));

    std::vector<fr::Entity> visited;
    mRegistry->ForEachDescendant(
        root, [](fr::Entity) { return true; }, [&](fr::Entity e) { visited.push_back(e); });

    ASSERT_EQ(visited.size(), 2u);
    const auto posA      = std::ranges::find(visited, a);
    const auto posChildA = std::ranges::find(visited, childOfA);
    EXPECT_LT(posA, posChildA);
}

// ── ForEachDescendantWithParent ───────────────────────────────────────────────

TEST_P(HierarchySpec, ForEachDescendantWithParentShouldPassCorrectParent)
{
    const auto root       = mRegistry->CreateEntity();
    const auto child      = mRegistry->CreateEntity();
    const auto grandchild = mRegistry->CreateEntity();
    ASSERT_TRUE(mRegistry->SetParent(child, root));
    ASSERT_TRUE(mRegistry->SetParent(grandchild, child));

    std::vector<std::pair<fr::Entity, fr::Entity>> pairs;
    mRegistry->ForEachDescendantWithParent(
        root, [&](fr::Entity c, fr::Entity p) { pairs.emplace_back(c, p); });

    ASSERT_EQ(pairs.size(), 2u);
    const auto findPair = [&](fr::Entity c, fr::Entity p) {
        return std::ranges::find_if(pairs, [c, p](const auto& kv) {
                   return kv.first == c && kv.second == p;
               }) != pairs.end();
    };
    EXPECT_TRUE(findPair(child, root));
    EXPECT_TRUE(findPair(grandchild, child));
}

TEST_P(HierarchySpec, ForEachDescendantWithParentShouldVisitParentBeforeChild)
{
    const auto root       = mRegistry->CreateEntity();
    const auto child      = mRegistry->CreateEntity();
    const auto grandchild = mRegistry->CreateEntity();
    ASSERT_TRUE(mRegistry->SetParent(child, root));
    ASSERT_TRUE(mRegistry->SetParent(grandchild, child));

    std::vector<fr::Entity> visited;
    mRegistry->ForEachDescendantWithParent(
        root, [&](fr::Entity c, fr::Entity) { visited.push_back(c); });

    ASSERT_EQ(visited.size(), 2u);
    const auto posChild      = std::ranges::find(visited, child);
    const auto posGrandchild = std::ranges::find(visited, grandchild);
    EXPECT_LT(posChild, posGrandchild);
}

// ── MoveSiblingBefore / MoveSiblingToIndex ────────────────────────────────────

TEST_P(HierarchySpec, MoveSiblingBeforeShouldReorderChildren)
{
    const auto parent = mRegistry->CreateEntity();
    const auto a      = mRegistry->CreateEntity();
    const auto b      = mRegistry->CreateEntity();
    const auto c      = mRegistry->CreateEntity();
    ASSERT_TRUE(mRegistry->SetParent(a, parent));
    ASSERT_TRUE(mRegistry->SetParent(b, parent));
    ASSERT_TRUE(mRegistry->SetParent(c, parent));

    mRegistry->MoveSiblingBefore(c, a);

    const std::vector<fr::Entity> children(mRegistry->Children(parent).begin(),
                                           mRegistry->Children(parent).end());
    ASSERT_EQ(children.size(), 3u);
    EXPECT_EQ(children[0], c);
    EXPECT_EQ(children[1], a);
    EXPECT_EQ(children[2], b);
}

TEST_P(HierarchySpec, MoveSiblingBeforeSelfShouldBeNoOp)
{
    const auto parent = mRegistry->CreateEntity();
    const auto a      = mRegistry->CreateEntity();
    const auto b      = mRegistry->CreateEntity();
    ASSERT_TRUE(mRegistry->SetParent(a, parent));
    ASSERT_TRUE(mRegistry->SetParent(b, parent));

    mRegistry->MoveSiblingBefore(a, a);

    const std::vector<fr::Entity> children(mRegistry->Children(parent).begin(),
                                           mRegistry->Children(parent).end());
    ASSERT_EQ(children.size(), 2u);
    EXPECT_EQ(children[0], a);
    EXPECT_EQ(children[1], b);
}

TEST_P(HierarchySpec, MoveSiblingToIndexZeroShouldMoveToFront)
{
    const auto parent = mRegistry->CreateEntity();
    const auto a      = mRegistry->CreateEntity();
    const auto b      = mRegistry->CreateEntity();
    const auto c      = mRegistry->CreateEntity();
    ASSERT_TRUE(mRegistry->SetParent(a, parent));
    ASSERT_TRUE(mRegistry->SetParent(b, parent));
    ASSERT_TRUE(mRegistry->SetParent(c, parent));

    mRegistry->MoveSiblingToIndex(c, 0);

    const std::vector<fr::Entity> children(mRegistry->Children(parent).begin(),
                                           mRegistry->Children(parent).end());
    ASSERT_EQ(children.size(), 3u);
    EXPECT_EQ(children[0], c);
    EXPECT_EQ(children[1], a);
    EXPECT_EQ(children[2], b);
}

TEST_P(HierarchySpec, MoveSiblingToIndexLastShouldMoveToEnd)
{
    const auto parent = mRegistry->CreateEntity();
    const auto a      = mRegistry->CreateEntity();
    const auto b      = mRegistry->CreateEntity();
    const auto c      = mRegistry->CreateEntity();
    ASSERT_TRUE(mRegistry->SetParent(a, parent));
    ASSERT_TRUE(mRegistry->SetParent(b, parent));
    ASSERT_TRUE(mRegistry->SetParent(c, parent));

    mRegistry->MoveSiblingToIndex(a, 2);

    const std::vector<fr::Entity> children(mRegistry->Children(parent).begin(),
                                           mRegistry->Children(parent).end());
    ASSERT_EQ(children.size(), 3u);
    EXPECT_EQ(children[0], b);
    EXPECT_EQ(children[1], c);
    EXPECT_EQ(children[2], a);
}
