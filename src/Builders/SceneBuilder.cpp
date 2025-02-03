#include "Freyr/Builders/SceneBuilder.hpp"

namespace FREYR_NAMESPACE
{
    std::shared_ptr<Scene> SceneBuilder::Build(ServiceProvider& serviceProvider)
    {
        mComponentManager->SetMaxEntities(mMaxEntities);

        return std::make_shared<Scene>(
            mMaxEntities, std::move(mSystemManager),
            std::move(mComponentManager),
            serviceProvider.GetService<ServiceProvider>());
    }
    std::shared_ptr<Scene> SceneBuilder::Build()
    {
        const auto provider = mServiceCollection->CreateServiceProvider();
        return Build(*provider);
    }
} // namespace FREYR_NAMESPACE