#include <benchmark/benchmark.h>

#include "FriggaTransformUtil.hpp"
#include "Scenes.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace
{
    using namespace scenes;
    namespace TransformUtil = frigga::TransformUtil;

    constexpr float kDeltaTime = 0.016f;

    std::size_t CapacityFor(const SceneDesc& scene)
    {
        return scene.nodes.size() * 2 + 4'096;
    }

    void LabelScene(benchmark::State& state, const SceneDesc& scene)
    {
        state.SetLabel(std::to_string(scene.nodes.size()) + " ents");
    }

    float GatherFrigga(fr::Registry& registry)
    {
        float sink = 0.f;
        registry.CreateMutation()->Each(
            [&](fr::Entity entity, frigga::TransformComponent&, Renderable&) {
                Accumulate(sink, TransformUtil::WorldPose(registry, entity));
            });
        registry.CreateMutation()->Each(
            [&](fr::Entity entity, frigga::TransformComponent&, PointLight&) {
                Accumulate(sink, TransformUtil::WorldPose(registry, entity));
            });
        return sink;
    }

    float GatherFreyr(fr::Registry& registry)
    {
        float sink = 0.f;
        registry.CreateMutation()->Each([&](fr::WorldTransform3D& world, Renderable&) {
            Accumulate(sink, fr::DecomposeMatrix(world.matrix));
        });
        registry.CreateMutation()->Each([&](fr::WorldTransform3D& world, PointLight&) {
            Accumulate(sink, fr::DecomposeMatrix(world.matrix));
        });
        return sink;
    }

    template <bool MarkDirty>
    void AnimateCrowd(fr::Registry& registry, const std::vector<fr::Entity>& entities,
                      const SceneDesc& scene, std::int64_t frame)
    {
        const float time = static_cast<float>(frame) * kDeltaTime;
        for (std::size_t i = 0; i < scene.characters.size(); ++i)
        {
            const auto& character = scene.characters[i];
            const float yaw       = time + static_cast<float>(i);
            const auto  root      = entities[character.root];
            registry.TryGetComponents<fr::Transform3D>(root, [&](fr::Transform3D& local) {
                local.position[0] += std::sin(yaw) * 0.02f;
                local.position[2] += std::cos(yaw) * 0.02f;
                local.rotation[1] = std::sin(yaw * 0.5f);
                local.rotation[3] = std::cos(yaw * 0.5f);
            });
            registry.TryGetComponents<fr::Transform3D>(
                entities[character.rightSocket], [&](fr::Transform3D& local) {
                    local.rotation[0] = std::sin(yaw * 4.f) * 0.1f;
                    local.rotation[3] = std::sqrt(1.f - local.rotation[0] * local.rotation[0]);
                });
            if constexpr (MarkDirty)
                registry.MarkHierarchyDirty<fr::Transform3D>(root);
        }
    }

    std::vector<fr::Entity> PickMovers(const std::vector<fr::Entity>& entities,
                                       const SceneDesc& scene, std::size_t count)
    {
        std::vector<fr::Entity> movers;
        if (count == 0)
            return movers;
        const auto stride = std::max<std::size_t>(1, scene.bodies.size() / count);
        for (std::size_t i = 0; i < scene.bodies.size() && movers.size() < count; i += stride)
            movers.push_back(entities[scene.bodies[i]]);
        return movers;
    }

    template <bool MarkDirty>
    void AnimateMovers(fr::Registry& registry, const std::vector<fr::Entity>& movers,
                       std::int64_t frame)
    {
        const float offset = std::sin(static_cast<float>(frame) * kDeltaTime) * 0.01f;
        for (const auto mover : movers)
        {
            registry.TryGetComponents<fr::Transform3D>(mover, [&](fr::Transform3D& local) {
                local.position[1] += offset;
            });
            if constexpr (MarkDirty)
                registry.MarkHierarchyDirty<fr::Transform3D>(mover);
        }
    }

    struct BodyTarget
    {
        float position[3] = { 0.f, 0.f, 0.f };
        float rotation[4] = { 0.f, 0.f, 0.f, 1.f };
    };

    template <typename TPoseOf>
    std::vector<BodyTarget> CaptureBodyTargets(const std::vector<fr::Entity>& entities,
                                               const SceneDesc& scene, std::size_t capacity,
                                               TPoseOf&& poseOf)
    {
        std::vector<BodyTarget> targets(capacity);
        for (const auto index : scene.bodies)
        {
            const auto            entity = entities[index];
            const fr::Transform3D pose   = poseOf(entity);
            for (int i = 0; i < 3; ++i)
                targets[entity].position[i] = pose.position[i];
            for (int i = 0; i < 4; ++i)
                targets[entity].rotation[i] = pose.rotation[i];
        }
        return targets;
    }
} // namespace

