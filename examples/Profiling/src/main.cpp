#include "components/velocity.hpp"
#include "systems/collision.hpp"
#include "systems/physics.hpp"

#include <Freyr/Freyr.hpp>

class ProfilingApp : public skr::IApplication
{
  public:
    explicit ProfilingApp(const Ref<skr::ServiceProvider>& rootServiceProvider) : IApplication(rootServiceProvider)
    {
        mRegistry = rootServiceProvider->GetService<fr::Registry>();
    }

    void Run() override
    {
        mRegistry->BeginProfiling();

        mRegistry->CreateArchetypeBuilder().WithComponent(Position {}).WithEntities(2'000'000).Build();

        mRegistry->CreateArchetypeBuilder()
            .WithComponent(Position {})
            .WithComponent(Velocity {})
            .WithEntities(2'000'000)
            .Build();

        for (auto i = 0; i < 10; i++)
            mRegistry->Update(0.016f);

        mRegistry->EndProfiling();
    }

  private:
    Ref<fr::Registry> mRegistry;
};

int main(int argc, char const* argv[])
{
    const auto app =
        skr::ApplicationBuilder()
            .WithExtension<fr::FreyrExtension>([](fr::FreyrExtension& freyr) {
                freyr
                    .WithOptions([](fr::FreyrOptionsBuilder& builder) {
                        builder.WithMaxEntities(4 * 1024 * 1024).WithAllPhysicalCores();
                    })
                    .WithPipeline([](fr::PipelineBuilder& pipeline) {
                        pipeline.WithName("Main").WithSystem<CollisionSystem>().WithSystem<PhysicsSystem>();
                    })
                    .WithComponent<Position>()
                    .WithComponent<Velocity>();
            })
            .Build<ProfilingApp>();

    app->Run();

    return 0;
}
