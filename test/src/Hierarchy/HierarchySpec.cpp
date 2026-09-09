#include <Freyr/Freyr.hpp>
#include <Freyr/Hierarchy/Policies/Affine2DTransformPolicy.hpp>
#include <Freyr/Hierarchy/Policies/Mat4TransformPolicy.hpp>
#include <gtest/gtest.h>

#include "../EmptyApp.hpp"

namespace
{
    class HierarchySpec : public ::testing::Test
    {
      protected:
        void SetUp() override
        {
            mApp = skr::ApplicationBuilder()
                       .WithExtension<fr::FreyrExtension>([](fr::FreyrExtension& freyr) {
                           freyr.WithHierarchy().WithOptions([](fr::FreyrOptionsBuilder& options) {
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

TEST_F(HierarchySpec, SetParentShouldUpdateChildOfParentDepthAndChildrenOrder)
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

    const auto children = mRegistry->Children(root);
    ASSERT_EQ(children.size(), 2u);
    EXPECT_EQ(children[0], child);
    EXPECT_EQ(children[1], mid);

    ASSERT_TRUE(mRegistry->SetParent(child, mid));
    mRegistry->ExecuteTasks();

    EXPECT_EQ(mRegistry->GetParent(child), mid);
    EXPECT_EQ(mRegistry->GetDepth(child), 2);
    EXPECT_EQ(mRegistry->Children(root).size(), 1u);
    EXPECT_EQ(mRegistry->Children(root)[0], mid);
    EXPECT_EQ(mRegistry->Children(mid).size(), 1u);
}

TEST_F(HierarchySpec, SetParentShouldRejectCycles)
{
    const auto a = mRegistry->CreateEntity();
    const auto b = mRegistry->CreateEntity();
    ASSERT_TRUE(mRegistry->SetParent(b, a));
    EXPECT_FALSE(mRegistry->SetParent(a, b));
    EXPECT_EQ(mRegistry->GetParent(a), fr::NullEntity);
}

TEST_F(HierarchySpec, ClearParentShouldDetachChild)
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

TEST_F(HierarchySpec, CascadeDestroyShouldDestroyDescendants)
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

TEST_F(HierarchySpec, DestroyLeafShouldNotAffectSibling)
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
    EXPECT_EQ(mRegistry->Children(root)[0], b);
    EXPECT_EQ(mRegistry->GetParent(b), root);
}

TEST_F(HierarchySpec, ChildrenOrderShouldStayStableWhenRemovingMiddle)
{
    const auto root = mRegistry->CreateEntity();
    const auto a    = mRegistry->CreateEntity();
    const auto b    = mRegistry->CreateEntity();
    const auto c    = mRegistry->CreateEntity();
    ASSERT_TRUE(mRegistry->SetParent(a, root));
    ASSERT_TRUE(mRegistry->SetParent(b, root));
    ASSERT_TRUE(mRegistry->SetParent(c, root));

    ASSERT_TRUE(mRegistry->ClearParent(b));

    const auto children = mRegistry->Children(root);
    ASSERT_EQ(children.size(), 2u);
    EXPECT_EQ(children[0], a);
    EXPECT_EQ(children[1], c);
}