static void BM_CrowdFrame_Frigga(benchmark::State& state)
{
    const auto scene    = BuildCrowd(static_cast<std::uint32_t>(state.range(0)));
    auto       app      = CreateFriggaApp(CapacityFor(scene));
    auto&      registry = *app->registry;
    const auto entities = LoadFrigga(registry, scene);

    std::int64_t frame = 0;
    for (auto _ : state)
    {
        AnimateCrowd<false>(registry, entities, scene, ++frame);
        registry.Update(kDeltaTime);
        benchmark::DoNotOptimize(GatherFrigga(registry));
    }

    LabelScene(state, scene);
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(scene.nodes.size()));
}

template <fr::HierarchyStorageMode Storage>
static void CrowdFrameFreyr(benchmark::State& state)
{
    const auto scene    = BuildCrowd(static_cast<std::uint32_t>(state.range(0)));
    auto       app      = CreateFreyrApp(CapacityFor(scene), Storage);
    auto&      registry = *app->registry;
    const auto entities = LoadFreyr(registry, scene);

    std::int64_t frame = 0;
    for (auto _ : state)
    {
        AnimateCrowd<true>(registry, entities, scene, ++frame);
        registry.Update(kDeltaTime);
        benchmark::DoNotOptimize(GatherFreyr(registry));
    }

    LabelScene(state, scene);
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(scene.nodes.size()));
}

static void BM_CityFrame_Frigga(benchmark::State& state)
{
    const auto scene    = BuildCity(static_cast<std::uint32_t>(state.range(0)));
    auto       app      = CreateFriggaApp(CapacityFor(scene));
    auto&      registry = *app->registry;
    const auto entities = LoadFrigga(registry, scene);
    const auto movers   = PickMovers(entities, scene, static_cast<std::size_t>(state.range(1)));

    std::int64_t frame = 0;
    for (auto _ : state)
    {
        AnimateMovers<false>(registry, movers, ++frame);
        registry.Update(kDeltaTime);
        benchmark::DoNotOptimize(GatherFrigga(registry));
    }

    LabelScene(state, scene);
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(scene.nodes.size()));
}

template <fr::HierarchyStorageMode Storage>
static void CityFrameFreyr(benchmark::State& state)
{
    const auto scene    = BuildCity(static_cast<std::uint32_t>(state.range(0)));
    auto       app      = CreateFreyrApp(CapacityFor(scene), Storage);
    auto&      registry = *app->registry;
    const auto entities = LoadFreyr(registry, scene);
    const auto movers   = PickMovers(entities, scene, static_cast<std::size_t>(state.range(1)));

    std::int64_t frame = 0;
    for (auto _ : state)
    {
        AnimateMovers<true>(registry, movers, ++frame);
        registry.Update(kDeltaTime);
        benchmark::DoNotOptimize(GatherFreyr(registry));
    }

    LabelScene(state, scene);
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(scene.nodes.size()));
}

static void BM_PhysicsWriteBack_Frigga(benchmark::State& state)
{
    const auto scene    = BuildCity(static_cast<std::uint32_t>(state.range(0)));
    const auto capacity = CapacityFor(scene);
    auto       app      = CreateFriggaApp(capacity);
    auto&      registry = *app->registry;
    const auto entities = LoadFrigga(registry, scene);
    auto       targets  = CaptureBodyTargets(entities, scene, capacity, [&](fr::Entity entity) {
        return TransformUtil::WorldPose(registry, entity);
    });

    std::int64_t frame = 0;
    for (auto _ : state)
    {
        const float offset = std::sin(static_cast<float>(++frame) * kDeltaTime) * 0.01f;
        registry.CreateMutation()->Each(
            [&](fr::Entity entity, frigga::TransformComponent&, RigidBody&) {
                auto& target = targets[entity];
                target.position[1] += offset;
                TransformUtil::SetWorldPose(registry, entity, target.position, target.rotation);
            });
        registry.Update(kDeltaTime);
    }

    LabelScene(state, scene);
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(scene.bodies.size()));
}

