#include <gtest/gtest.h>

#include "Freyr/Core/FreyrExtension.hpp"
#include "Freyr/Core/Registry.hpp"

#include "../Components/ModelComponent.hpp"
#include "../Components/NameComponent.hpp"
#include "../Components/PositionComponent.hpp"
#include "../Components/VelocityComponent.hpp"

class MutationApp : public skr::IApplication
{
  public:
    explicit MutationApp(const Ref<skr::ServiceProvider>& rootServiceProvider) :
        IApplication(rootServiceProvider)
    {
    }

    void Run() override {}
};

struct MutationSpec : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        mApp = skr::ApplicationBuilder()
                   .WithExtension<fr::FreyrExtension>([](fr::FreyrExtension& freyr) {
                       freyr.WithComponent<NameComponent>()
                           .WithComponent<PositionComponent>()
                           .WithComponent<ModelComponent>()
                           .WithComponent<VelocityComponent>();
                   })
                   .Build<MutationApp>();

        mRegistry = mApp->GetRootServiceProvider()->GetService<fr::Registry>();
    }

    Ref<MutationApp>  mApp;
    Ref<fr::Registry> mRegistry;
};
