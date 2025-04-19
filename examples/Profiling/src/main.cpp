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
                builder.SetMaxEntities(4'000'000)
                    .SetArchetypeChunkCapacity(1024 * 16)
                    .SetThreadCount(14);
            })
            .Build();

    scene->StartProfiling();

    scene->CreateArchetypeBuilder()
        .WithDefault(Position {})
        .WithEntities(2'000'000)
        .Build();

    scene->CreateArchetypeBuilder()
        .WithDefault(Position {})
        .WithDefault(Velocity {})
        .WithEntities(2'000'000)
        .Build();

    for (auto i = 0; i < 10; i++)
        scene->Update(1.0);

    scene->EndProfiling();

    return 0;
}