template <fr::HierarchyStorageMode Storage>
static void PhysicsWriteBackFreyr(benchmark::State& state)
{
    const auto scene    = BuildCity(static_cast<std::uint32_t>(state.range(0)));
    const auto capacity = CapacityFor(scene);
    auto       app      = CreateFreyrApp(capacity, Storage);
    auto&      registry = *app->registry;
    const auto entities = LoadFreyr(registry, scene);
    auto       targets  = CaptureBodyTargets(entities, scene, capacity, [&](fr::Entity entity) {
        fr::Transform3D pose {};
        registry.TryGetComponents<fr::WorldTransform3D>(entity, [&](fr::WorldTransform3D& world) {
            pose = fr::DecomposeMatrix(world.matrix);
        });
        return pose;
    });

    std::int64_t frame = 0;
    for (auto _ : state)
    {
        const float offset = std::sin(static_cast<float>(++frame) * kDeltaTime) * 0.01f;
        registry.CreateMutation()->Each([&](fr::Entity entity, fr::Transform3D& local,
                                            fr::WorldTransform3D& world, RigidBody&) {
            auto& target = targets[entity];
            target.position[1] += offset;

            fr::Transform3D pose = fr::DecomposeMatrix(world.matrix);
            for (int i = 0; i < 3; ++i)
                pose.position[i] = target.position[i];
            for (int i = 0; i < 4; ++i)
                pose.rotation[i] = target.rotation[i];
            float targetWorld[16];
            fr::ComposeMatrix(pose, targetWorld);

            registry.TryGetComponents<fr::WorldTransform3D>(
                registry.GetParent(entity), [&](fr::WorldTransform3D& parentWorld) {
                    const auto next = fr::LocalFromWorld(parentWorld.matrix, targetWorld);
                    for (int i = 0; i < 3; ++i)
                    {
                        local.position[i] = next.position[i];
                        local.scale[i]    = next.scale[i];
                    }
                    for (int i = 0; i < 4; ++i)
                        local.rotation[i] = next.rotation[i];
                });
            registry.MarkHierarchyDirty<fr::Transform3D>(entity);
        });
        registry.Update(kDeltaTime);
    }

    LabelScene(state, scene);
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(scene.bodies.size()));
}

static void BM_WeaponSwap_Frigga(benchmark::State& state)
{
    const auto scene    = BuildCrowd(static_cast<std::uint32_t>(state.range(0)));
    auto       app      = CreateFriggaApp(CapacityFor(scene));
    auto&      registry = *app->registry;
    const auto entities = LoadFrigga(registry, scene);

    bool toLeft = true;
    for (auto _ : state)
    {
        for (const auto& character : scene.characters)
        {
            const auto socket = entities[toLeft ? character.leftSocket : character.rightSocket];
            TransformUtil::SetParent(registry, entities[character.weapon], socket, true);
        }
        toLeft = !toLeft;
        registry.Update(kDeltaTime);
    }

    LabelScene(state, scene);
    state.SetItemsProcessed(
        state.iterations() * static_cast<std::int64_t>(scene.characters.size()));
}

