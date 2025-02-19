#include "Freyr/Builders/SceneBuilder.hpp"

namespace FREYR_NAMESPACE
{
    std::shared_ptr<Scene> SceneBuilder::Build(ServiceProvider& serviceProvider)
    {
        mServiceCollection->AddSingleton(mFreyrOptionsBuilder.Build());
        mServiceCollection->AddSingleton<EntityManager>();
        mServiceCollection->AddSingleton<ComponentManager>();
        mServiceCollection->AddSingleton<SystemManager>();
        mServiceCollection->AddSingleton<TaskManager>();
        mServiceCollection->AddSingleton<EventManager>();
        mServiceCollection->AddSingleton<Scene>();

        const auto componentManager =
            serviceProvider.GetService<ComponentManager>();

        for (auto& func : mComponentManagerFunctions)
        {
            func(*componentManager);
        }

        const auto systemManager = serviceProvider.GetService<SystemManager>();

        for (auto func : mSystemManagerFunctions)
        {
            func(*systemManager);
        }

        return serviceProvider.GetService<Scene>();
    }

    std::shared_ptr<Scene> SceneBuilder::Build()
    {
        const auto provider = mServiceCollection->CreateServiceProvider();
        auto       scene    = Build(*provider);
        return scene;
    }
} // namespace FREYR_NAMESPACE