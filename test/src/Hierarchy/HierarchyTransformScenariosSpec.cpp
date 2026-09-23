#include <Freyr/Freyr.hpp>
#include <Freyr/Hierarchy/Policies/Transform3DPolicy.hpp>
#include <gtest/gtest.h>

#include "../EmptyApp.hpp"

#include <array>
#include <cmath>
#include <numbers>
#include <random>
#include <vector>

namespace
{
    constexpr float kTolerance = 1e-4f;

    struct Vec3
    {
        float x = 0.f;
        float y = 0.f;
        float z = 0.f;
    };

    std::array<float, 4> AxisAngle(Vec3 axis, float radians)
    {
        const float s = std::sin(radians * 0.5f);
        return { axis.x * s, axis.y * s, axis.z * s, std::cos(radians * 0.5f) };
    }

    fr::Transform3D MakeTransform(Vec3                 position,
                                  std::array<float, 4> rotation = { 0.f, 0.f, 0.f, 1.f },
                                  Vec3                 scale    = { 1.f, 1.f, 1.f })
    {
        fr::Transform3D transform {};
        transform.position[0] = position.x;
        transform.position[1] = position.y;
        transform.position[2] = position.z;
        for (int i = 0; i < 4; ++i)
            transform.rotation[i] = rotation[i];
        transform.scale[0] = scale.x;
        transform.scale[1] = scale.y;
        transform.scale[2] = scale.z;
        return transform;
    }