template <fr::HierarchyStorageMode Storage>
static void WeaponSwapFreyr(benchmark::State& state)
{
    const auto scene    = BuildCrowd(static_cast<std::uint32_t>(state.range(0)));
    auto       app      = CreateFreyrApp(CapacityFor(scene), Storage);
    auto&      registry = *app->registry;
    const auto entities = LoadFreyr(registry, scene);

    bool toLeft = true;
    for (auto _ : state)
    {
        for (const auto& character : scene.characters)
        {
            const auto weapon = entities[character.weapon];
            const auto socket = entities[toLeft ? character.leftSocket : character.rightSocket];

            float world[16];
            registry.TryGetComponents<fr::WorldTransform3D>(
                weapon, [&](fr::WorldTransform3D& current) {
                    for (int i = 0; i < 16; ++i)
                        world[i] = current.matrix[i];
                });
            registry.SetParent(weapon, socket);
            registry.TryGetComponents<fr::WorldTransform3D>(
                socket, [&](fr::WorldTransform3D& parentWorld) {
                    const auto next = fr::LocalFromWorld(parentWorld.matrix, world);
                    registry.TryGetComponents<fr::Transform3D>(weapon, [&](fr::Transform3D& local) {
                        for (int i = 0; i < 3; ++i)
                        {
                            local.position[i] = next.position[i];
                            local.scale[i]    = next.scale[i];
                        }
                        for (int i = 0; i < 4; ++i)
                            local.rotation[i] = next.rotation[i];
                    });
                });
            registry.MarkHierarchyDirty<fr::Transform3D>(weapon);
        }
        toLeft = !toLeft;
        registry.Update(kDeltaTime);
    }

    LabelScene(state, scene);
    state.SetItemsProcessed(
        state.iterations() * static_cast<std::int64_t>(scene.characters.size()));
}

static void BM_InstantiateModel_Frigga(benchmark::State& state)
{
    const auto scene    = BuildCrowd(static_cast<std::uint32_t>(state.range(0)));
    auto       app      = CreateFriggaApp(CapacityFor(scene));
    auto&      registry = *app->registry;

    std::vector<fr::Entity> entities;
    entities.reserve(scene.nodes.size());
    for (auto _ : state)
    {
        entities.clear();
        for (const auto& node : scene.nodes)
        {
            const auto entity =
                CreateWithKind(registry, node.kind, node.local, frigga::NameComponent {});
            if (node.parent >= 0)
                TransformUtil::SetParent(registry, entity, entities[node.parent], false);
            entities.push_back(entity);
        }
        registry.ExecuteTasks();

        state.PauseTiming();
        for (const auto& character : scene.characters)
            TransformUtil::DestroySubtree(registry, entities[character.root]);
        state.ResumeTiming();
    }

    LabelScene(state, scene);
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(scene.nodes.size()));
}

template <fr::HierarchyStorageMode Storage>
static void InstantiateModelFreyr(benchmark::State& state)
{
    const auto scene    = BuildCrowd(static_cast<std::uint32_t>(state.range(0)));
    auto       app      = CreateFreyrApp(CapacityFor(scene), Storage);
    auto&      registry = *app->registry;

    std::vector<fr::Entity> entities;
    entities.reserve(scene.nodes.size());
    for (auto _ : state)
    {
        entities.clear();
        for (const auto& node : scene.nodes)
        {
            const auto entity =
                CreateWithKind(registry, node.kind, node.local, fr::WorldTransform3D {});
            if (node.parent >= 0)
                registry.SetParent(entity, entities[node.parent]);
            entities.push_back(entity);
        }
        registry.ExecuteTasks();

        state.PauseTiming();
        for (const auto& character : scene.characters)
            registry.DestroyEntity(entities[character.root]);
        registry.ExecuteTasks();
        state.ResumeTiming();
    }

    LabelScene(state, scene);
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(scene.nodes.size()));
}

static void BM_DestroyCharacters_Frigga(benchmark::State& state)
{
    const auto scene = BuildCrowd(static_cast<std::uint32_t>(state.range(0)));

    for (auto _ : state)
    {
        state.PauseTiming();
        auto       app      = CreateFriggaApp(CapacityFor(scene));
        auto&      registry = *app->registry;
        const auto entities = LoadFrigga(registry, scene);
        state.ResumeTiming();

        for (const auto& character : scene.characters)
            TransformUtil::DestroySubtree(registry, entities[character.root]);

        state.PauseTiming();
        app.reset();
        state.ResumeTiming();
    }

    LabelScene(state, scene);
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(scene.nodes.size()));
}

