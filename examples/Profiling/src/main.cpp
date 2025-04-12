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
                builder.SetMaxEntities(500'000);
            })
            .Build();

    scene->StartProfiling();

    scene->StartTraceProfiling("Build Entity");
    auto archetype = scene->CreateArchetypeBuilder()
                         .WithDefault(Position {})
                         .WithEntities(200'000)
                         .Build();

    auto archetype2 =
        scene->CreateArchetypeBuilder()
            .WithDefault(Position {})
            .WithDefault(Velocity {})
            .WithEntities(200'000)
            .Build();

    scene->EndTraceProfiling();

    for (auto i = 0; i < 100; i++)
        scene->Update(1.0);

    scene->EndProfiling();

    return 0;
}
