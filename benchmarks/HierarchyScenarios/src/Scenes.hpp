#pragma once

#include "FriggaTransformUtil.hpp"

#include <Freyr/Freyr.hpp>
#include <Freyr/Hierarchy/Policies/Transform3DPolicy.hpp>

#include <cmath>
#include <cstdint>
#include <random>
#include <vector>

namespace scenes
{
    struct Renderable : fr::Component
    {
        std::uint32_t meshId = 0;
    };

    struct PointLight : fr::Component
    {
        float intensity = 1.f;
    };

    struct Animator : fr::Component
    {
        std::uint32_t boneOffset = 0;
    };

    struct RigidBody : fr::Component
    {
        std::uint32_t bodyId = 0;
    };

    enum class Kind : std::uint8_t
    {
        Node,
        Renderable,
        Light,
        Animator,
        Body
    };

    struct NodeDesc
    {
        fr::Transform3D local;
        std::int32_t    parent = -1;
        Kind            kind   = Kind::Node;
    };

    struct CharacterDesc
    {
        std::int32_t root        = -1;
        std::int32_t rightSocket = -1;
        std::int32_t leftSocket  = -1;
        std::int32_t weapon      = -1;
    };

    struct SceneDesc
    {
        std::vector<NodeDesc>      nodes;
        std::vector<CharacterDesc> characters;
        std::vector<std::int32_t>  bodies;

        std::int32_t Add(const fr::Transform3D& local, std::int32_t parent, Kind kind)
        {
            nodes.push_back({ .local = local, .parent = parent, .kind = kind });
            return static_cast<std::int32_t>(nodes.size() - 1);
        }
    };

    inline fr::Transform3D MakeTransform(float x, float y, float z, float yaw = 0.f,
                                         float scale = 1.f)
    {
        fr::Transform3D transform {};
        transform.position[0] = x;
        transform.position[1] = y;
        transform.position[2] = z;
        transform.rotation[1] = std::sin(yaw * 0.5f);
        transform.rotation[3] = std::cos(yaw * 0.5f);
        transform.scale[0] = transform.scale[1] = transform.scale[2] = scale;
        return transform;
    }

    inline CharacterDesc AddCharacter(SceneDesc& scene, float x, float z, float yaw)
    {
        CharacterDesc character {};
        character.root = scene.Add(MakeTransform(x, 0.f, z, yaw), -1, Kind::Animator);

        const auto armature =
            scene.Add(MakeTransform(0.f, 0.f, 0.f, 0.f, 0.98f), character.root, Kind::Node);
        scene.Add(MakeTransform(0.f, 0.9f, 0.f), armature, Kind::Renderable);
        scene.Add(MakeTransform(0.f, 1.65f, 0.f), armature, Kind::Renderable);

        character.rightSocket =
            scene.Add(MakeTransform(0.45f, 1.1f, 0.1f, 0.2f), armature, Kind::Node);
        character.weapon =
            scene.Add(MakeTransform(0.f, 0.f, 0.35f), character.rightSocket, Kind::Renderable);
        scene.Add(MakeTransform(0.f, 0.05f, 0.6f), character.weapon, Kind::Light);

        character.leftSocket =
            scene.Add(MakeTransform(-0.45f, 1.1f, 0.1f, -0.2f), armature, Kind::Node);
        scene.Add(MakeTransform(-0.05f, 0.f, 0.f), character.leftSocket, Kind::Renderable);

        const auto headSocket = scene.Add(MakeTransform(0.f, 1.8f, 0.f), armature, Kind::Node);
        const auto hat = scene.Add(MakeTransform(0.f, 0.1f, 0.f), headSocket, Kind::Renderable);
        scene.Add(MakeTransform(0.08f, 0.12f, -0.05f, 0.4f), hat, Kind::Renderable);

        scene.Add(MakeTransform(0.f, 1.6f, 0.f), character.root, Kind::Node);
        scene.Add(MakeTransform(0.f, 2.2f, -3.5f), character.root, Kind::Node);

        scene.characters.push_back(character);
        return character;
    }

    inline SceneDesc BuildCrowd(std::uint32_t characters, std::uint32_t seed = 42)
    {
        std::mt19937                          rng(seed);
        std::uniform_real_distribution<float> yaw(-3.14f, 3.14f);

        SceneDesc scene;
        scene.nodes.reserve(characters * 14u);
        for (std::uint32_t i = 0; i < characters; ++i)
            AddCharacter(scene, static_cast<float>(i % 100u) * 2.f,
                         static_cast<float>(i / 100u) * 2.f, yaw(rng));
        return scene;
    }

    inline SceneDesc BuildCity(std::uint32_t buildings, std::uint32_t seed = 42)
    {
        std::mt19937                          rng(seed);
        std::uniform_real_distribution<float> yaw(-3.14f, 3.14f);
        std::uniform_real_distribution<float> spread(-4.f, 4.f);

        SceneDesc scene;
        scene.nodes.reserve(buildings * 133u);
        for (std::uint32_t b = 0; b < buildings; ++b)
        {
            const auto building =
                scene.Add(MakeTransform(static_cast<float>(b % 20u) * 30.f, 0.f,
                                        static_cast<float>(b / 20u) * 30.f, yaw(rng)),
                          -1, Kind::Node);
            for (int f = 0; f < 4; ++f)
            {
                const auto floor = scene.Add(
                    MakeTransform(0.f, static_cast<float>(f) * 3.f, 0.f), building, Kind::Node);
                for (int r = 0; r < 4; ++r)
                {
                    const auto room =
                        scene.Add(MakeTransform((r % 2) * 10.f - 5.f, 0.f, (r / 2) * 10.f - 5.f,
                                                static_cast<float>(r) * 1.5708f),
                                  floor, Kind::Node);
                    for (int p = 0; p < 5; ++p)
                        scene.bodies.push_back(
                            scene.Add(MakeTransform(spread(rng), 0.5f, spread(rng), yaw(rng)), room,
                                      Kind::Body));
                    const auto lamp =
                        scene.Add(MakeTransform(0.f, 2.6f, 0.f), room, Kind::Renderable);
                    scene.Add(MakeTransform(0.f, -0.2f, 0.f), lamp, Kind::Light);
                }
            }
        }
        return scene;
    }