template <fr::HierarchyStorageMode Storage>
static void DestroyCharactersFreyr(benchmark::State& state)
{
    const auto scene = BuildCrowd(static_cast<std::uint32_t>(state.range(0)));

    for (auto _ : state)
    {
        state.PauseTiming();
        auto       app      = CreateFreyrApp(CapacityFor(scene), Storage);
        auto&      registry = *app->registry;
        const auto entities = LoadFreyr(registry, scene);
        state.ResumeTiming();

        for (const auto& character : scene.characters)
            registry.DestroyEntity(entities[character.root]);
        registry.ExecuteTasks();

        state.PauseTiming();
        app.reset();
        state.ResumeTiming();
    }

    LabelScene(state, scene);
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(scene.nodes.size()));
}

static void BM_SkinAncestorLookup_Frigga(benchmark::State& state)
{
    const auto scene    = BuildCrowd(static_cast<std::uint32_t>(state.range(0)));
    auto       app      = CreateFriggaApp(CapacityFor(scene));
    auto&      registry = *app->registry;
    LoadFrigga(registry, scene);

    for (auto _ : state)
    {
        std::uint64_t skinned = 0;
        registry.CreateMutation()->Each([&](fr::Entity entity, Renderable&) {
            for (auto current = entity; current != frigga::kInvalidEntity;
                 current      = TransformUtil::ParentOf(registry, current))
            {
                if (registry.TryGetComponents<Animator>(current, [&](Animator& animator) {
                        skinned += animator.boneOffset + 1;
                    }))
                    break;
            }
        });
        benchmark::DoNotOptimize(skinned);
    }

    LabelScene(state, scene);
}

template <fr::HierarchyStorageMode Storage>
static void SkinAncestorLookupFreyr(benchmark::State& state)
{
    const auto scene    = BuildCrowd(static_cast<std::uint32_t>(state.range(0)));
    auto       app      = CreateFreyrApp(CapacityFor(scene), Storage);
    auto&      registry = *app->registry;
    LoadFreyr(registry, scene);

    for (auto _ : state)
    {
        std::uint64_t skinned = 0;
        registry.CreateMutation()->Each([&](fr::Entity entity, Renderable&) {
            for (auto current = entity; current != fr::NullEntity;
                 current      = registry.GetParent(current))
            {
                if (registry.TryGetComponents<Animator>(current, [&](Animator& animator) {
                        skinned += animator.boneOffset + 1;
                    }))
                    break;
            }
        });
        benchmark::DoNotOptimize(skinned);
    }

    LabelScene(state, scene);
}

BENCHMARK(BM_CrowdFrame_Frigga)->Arg(100)->Arg(1'000)->Arg(5'000)->Unit(benchmark::kMillisecond);
static void BM_CrowdFrame_Freyr(benchmark::State& state)
{
    CrowdFrameFreyr<fr::HierarchyStorageMode::Dense>(state);
}

static void BM_CrowdFrame_FreyrSparse(benchmark::State& state)
{
    CrowdFrameFreyr<fr::HierarchyStorageMode::Sparse>(state);
}

BENCHMARK(BM_CrowdFrame_Freyr)->Arg(100)->Arg(1'000)->Arg(5'000)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_CrowdFrame_FreyrSparse)->Arg(100)->Arg(1'000)->Arg(5'000)->Unit(benchmark::kMillisecond);

