#include "Freyr/Core/FreyrExtension.hpp"

namespace FREYR_NAMESPACE
{
    void FreyrExtension::ConfigureServices(skr::ServiceCollection& services)
    {
        for (auto& func : mServiceCollectionFunctions)
        {
            func(services);
        }

        services.AddSingleton<FreyrOptions>(mFreyrOptionsBuilder.Build());
        services.AddSingleton<EntityManager>();
        services.AddSingleton<ComponentManager>();
        services.AddSingleton<SystemManager>();
        services.AddSingleton<TaskManager>();
        services.AddSingleton<TaskCounter>();
        services.AddSingleton<EventManager>();
        services.AddSingleton<Scene>();
        services.AddTransient<Archetype>();
    }

    void FreyrExtension::UseServices(skr::ServiceProvider& serviceProvider)
    {
        const auto systemManager = serviceProvider.GetService<SystemManager>();

        for (const auto& config : mPipelineConfigs)
        {
            systemManager->RegisterPipeline(config.Name, config.Rate);
        }

        for (auto& func : mSystemManagerFunctions)
        {
            func(*systemManager);
        }

        const auto componentManager = serviceProvider.GetService<ComponentManager>();

        for (auto& func : mComponentManagerFunctions)
        {
            func(*componentManager);
        }
    }
} // namespace FREYR_NAMESPACE