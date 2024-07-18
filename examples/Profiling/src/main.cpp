#include <Freyr/Containers/Signature.hpp>
#include <Freyr/Freyr.hpp>

#include "components/velocity.hpp"
#include "systems/collision.hpp"
#include "systems/physics.hpp"

auto CreateSig()
{
    return fr::Signature {};
}

int main(int argc, char const* argv[])
{
    auto manager = fr::Scene(20'000'000);

    manager.RegisterComponent<Position>();
    manager.RegisterComponent<Velocity>();

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

    auto count = 0l;

    manager.StartTraceProfiling("Dynamic Signature");
    for (auto i = 0; i < 1'000'000; i++)
    {
        auto a = fr::Signature();
        auto b = fr::Signature();

        if (a.Match(b))
        {
            count++;
        }
    }
    manager.EndTraceProfiling();

    // manager.Update(1.0);

    return 0;
}