BENCHMARK(BM_CityFrame_Frigga)
    ->ArgNames({ "buildings", "movers" })
    ->Args({ 50, 0 })
    ->Args({ 50, 16 })
    ->Args({ 400, 0 })
    ->Args({ 400, 16 })
    ->Args({ 400, 1'024 })
    ->Unit(benchmark::kMillisecond);
static void BM_CityFrame_Freyr(benchmark::State& state)
{
    CityFrameFreyr<fr::HierarchyStorageMode::Dense>(state);
}

static void BM_CityFrame_FreyrSparse(benchmark::State& state)
{
    CityFrameFreyr<fr::HierarchyStorageMode::Sparse>(state);
}

BENCHMARK(BM_CityFrame_Freyr)
    ->ArgNames({ "buildings", "movers" })
    ->Args({ 50, 0 })
    ->Args({ 50, 16 })
    ->Args({ 400, 0 })
    ->Args({ 400, 16 })
    ->Args({ 400, 1'024 })
    ->Unit(benchmark::kMillisecond);
BENCHMARK(BM_CityFrame_FreyrSparse)
    ->ArgNames({ "buildings", "movers" })
    ->Args({ 50, 0 })
    ->Args({ 50, 16 })
    ->Args({ 400, 0 })
    ->Args({ 400, 16 })
    ->Args({ 400, 1'024 })
    ->Unit(benchmark::kMillisecond);

BENCHMARK(BM_PhysicsWriteBack_Frigga)
    ->ArgName("buildings")
    ->Arg(50)
    ->Arg(400)
    ->Unit(benchmark::kMillisecond);
static void BM_PhysicsWriteBack_Freyr(benchmark::State& state)
{
    PhysicsWriteBackFreyr<fr::HierarchyStorageMode::Dense>(state);
}

static void BM_PhysicsWriteBack_FreyrSparse(benchmark::State& state)
{
    PhysicsWriteBackFreyr<fr::HierarchyStorageMode::Sparse>(state);
}

BENCHMARK(BM_PhysicsWriteBack_Freyr)
    ->ArgName("buildings")
    ->Arg(50)
    ->Arg(400)
    ->Unit(benchmark::kMillisecond);
BENCHMARK(BM_PhysicsWriteBack_FreyrSparse)
    ->ArgName("buildings")
    ->Arg(50)
    ->Arg(400)
    ->Unit(benchmark::kMillisecond);

BENCHMARK(BM_WeaponSwap_Frigga)->Arg(100)->Arg(1'000)->Unit(benchmark::kMillisecond);
static void BM_WeaponSwap_Freyr(benchmark::State& state)
{
    WeaponSwapFreyr<fr::HierarchyStorageMode::Dense>(state);
}

static void BM_WeaponSwap_FreyrSparse(benchmark::State& state)
{
    WeaponSwapFreyr<fr::HierarchyStorageMode::Sparse>(state);
}

BENCHMARK(BM_WeaponSwap_Freyr)->Arg(100)->Arg(1'000)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_WeaponSwap_FreyrSparse)->Arg(100)->Arg(1'000)->Unit(benchmark::kMillisecond);

BENCHMARK(BM_InstantiateModel_Frigga)->Arg(1)->Arg(16)->Arg(128)->Unit(benchmark::kMicrosecond);
static void BM_InstantiateModel_Freyr(benchmark::State& state)
{
    InstantiateModelFreyr<fr::HierarchyStorageMode::Dense>(state);
}

static void BM_InstantiateModel_FreyrSparse(benchmark::State& state)
{
    InstantiateModelFreyr<fr::HierarchyStorageMode::Sparse>(state);
}

BENCHMARK(BM_InstantiateModel_Freyr)->Arg(1)->Arg(16)->Arg(128)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_InstantiateModel_FreyrSparse)->Arg(1)->Arg(16)->Arg(128)->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_DestroyCharacters_Frigga)->Arg(100)->Arg(1'000)->Unit(benchmark::kMillisecond);
static void BM_DestroyCharacters_Freyr(benchmark::State& state)
{
    DestroyCharactersFreyr<fr::HierarchyStorageMode::Dense>(state);
}

static void BM_DestroyCharacters_FreyrSparse(benchmark::State& state)
{
    DestroyCharactersFreyr<fr::HierarchyStorageMode::Sparse>(state);
}

BENCHMARK(BM_DestroyCharacters_Freyr)->Arg(100)->Arg(1'000)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_DestroyCharacters_FreyrSparse)->Arg(100)->Arg(1'000)->Unit(benchmark::kMillisecond);

BENCHMARK(BM_SkinAncestorLookup_Frigga)->Arg(1'000)->Arg(5'000)->Unit(benchmark::kMillisecond);
static void BM_SkinAncestorLookup_Freyr(benchmark::State& state)
{
    SkinAncestorLookupFreyr<fr::HierarchyStorageMode::Dense>(state);
}

static void BM_SkinAncestorLookup_FreyrSparse(benchmark::State& state)
{
    SkinAncestorLookupFreyr<fr::HierarchyStorageMode::Sparse>(state);
}

BENCHMARK(BM_SkinAncestorLookup_Freyr)->Arg(1'000)->Arg(5'000)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_SkinAncestorLookup_FreyrSparse)->Arg(1'000)->Arg(5'000)->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();
