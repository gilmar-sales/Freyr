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
} // namespace FREYR_NAMESPACE