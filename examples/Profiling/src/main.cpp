#include <Freyr/Freyr.hpp>

#include "components/velocity.hpp"
#include "systems/collision.hpp"
#include "systems/physics.hpp"

int main(int argc, char const* argv[])
{
    auto manager = fr::ECSManager(1000);

    manager.RegisterComponent<Position>();
    manager.RegisterComponent<Velocity>();

    for (auto i = 0; i < 100; i++)
    {
        auto entity = manager.CreateEntity();
        manager.AddComponent(entity, Position {});

        if (i % 2)
            manager.AddComponent(entity, Velocity {});
    }

    manager.RegisterSystem<CollisionSystem>();
    manager.RegisterSystem<PhysicsSystem>();

    manager.Update(1.0);

    return 0;
}
