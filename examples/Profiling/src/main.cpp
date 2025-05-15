#include "components/velocity.hpp"
#include "systems/collision.hpp"
#include "systems/physics.hpp"

#include <Freyr/Freyr.hpp>

class ProfilingApp : skr::IApplication
{
  public:
    ProfilingApp(const Ref<skr::ServiceProvider>& rootServiceProvider) :
        IApplication(rootServiceProvider)
    {
        mScene = rootServiceProvider->GetService<fr::Scene>();
    }

    void Run() override
    {
        mScene->StartProfiling();

        mScene->CreateArchetypeBuilder()
            .WithDefault(Position {})
            .WithEntities(2'000'000)
            .Build();

        mScene->CreateArchetypeBuilder()
            .WithDefault(Position {})
            .WithDefault(Velocity {})
            .WithEntities(2'000'000)
            .Build();

        for (auto i = 0; i < 10; i++)
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
            .AddExtension(
                fr::FreyrExtension()
                    .WithOptions([](fr::FreyrOptionsBuilder& builder) {
                        builder.SetMaxEntities(4'000'000)
                            .SetArchetypeChunkCapacity(8192)
                            .SetThreadCount(14);
                    })
                    .AddComponent<Position>()
                    .AddComponent<Velocity>()
                    .AddSystem<CollisionSystem>()
                    .AddSystem<PhysicsSystem>())
            .Build<ProfilingApp>();

    app->Run();

    return 0;
}
