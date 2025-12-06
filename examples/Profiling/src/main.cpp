#include "components/velocity.hpp"
#include "systems/collision.hpp"
#include "systems/physics.hpp"

#include <Freyr/Freyr.hpp>

class ProfilingApp : public skr::IApplication
{
  public:
    ProfilingApp(const Ref<skr::ServiceProvider>& rootServiceProvider) : IApplication(rootServiceProvider)
    {
        mScene = rootServiceProvider->GetService<fr::Scene>();
    }

    void Run() override
    {
        mScene->BeginProfiling();

        mScene->CreateArchetypeBuilder().WithComponent(Position {}).WithEntities(2'000'000).Build();

        mScene->CreateArchetypeBuilder()
            .WithComponent(Position {})
            .WithComponent(Velocity {})
            .WithEntities(2'000'000)
            .Build();

        for (auto i = 0; i < 100; i++)
            mScene->Update(1.0);

        mScene->EndProfiling();
    }

  private:
    Ref<fr::Scene> mScene;
};

int main(int argc, char const* argv[])
{
    auto app =
        skr::ApplicationBuilder()
            .AddExtension<fr::FreyrExtension>([](fr::FreyrExtension& freyr) {
                freyr
                    .WithOptions([](fr::FreyrOptionsBuilder& builder) {
                        builder.SetMaxEntities(4'000'000).SetArchetypeChunkCapacity(32 * 1024).SetThreadCount(10);
                    })
                    .AddComponent<Position>()
                    .AddComponent<Velocity>()
                    .AddSystem<CollisionSystem>()
                    .AddSystem<PhysicsSystem>();
            })
            .Build<ProfilingApp>();

    app->Run();

    return 0;
}
