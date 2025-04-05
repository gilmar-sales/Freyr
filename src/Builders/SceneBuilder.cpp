#include "Freyr/Builders/SceneBuilder.hpp"

namespace FREYR_NAMESPACE
{
    Ref<Scene> SceneBuilder::Build(skr::ServiceProvider& serviceProvider)
    {
        mServiceCollection->AddSingleton(mFreyrOptionsBuilder.Build());
        mServiceCollection->AddSingleton<EntityManager>();
        mServiceCollection->AddSingleton<ComponentManager>();
        mServiceCollection->AddSingleton<SystemManager>();
        mServiceCollection->AddSingleton<TaskManager>();
        mServiceCollection->AddSingleton<EventManager>();

        mServiceCollection->AddTransient<Archetype>();

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

        auto scene = skr::MakeRef<Scene>(
            serviceProvider.GetService<skr::ServiceProvider>());

        if (!mServiceCollection->Contains<Scene>())
            mServiceCollection->AddSingleton(scene);

        return scene;
    }

    Ref<Scene> SceneBuilder::Build()
    {
        const auto provider = mServiceCollection->CreateServiceProvider();
        auto       scene    = Build(*provider);
        return scene;
    }
} // namespace FREYR_NAMESPACE