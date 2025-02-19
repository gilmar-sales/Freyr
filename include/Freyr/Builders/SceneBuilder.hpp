#pragma once

#include <memory>

#include "Freyr/Builders/FreyrOptionsBuilder.hpp"
#include "Freyr/Core/Scene.hpp"

namespace FREYR_NAMESPACE
{
    class Scene;
    class ComponentManager;
    class SystemManager;

    class SceneBuilder
    {
        template <typename T>
        using Action = std::function<void(T&)>;

      public:
        explicit SceneBuilder(
            const std::shared_ptr<ServiceCollection>& serviceCollection =
                std::make_shared<ServiceCollection>()) :
            mSystemManagerFunctions({}), mComponentManagerFunctions({}),
            mFreyrOptionsBuilder({}), mServiceCollection(serviceCollection)
        {
            mSystemManagerFunctions.reserve(1024);
            mComponentManagerFunctions.reserve(1024);
        }

        template <typename T>
            requires IsSystem<T>
        SceneBuilder& AddSystem()
        {
            mSystemManagerFunctions.emplace_back(
                [this](SystemManager& systemManager) {
                    systemManager.RegisterSystem<T>();
                });

            mServiceCollection->AddSingleton<T>();
            return *this;
        }

        template <typename T>
            requires IsComponent<T>
        SceneBuilder& AddComponent()
        {
            mComponentManagerFunctions.push_back(
                [this](ComponentManager& componenManager) {
                    componenManager.RegisterComponent<T>();
                });

            return *this;
        }

        SceneBuilder& WithOptions(
            const Action<FreyrOptionsBuilder>& configOptions)
        {
            configOptions(mFreyrOptionsBuilder);
            return *this;
        }

        std::shared_ptr<Scene> Build(ServiceProvider& serviceProvider);
        std::shared_ptr<Scene> Build();

      private:
        std::vector<Action<SystemManager>>    mSystemManagerFunctions;
        std::vector<Action<ComponentManager>> mComponentManagerFunctions;
        FreyrOptionsBuilder                   mFreyrOptionsBuilder;
        std::shared_ptr<ServiceCollection>    mServiceCollection;
    };
} // namespace FREYR_NAMESPACE
