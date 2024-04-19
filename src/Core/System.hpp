#pragma once

namespace FREYR_NAMESPACE
{
  class ECSManager;

  class System
  {
    public:
      virtual void PreStart() {}
      virtual void Start() {}
      virtual void PostStart() {}

      virtual void PreUpdate(float dt) {}
      virtual void Update(float dt) {}
      virtual void PostUpdate(float dt) {}

      virtual void PreFixedUpdate() {}
      virtual void FixedUpdate(float dt) {}
      virtual void PostFixedUpdate() {}

      virtual void PollEvents(float dt) {}

    protected:
      friend class SystemManager;
      friend class ECSManager;

      std::shared_ptr<ECSManager> mManager;
  };

    template<typename T>
    concept IsSystem = std::is_base_of_v<System, T>;

    using SystemId = unsigned long;

    inline SystemId SystemCount = 0;

    template<typename T>
        requires IsSystem<T>
    constexpr auto GetSystemId() -> SystemId
    {
        const static auto id = SystemCount++;

        return id;
    }

} // namespace FREYR_NAMESPACE
