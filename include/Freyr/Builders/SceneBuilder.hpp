#pragma once

#include <memory>

#include "Freyr/Core/Scene.hpp"

class ServiceCollection;

namespace FREYR_NAMESPACE
{
    class Scene;
    class ComponentManager;
    class SystemManager;

    class SceneBuilder
    {
      public:
        explicit SceneBuilder(
            const std::shared_ptr<ServiceCollection>& serviceCollection =
                std::make_shared<ServiceCollection>()) :
            mMaxEntities(10'000),
            mComponentManager(std::make_unique<ComponentManager>(1000)),
            mSystemManager(std::make_unique<SystemManager>(1024)),
            mServiceCollection(serviceCollection)
        {
        }

        template <typename T>
            requires IsSystem<T>
        SceneBuilder& AddSystem()
        {
            mSystemManager->RegisterSystem<T>();
            mServiceCollection->AddSingleton<T>();
            return *this;
        }

        template <typename T>
            requires IsComponent<T>
        SceneBuilder& AddComponent()
        {
            mComponentManager->RegisterComponent<T>();
            return *this;
        }

        SceneBuilder& SetMaxEntities(const Entity maxEntities)
        {
            mMaxEntities = maxEntities;
            return *this;
        }

        std::shared_ptr<Scene> Build(ServiceProvider& serviceProvider);

      private:
        Entity                             mMaxEntities;
        std::unique_ptr<ComponentManager>  mComponentManager;
        std::unique_ptr<SystemManager>     mSystemManager;
        std::shared_ptr<ServiceCollection> mServiceCollection;
    };
} // namespace FREYR_NAMESPACE
