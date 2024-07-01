#include <Freyr/Freyr.hpp>

#include "components/velocity.hpp"
#include "systems/collision.hpp"
#include "systems/physics.hpp"

int main(int argc, char const* argv[])
{
    auto manager = fr::ECSManager(20'000'000);

    manager.RegisterComponent<Position>();
    manager.RegisterComponent<Velocity>();

    manager.StartTraceProfiling("Create Entity");
    for (auto i = 0; i < 10'000'000; i++)
    {
        auto entity = manager.CreateEntity();
        manager.AddComponent(entity, Position {});

        if (i % 2)
            manager.AddComponent(entity, Velocity {});
    }

    manager.EndTraceProfiling();

    manager.StartTraceProfiling("Build Entity");
    auto archetype = manager.CreateArchetypeBuilder()
                         .WithDefault(Position {})
                         .WithEntities(5'000'000)
                         .Build();
    auto archetype2 =
        manager.CreateArchetypeBuilder()
            .WithDefault(Position {})
            .WithDefault(Velocity {})
            .WithEntities(5'000'000)
            .Build();

    manager.EndTraceProfiling();

    manager.RegisterSystem<CollisionSystem>();
    manager.RegisterSystem<PhysicsSystem>();

    // manager.Update(1.0);

    return 0;
}
