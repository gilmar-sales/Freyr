#include "Freyr/Builders/SceneBuilder.hpp"

namespace FREYR_NAMESPACE
{
    std::shared_ptr<Scene> SceneBuilder::Build()
    {
        mComponentManager->SetMaxEntities(mMaxEntities);
        return std::make_shared<Scene>(
            mMaxEntities, std::move(mSystemManager),
            std::move(mComponentManager), mServiceCollection);
    }
} // namespace FREYR_NAMESPACE