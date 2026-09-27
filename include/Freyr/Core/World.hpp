#pragma once

#include "Freyr/Core/FreyrExtension.hpp"
#include "Freyr/Core/Registry.hpp"

#include <Skirnir/Skirnir.hpp>

#include <utility>

namespace FREYR_NAMESPACE
{
    namespace detail
    {
        class WorldApp : public skr::IApplication
        {
          public:
            explicit WorldApp(const skr::Arc<skr::ServiceProvider>& rootServiceProvider) :
                IApplication(rootServiceProvider)
            {
            }

            void Run() override {}
        };
    } // namespace detail

    class World
    {
      public:
        template <typename Configure>
        static World Create(Configure&& configure)
        {
            auto app = skr::ApplicationBuilder()
                           .WithExtension<FreyrExtension>(std::forward<Configure>(configure))
                           .template Build<detail::WorldApp>();

            World world;
            world.mApp      = std::move(app);
            world.mRegistry = world.mApp->GetRootServiceProvider()->GetService<::FREYR_NAMESPACE::Registry>();
            return world;
        }

        [[nodiscard]] skr::Arc<::FREYR_NAMESPACE::Registry> GetRegistry() const { return mRegistry; }

        [[nodiscard]] ::FREYR_NAMESPACE::Registry& Get() { return *mRegistry; }

        [[nodiscard]] const ::FREYR_NAMESPACE::Registry& Get() const { return *mRegistry; }

      private:
        skr::Arc<skr::IApplication>              mApp;
        skr::Arc<::FREYR_NAMESPACE::Registry>    mRegistry;
    };
} // namespace FREYR_NAMESPACE
