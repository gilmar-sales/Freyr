#pragma once

namespace FREYR_NAMESPACE
{
    using SystemId = unsigned long;

    inline SystemId SystemCount = 0;

    class Scene;

    class System
    {
      public:
        explicit System(const Ref<Scene>& scene) : mScene(scene) {}
        virtual ~System() = default;

        virtual void PreUpdate(float deltaTime) {}
        virtual void Update(float deltaTime) {}
        virtual void PostUpdate(float deltaTime) {}

      protected:
        friend class SystemManager;
        friend class Scene;

        Ref<Scene> mScene;
    };

    template <typename T>
    concept IsSystem = std::is_base_of_v<System, T>;

    template <typename T>
        requires IsSystem<T>
    constexpr auto GetSystemId() -> SystemId
    {
        static auto id = SystemCount++;

        return id;
    }
} // namespace FREYR_NAMESPACE