    template <typename... Base>
    fr::Entity CreateWithKind(fr::Registry& registry, Kind kind, const Base&... base)
    {
        switch (kind)
        {
            case Kind::Renderable:
                return registry.CreateEntity(base..., Renderable {});
            case Kind::Light:
                return registry.CreateEntity(base..., PointLight {});
            case Kind::Animator:
                return registry.CreateEntity(base..., Animator {});
            case Kind::Body:
                return registry.CreateEntity(base..., Renderable {}, RigidBody {});
            case Kind::Node:
                break;
        }
        return registry.CreateEntity(base...);
    }

    class BenchApp : public skr::IApplication
    {
      public:
        explicit BenchApp(const skr::Arc<skr::ServiceProvider>& rootServiceProvider) :
            IApplication(rootServiceProvider)
        {
            registry = rootServiceProvider->GetService<fr::Registry>();
        }

        void Run() override {}

        skr::Arc<fr::Registry> registry;
    };

    inline void WithSceneComponents(fr::FreyrExtension& freyr, std::size_t maxEntities,
                                    fr::HierarchyStorageMode storage = fr::HierarchyStorageMode::Dense)
    {
        freyr.WithComponent<Renderable>()
            .WithComponent<PointLight>()
            .WithComponent<Animator>()
            .WithComponent<RigidBody>()
            .WithOptions([maxEntities, storage](fr::FreyrOptionsBuilder& options) {
                options.WithMaxEntities(maxEntities)
                    .WithArchetypeChunkCapacity(512)
                    .WithThreadCount(4)
                    .WithHierarchyStorage(storage);
            });
    }

    inline skr::Arc<BenchApp> CreateFriggaApp(std::size_t maxEntities)
    {
        return skr::ApplicationBuilder()
            .WithExtension<fr::FreyrExtension>([maxEntities](fr::FreyrExtension& freyr) {
                freyr.WithComponent<frigga::TransformComponent>()
                    .WithComponent<frigga::HierarchyComponent>()
                    .WithComponent<frigga::NameComponent>();
                WithSceneComponents(freyr, maxEntities);
            })
            .Build<BenchApp>();
    }

    inline skr::Arc<BenchApp> CreateFreyrApp(
        std::size_t maxEntities, fr::HierarchyStorageMode storage = fr::HierarchyStorageMode::Dense)
    {
        return skr::ApplicationBuilder()
            .WithExtension<fr::FreyrExtension>([maxEntities, storage](fr::FreyrExtension& freyr) {
                freyr.WithHierarchyPropagation<fr::Transform3DPolicy>();
                WithSceneComponents(freyr, maxEntities, storage);
            })
            .Build<BenchApp>();
    }

    inline std::vector<fr::Entity> LoadFrigga(fr::Registry& registry, const SceneDesc& scene)
    {
        std::vector<fr::Entity> entities;
        entities.reserve(scene.nodes.size());
        for (std::size_t i = 0; i < scene.nodes.size(); ++i)
        {
            const auto& node   = scene.nodes[i];
            const auto  parent = node.parent < 0 ? frigga::kInvalidEntity : entities[node.parent];
            entities.push_back(
                CreateWithKind(registry, node.kind, node.local, frigga::NameComponent {},
                               frigga::HierarchyComponent { .parent = parent }));
            if ((i % 256u) == 0u)
                registry.ExecuteTasks();
        }
        registry.ExecuteTasks();

        for (std::size_t i = 0; i < scene.nodes.size(); ++i)
        {
            const auto parent = scene.nodes[i].parent;
            if (parent < 0)
                continue;
            registry.TryGetComponents<frigga::HierarchyComponent>(
                entities[parent],
                [&](frigga::HierarchyComponent& hierarchy) {
                    hierarchy.children.push_back(entities[i]);
                });
        }
        return entities;
    }

    inline std::vector<fr::Entity> LoadFreyr(fr::Registry& registry, const SceneDesc& scene)
    {
        std::vector<fr::Entity> entities;
        entities.reserve(scene.nodes.size());
        for (std::size_t i = 0; i < scene.nodes.size(); ++i)
        {
            const auto& node = scene.nodes[i];
            const auto  entity =
                CreateWithKind(registry, node.kind, node.local, fr::WorldTransform3D {});
            if (node.parent >= 0)
                registry.SetParent(entity, entities[node.parent]);
            entities.push_back(entity);
            if ((i % 256u) == 0u)
                registry.ExecuteTasks();
        }
        registry.ExecuteTasks();
        registry.Update(0.016f);
        return entities;
    }

    inline void Accumulate(float& sink, const fr::Transform3D& pose)
    {
        sink += pose.position[0] + pose.position[1] + pose.position[2] + pose.rotation[3] +
                pose.scale[0];
    }
} // namespace scenes
