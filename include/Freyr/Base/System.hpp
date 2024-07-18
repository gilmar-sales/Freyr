#pragma once

namespace FREYR_NAMESPACE
{
    class Scene;

    class System
    {
      public:
        virtual void PreStart() {}
        virtual void Start() {}
        virtual void PostStart() {}

        virtual void PreUpdate(float deltaTime) {}
        virtual void Update(float deltaTime) {}
        virtual void PostUpdate(float deltaTime) {}

        virtual void PreFixedUpdate(float deltaTime) {}
        virtual void FixedUpdate(float deltaTime) {}
        virtual void PostFixedUpdate(float deltaTime) {}

      protected:
        ~System() = default;
        friend class SystemManager;
        friend class Scene;

        Scene* mManager;
    };

    template <typename T>
    concept IsSystem = std::is_base_of_v<System, T>;

    using SystemId = unsigned long;

    inline SystemId SystemCount = 0;

    template <typename T>
        requires IsSystem<T>
    constexpr auto GetSystemId() -> SystemId
    {
        const static auto id = SystemCount++;

        return id;
    }
} // namespace FREYR_NAMESPACE
