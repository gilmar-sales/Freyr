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
                builder.SetInitialCapacity(500'000).SetThreadCount(4);
            })
            .Build();

    scene->StartProfiling();

    auto archetype = scene->CreateArchetypeBuilder()
                         .WithDefault(Position {})
                         .WithEntities(20'000)
                         .Build();

    auto archetype2 =
        scene->CreateArchetypeBuilder()
            .WithDefault(Position {})
            .WithDefault(Velocity {})
            .WithEntities(20'000)
            .Build();

    for (auto i = 0; i < 10; i++)
        scene->Update(1.0);

    scene->EndProfiling();

    return 0;
}
