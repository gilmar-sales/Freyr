#pragma once

#include "Freyr/Core/Scheduler.hpp"

namespace FREYR_NAMESPACE
{
    class Scene;

    class System
    {
      public:
        explicit System(const Ref<Scene>& scene) : mScene(scene) {}
        virtual ~System() = default;

        virtual void PreUpdate(float deltaTime, const Ref<Scheduler>& scheduler) {}
        virtual void Update(float deltaTime, const Ref<Scheduler>& scheduler) {}
        virtual void PostUpdate(float deltaTime, const Ref<Scheduler>& scheduler) {}

        virtual void PreFixedUpdate(float deltaTime, const Ref<Scheduler>& scheduler) {}
        virtual void FixedUpdate(float deltaTime, const Ref<Scheduler>& scheduler) {}
        virtual void PostFixedUpdate(float deltaTime, const Ref<Scheduler>& scheduler) {}

      protected:
        friend class SystemManager;
        friend class Scene;

        Ref<Scene> mScene;
    };

    template <typename T>
    concept IsSystem = std::is_base_of_v<System, T>;

    using SystemId = unsigned long;

    inline SystemId SystemCount = 0;

    template <typename T>
        requires IsSystem<T>
    constexpr auto GetSystemId() -> SystemId
    {
        static auto id = SystemCount++;

        return id;
    }
} // namespace FREYR_NAMESPACE