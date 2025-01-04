#include <Freyr/Containers/Signature.hpp>
#include <Freyr/Freyr.hpp>

#include "components/velocity.hpp"
#include "systems/collision.hpp"
#include "systems/physics.hpp"

#include <boost/container/flat_map.hpp>
#include <boost/container/flat_set.hpp>

auto CreateSig()
{
    return fr::Signature {};
}

int main(int argc, char const* argv[])
{
    auto manager = std::make_shared<fr::Scene>(20'000'000);

    manager->RegisterComponent<Position>();
    manager->RegisterComponent<Velocity>();

    manager->StartTraceProfiling("Build Entity");
    auto archetype = manager->CreateArchetypeBuilder()
                         .WithDefault(Position {})
                         .WithEntities(100'000)
                         .Build();
    auto archetype2 =
        manager->CreateArchetypeBuilder()
            .WithDefault(Position {})
            .WithDefault(Velocity {})
            .WithEntities(100'000)
            .Build();

    manager->EndTraceProfiling();

    manager->RegisterSystem<CollisionSystem>();
    manager->RegisterSystem<PhysicsSystem>();

    for (auto i = 0; i < 100; i++)
        manager->Update(1.0);

    return 0;
}
