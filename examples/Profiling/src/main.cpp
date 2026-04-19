#include "components/velocity.hpp"
#include "systems/collision.hpp"
#include "systems/physics.hpp"

#include <Freyr/Freyr.hpp>

class ProfilingApp : public skr::IApplication
{
  public:
    explicit ProfilingApp(const Ref<skr::ServiceProvider>& rootServiceProvider) : IApplication(rootServiceProvider)
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

        for (auto i = 0; i < 10; i++)
            mScene->Update(0.016f);

        mScene->EndProfiling();
    }

  private:
    Ref<fr::Scene> mScene;
};

int main(int argc, char const* argv[])
{
    auto app = skr::ApplicationBuilder()
                   .AddExtension<fr::FreyrExtension>([](fr::FreyrExtension& freyr) {
                       freyr
                           .WithOptions([](fr::FreyrOptionsBuilder& builder) {
                               builder.WithThreadCount(std::thread::hardware_concurrency() - 2);
                           })
                       .WithPipeline([](fr::PipelineBuilder& pipeline) {
                           pipeline
                           .WithName("Main")
                           .WithSystem<CollisionSystem>()
                           .WithSystem<PhysicsSystem>();
                       })
                           .WithComponent<Position>()
                           .WithComponent<Velocity>();
                   })
                   .Build<ProfilingApp>();

    app->Run();

    return 0;
}
