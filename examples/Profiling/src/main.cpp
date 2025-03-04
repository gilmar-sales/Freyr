#include "components/velocity.hpp"
#include "systems/collision.hpp"
#include "systems/physics.hpp"

int main(int argc, char const* argv[])
{
    const auto scene =
        fr::SceneBuilder()
            .AddComponent<Position>()
            .AddComponent<Velocity>()
            .AddSystem<CollisionSystem>()
            .AddSystem<PhysicsSystem>()
            .WithOptions([](fr::FreyrOptionsBuilder& builder) {
                builder.SetMaxEntities(300'000).SetArchetypeChunkCapacity(
                    10000);
            })
            .Build();

    scene->StartProfiling();

    scene->StartTraceProfiling("Build Entity");
    auto archetype = scene->CreateArchetypeBuilder()
                         .WithDefault(Position {})
                         .WithEntities(100'000)
                         .Build();

    auto archetype2 =
        scene->CreateArchetypeBuilder()
            .WithDefault(Position {})
            .WithDefault(Velocity {})
            .WithEntities(100'000)
            .Build();

    scene->EndTraceProfiling();

    for (auto i = 0; i < 100; i++)
        scene->Update(1.0);

    scene->EndProfiling();

    return 0;
}