    class HierarchyTransformScenariosSpec : public ::testing::Test
    {
      protected:
        void SetUp() override
        {
            mApp      = skr::ApplicationBuilder()
                            .WithExtension<fr::FreyrExtension>([](fr::FreyrExtension& freyr) {
                           freyr.WithHierarchyPropagation<fr::Transform3DPolicy>().WithOptions(
                               [](fr::FreyrOptionsBuilder& options) {
                                   options.WithMaxEntities(8192).WithThreadCount(4);
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

        fr::Entity Spawn(const fr::Transform3D& local, fr::Entity parent = fr::NullEntity)
        {
            const auto entity = mRegistry->CreateEntity(local, fr::WorldTransform3D {});
            if (parent != fr::NullEntity)
                EXPECT_TRUE(mRegistry->SetParent(entity, parent));
            return entity;
        }

        std::array<float, 16> World(fr::Entity entity)
        {
            std::array<float, 16> matrix {};
            EXPECT_TRUE(mRegistry->TryGetComponents<fr::WorldTransform3D>(
                entity, [&](fr::WorldTransform3D& world) {
                    for (int i = 0; i < 16; ++i)
                        matrix[i] = world.matrix[i];
                }));
            return matrix;
        }

        Vec3 WorldPosition(fr::Entity entity)
        {
            const auto matrix = World(entity);
            return { matrix[12], matrix[13], matrix[14] };
        }

        void EditLocal(fr::Entity entity, auto&& edit)
        {
            ASSERT_TRUE(mRegistry->TryGetComponents<fr::Transform3D>(entity, edit));
            mRegistry->MarkHierarchyDirty<fr::Transform3D>(entity);
        }

        void SetWorldPose(fr::Entity entity, const fr::Transform3D& worldPose)
        {
            float target[16];
            fr::ComposeMatrix(worldPose, target);

            fr::Transform3D local  = fr::DecomposeMatrix(target);
            const auto      parent = mRegistry->GetParent(entity);
            if (parent != fr::NullEntity)
                local = fr::LocalFromWorld(World(parent).data(), target);

            EditLocal(entity, [&](fr::Transform3D& current) {
                for (int i = 0; i < 3; ++i)
                {
                    current.position[i] = local.position[i];
                    current.scale[i]    = local.scale[i];
                }
                for (int i = 0; i < 4; ++i)
                    current.rotation[i] = local.rotation[i];
            });
        }

        bool ReparentKeepingWorld(fr::Entity entity, fr::Entity newParent)
        {
            const auto world = World(entity);
            if (!mRegistry->SetParent(entity, newParent))
                return false;
            const auto local = fr::LocalFromWorld(World(newParent).data(), world.data());
            EditLocal(entity, [&](fr::Transform3D& current) {
                for (int i = 0; i < 3; ++i)
                {
                    current.position[i] = local.position[i];
                    current.scale[i]    = local.scale[i];
                }
                for (int i = 0; i < 4; ++i)
                    current.rotation[i] = local.rotation[i];
            });
            return true;
        }

        void Frame()
        {
            mRegistry->ExecuteTasks();
            mRegistry->Update(0.016f);
        }

        skr::Arc<fr::Registry> mRegistry;
        skr::Arc<EmptyApp>     mApp;
    };

    void ExpectNear(Vec3 actual, Vec3 expected)
    {
        EXPECT_NEAR(actual.x, expected.x, kTolerance);
        EXPECT_NEAR(actual.y, expected.y, kTolerance);
        EXPECT_NEAR(actual.z, expected.z, kTolerance);
    }
} // namespace

TEST_F(HierarchyTransformScenariosSpec, CharacterHandShouldFollowRootMotionAndRootYaw)
{
    const auto root     = Spawn(MakeTransform({ 0.f, 0.f, 0.f }));
    const auto hips     = Spawn(MakeTransform({ 0.f, 1.f, 0.f }), root);
    const auto spine    = Spawn(MakeTransform({ 0.f, 0.5f, 0.f }), hips);
    const auto shoulder = Spawn(MakeTransform({ 0.2f, 0.3f, 0.f }), spine);
    const auto upperArm = Spawn(MakeTransform({ 0.3f, 0.f, 0.f }), shoulder);
    const auto hand     = Spawn(MakeTransform({ 0.3f, 0.f, 0.f }), upperArm);
    Frame();

    ExpectNear(WorldPosition(hand), { 0.8f, 1.8f, 0.f });

    EditLocal(root, [](fr::Transform3D& local) {
        local = MakeTransform({ 10.f, 0.f, 0.f },
                              AxisAngle({ 0.f, 1.f, 0.f }, std::numbers::pi_v<float> / 2.f));
    });
    Frame();

    ExpectNear(WorldPosition(hand), { 10.f, 1.8f, -0.8f });
    ExpectNear(WorldPosition(hips), { 10.f, 1.f, 0.f });
}

TEST_F(HierarchyTransformScenariosSpec, WeaponInHandSocketShouldInheritCharacterScale)
{
    const auto character =
        Spawn(MakeTransform({ 0.f, 0.f, 0.f }, { 0.f, 0.f, 0.f, 1.f }, { 2.f, 2.f, 2.f }));
    const auto socket = Spawn(MakeTransform({ 0.5f, 1.f, 0.f }), character);
    const auto weapon = Spawn(MakeTransform({ 0.f, 0.f, 0.25f }), socket);
    Frame();

    ExpectNear(WorldPosition(weapon), { 1.f, 2.f, 0.5f });

    const auto pose = fr::DecomposeMatrix(World(weapon).data());
    EXPECT_NEAR(pose.scale[0], 2.f, kTolerance);
    EXPECT_NEAR(pose.scale[1], 2.f, kTolerance);
    EXPECT_NEAR(pose.scale[2], 2.f, kTolerance);
}

TEST_F(HierarchyTransformScenariosSpec, SolarSystemMoonShouldComposeNestedOrbitRotations)
{
    const auto quarterTurn = AxisAngle({ 0.f, 1.f, 0.f }, std::numbers::pi_v<float> / 2.f);
    const auto sun         = Spawn(MakeTransform({ 0.f, 0.f, 0.f }, quarterTurn));
    const auto earth       = Spawn(MakeTransform({ 10.f, 0.f, 0.f }, quarterTurn), sun);
    const auto moon        = Spawn(MakeTransform({ 2.f, 0.f, 0.f }), earth);
    Frame();

    ExpectNear(WorldPosition(earth), { 0.f, 0.f, -10.f });
    ExpectNear(WorldPosition(moon), { -2.f, 0.f, -10.f });
}

TEST_F(HierarchyTransformScenariosSpec,
       MovingVehicleShouldCarryWheelsWithoutRecomputingParkedVehicle)
{
    const auto              movingChassis = Spawn(MakeTransform({ 0.f, 0.f, 0.f }));
    std::vector<fr::Entity> movingWheels;
    for (const Vec3 offset : { Vec3 { 1.f, -0.5f, 1.f }, Vec3 { -1.f, -0.5f, 1.f },
                               Vec3 { 1.f, -0.5f, -1.f }, Vec3 { -1.f, -0.5f, -1.f } })
        movingWheels.push_back(Spawn(MakeTransform(offset), movingChassis));

    const auto parkedChassis = Spawn(MakeTransform({ 50.f, 0.f, 0.f }));
    const auto parkedWheel   = Spawn(MakeTransform({ 1.f, -0.5f, 1.f }), parkedChassis);
    Frame();

    ASSERT_TRUE(mRegistry->TryGetComponents<fr::WorldTransform3D>(
        parkedWheel, [](fr::WorldTransform3D& world) { world.matrix[12] = -999.f; }));

    EditLocal(movingChassis, [](fr::Transform3D& local) { local.position[2] += 5.f; });
    Frame();

    ExpectNear(WorldPosition(movingWheels[0]), { 1.f, -0.5f, 6.f });
    ExpectNear(WorldPosition(movingWheels[3]), { -1.f, -0.5f, 4.f });
    EXPECT_FLOAT_EQ(WorldPosition(parkedWheel).x, -999.f);
}

TEST_F(HierarchyTransformScenariosSpec, PickingUpItemShouldKeepItsWorldPose)
{
    const auto character = Spawn(MakeTransform(
        { 2.f, 0.f, 0.f }, AxisAngle({ 0.f, 1.f, 0.f }, std::numbers::pi_v<float> / 2.f)));
    const auto hand      = Spawn(MakeTransform({ 0.5f, 1.f, 0.f }), character);
    const auto item = Spawn(MakeTransform({ 5.f, 1.f, 3.f }, AxisAngle({ 1.f, 0.f, 0.f }, 0.3f)));
    Frame();

    const auto before = World(item);

    ASSERT_TRUE(ReparentKeepingWorld(item, hand));
    Frame();

    const auto after = World(item);
    for (int i = 0; i < 16; ++i)
        EXPECT_NEAR(after[i], before[i], kTolerance) << i;

    EditLocal(character, [](fr::Transform3D& local) { local.position[0] += 1.f; });
    Frame();

    ExpectNear(WorldPosition(item), { before[12] + 1.f, before[13], before[14] });
}

TEST_F(HierarchyTransformScenariosSpec, DroppingItemShouldKeepWorldPoseAsNewRoot)
{
    const auto character =
        Spawn(MakeTransform({ 3.f, 0.f, -2.f }, AxisAngle({ 0.f, 1.f, 0.f }, 1.1f)));
    const auto hand = Spawn(MakeTransform({ 0.4f, 1.2f, 0.1f }), character);
    const auto item = Spawn(MakeTransform({ 0.f, 0.f, 0.3f }), hand);
    Frame();

    const auto before = World(item);
    const auto pose   = fr::DecomposeMatrix(before.data());

    ASSERT_TRUE(mRegistry->ClearParent(item));
    EditLocal(item, [&](fr::Transform3D& local) { local = pose; });
    Frame();

    const auto after = World(item);
    for (int i = 0; i < 16; ++i)
        EXPECT_NEAR(after[i], before[i], kTolerance) << i;
}

TEST_F(HierarchyTransformScenariosSpec,
       PhysicsWriteBackOnRotatedPlatformShouldLandOnRequestedWorldPose)
{
    const auto level    = Spawn(MakeTransform({ 0.f, 0.f, 0.f }));
    const auto platform = Spawn(
        MakeTransform({ 4.f, 2.f, 0.f }, AxisAngle({ 0.f, 0.f, 1.f }, 0.7f), { 1.5f, 1.5f, 1.5f }),
        level);
    const auto crate = Spawn(MakeTransform({ 0.f, 1.f, 0.f }), platform);
    Frame();

    const auto target =
        MakeTransform({ 6.f, 3.f, 1.f }, AxisAngle({ 0.f, 1.f, 0.f }, 0.4f), { 1.5f, 1.5f, 1.5f });
    SetWorldPose(crate, target);
    Frame();

    float expected[16];
    fr::ComposeMatrix(target, expected);
    const auto actual = World(crate);
    for (int i = 0; i < 16; ++i)
        EXPECT_NEAR(actual[i], expected[i], kTolerance) << i;
}

TEST_F(HierarchyTransformScenariosSpec, BoneChainDeeperThanSixtyFourShouldComposeEveryLink)
{
    constexpr int kBones = 100;
    fr::Entity    bone   = Spawn(MakeTransform({ 1.f, 0.f, 0.f }));
    for (int i = 1; i < kBones; ++i)
    {
        bone = Spawn(MakeTransform({ 1.f, 0.f, 0.f }), bone);
        if ((i % 32) == 0)
            mRegistry->ExecuteTasks();
    }
    Frame();

    EXPECT_EQ(mRegistry->GetDepth(bone), kBones - 1);
    ExpectNear(WorldPosition(bone), { static_cast<float>(kBones), 0.f, 0.f });
}

TEST_F(HierarchyTransformScenariosSpec, DestroyingCharacterShouldCascadeToAttachedProps)
{
    const auto character = Spawn(MakeTransform({ 0.f, 0.f, 0.f }));
    const auto socket    = Spawn(MakeTransform({ 0.5f, 1.f, 0.f }), character);
    const auto weapon    = Spawn(MakeTransform({ 0.f, 0.f, 0.3f }), socket);
    const auto light     = Spawn(MakeTransform({ 0.f, 0.f, 0.5f }), weapon);
    const auto bystander = Spawn(MakeTransform({ 10.f, 0.f, 0.f }));
    Frame();

    mRegistry->DestroyEntity(character);
    Frame();

    EXPECT_FALSE(mRegistry->IsAlive(character));
    EXPECT_FALSE(mRegistry->IsAlive(socket));
    EXPECT_FALSE(mRegistry->IsAlive(weapon));
    EXPECT_FALSE(mRegistry->IsAlive(light));
    EXPECT_TRUE(mRegistry->IsAlive(bystander));
    EXPECT_EQ(mRegistry->CreateQuery()->Count<fr::WorldTransform3D>(), 1u);
}

TEST_F(HierarchyTransformScenariosSpec,
       PropagatedWorldsShouldMatchOnDemandChainWalkAcrossAnimatedFrames)
{
    std::mt19937                          rng(7);
    std::uniform_real_distribution<float> offset(-2.f, 2.f);
    std::uniform_real_distribution<float> angle(-1.5f, 1.5f);
    std::uniform_real_distribution<float> scale(0.5f, 1.5f);

    auto randomTransform = [&] {
        return MakeTransform({ offset(rng), offset(rng), offset(rng) },
                             AxisAngle({ 0.f, 1.f, 0.f }, angle(rng)),
                             { scale(rng), scale(rng), scale(rng) });
    };

    std::vector<fr::Entity> entities;
    for (int character = 0; character < 8; ++character)
    {
        const auto root = Spawn(randomTransform());
        entities.push_back(root);
        for (int i = 0; i < 24; ++i)
        {
            const auto parent = entities[entities.size() - 1 - (rng() % (i + 1))];
            entities.push_back(Spawn(randomTransform(), parent));
        }
    }
    Frame();

    auto chainWalk = [&](fr::Entity entity) {
        std::vector<fr::Entity> chain;
        for (auto current = entity; current != fr::NullEntity;
             current      = mRegistry->GetParent(current))
            chain.push_back(current);

        std::array<float, 16> world { 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f,
                                      0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f };
        for (auto it = chain.rbegin(); it != chain.rend(); ++it)
        {
            float local[16];
            EXPECT_TRUE(mRegistry->TryGetComponents<fr::Transform3D>(
                *it, [&](fr::Transform3D& transform) { fr::ComposeMatrix(transform, local); }));
            fr::MultiplyMat4(world.data(), local, world.data());
        }
        return world;
    };

    for (int frame = 0; frame < 5; ++frame)
    {
        for (int edits = 0; edits < 10; ++edits)
        {
            const auto entity = entities[rng() % entities.size()];
            EditLocal(entity, [&](fr::Transform3D& local) { local = randomTransform(); });
        }
        Frame();

        for (const auto entity : entities)
        {
            const auto expected = chainWalk(entity);
            const auto actual   = World(entity);
            for (int i = 0; i < 16; ++i)
                ASSERT_NEAR(actual[i], expected[i], 1e-3f)
                    << "frame " << frame << " entity " << entity;
        }
    }
}

TEST_F(HierarchyTransformScenariosSpec, DecomposeShouldRoundTripTransformWithNegativeScale)
{
    const auto original =
        MakeTransform({ 1.f, 2.f, 3.f }, AxisAngle({ 0.f, 0.f, 1.f }, 0.9f), { -2.f, 1.f, 0.5f });
    float matrix[16];
    fr::ComposeMatrix(original, matrix);

    float rebuilt[16];
    fr::ComposeMatrix(fr::DecomposeMatrix(matrix), rebuilt);

    for (int i = 0; i < 16; ++i)
        EXPECT_NEAR(rebuilt[i], matrix[i], kTolerance) << i;
}
