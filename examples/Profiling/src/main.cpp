#include <Freyr.hpp>

#include "systems/physics.hpp"
#include "systems/collision.hpp"

int main(int argc, char const *argv[])
{
    auto manager = fr::ECSManager(1000);

    manager.RegisterComponent<Position>();

    for(auto i = 0; i < 100; i++)
    {
        auto entity = manager.CreateEntity();
        manager.AddComponent(entity, Position{});
    }

    auto a = manager.RegisterSystem<CollisionSystem>();
    auto b = manager.RegisterSystem<PhysicsSystem>();

    manager.Update(1.0);

    return 0;
}
