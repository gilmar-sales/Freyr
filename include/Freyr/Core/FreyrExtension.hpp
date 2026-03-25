#pragma once

#include "Freyr/Builders/FreyrOptionsBuilder.hpp"
#include "Freyr/Core/Scene.hpp"
#include "Skirnir/ApplicationBuilder.hpp"

namespace FREYR_NAMESPACE
{
    class FreyrExtension : public skr::IExtension
    {
      public:
        void ConfigureServices(skr::ServiceCollection& services) override;
        void UseServices(skr::ServiceProvider& serviceProvider) override;

        template <typename T>
            requires IsSystem<T>
        FreyrExtension& WithSystem()
        {
            mSystemManagerFunctions.emplace_back(
                [this](SystemManager& systemManager) {
                    systemManager.RegisterSystem<T>();
                });

            mServiceCollectionFunctions.emplace_back(
                [](skr::ServiceCollection& services) {
                    services.AddSingleton<T>();
                });

            return *this;
        }

        template <typename T>
            requires IsComponent<T>
        FreyrExtension& WithSystem()
        {
            mComponentManagerFunctions.push_back(
                [this](ComponentManager& componenManager) {
                    componenManager.RegisterComponent<T>();
                });

            return *this;
        }

        FreyrExtension& WithOptions(
            const std::function<void(FreyrOptionsBuilder&)>& func)
        {
            func(mFreyrOptionsBuilder);

            return *this;
        }

      private:
        template <typename T>
        using Action = std::function<void(T&)>;

        std::vector<Action<skr::ServiceCollection>> mServiceCollectionFunctions;
        std::vector<Action<SystemManager>>          mSystemManagerFunctions;
        std::vector<Action<ComponentManager>>       mComponentManagerFunctions;

        FreyrOptionsBuilder mFreyrOptionsBuilder;
    };
} // namespace FREYR_NAMESPACE
