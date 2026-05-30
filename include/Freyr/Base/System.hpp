#pragma once

namespace FREYR_NAMESPACE
{
    /**
     * @brief Unique identifier type for systems.
     */
    using SystemId = unsigned long;

    /**
     * @brief Counter for assigning unique IDs to system types.
     */
    inline SystemId SystemCount = 0;

    class Registry;

    /**
     * @brief Base class for all systems in the ECS.
     *
     * Systems contain the logic that operates on entities and their components.
     * Override lifecycle methods (PreUpdate, Update, PostUpdate) to define behavior.
     * Systems receive a weak reference to the Registry via constructor.
     */
    class System
    {
      public:
        /**
         * @brief Constructs the system with a reference to its registry.
         *
         * @param registry  Reference to the Registry this system belongs to
         */
        explicit System(const Ref<Registry>& registry) : mRegistry(registry) {}

        /**
         * @brief Virtual destructor for proper polymorphic cleanup.
         */
        virtual ~System() = default;

        /**
         * @brief Called before the main update phase of each pipeline execution.
         *
         * @param deltaTime  Time elapsed since last pipeline execution in seconds
         *
         * @note Override this to perform setup or early processing.
         */
        virtual void PreUpdate(float deltaTime) {}

        /**
         * @brief Called during the main update phase of each pipeline execution.
         *
         * @param deltaTime  Time elapsed since last pipeline execution in seconds
         *
         * @note Systems are executed according to pipeline stage ordering and rate settings.
         *       The pipeline may run at a fixed cadence independent of frame rate.
         */
        virtual void Update(float deltaTime) {}

        /**
         * @brief Called after the main update phase of each pipeline execution.
         *
         * @param deltaTime  Time elapsed since last pipeline execution in seconds
         *
         * @note Override this for cleanup or post-processing.
         */
        virtual void PostUpdate(float deltaTime) {}

      protected:
        friend class SystemManager;
        friend class Registry;

        /**
         * @brief Reference to the registry this system belongs to.
         */
        Ref<Registry> mRegistry;
    };

    /**
     * @brief Concept that verifies if a type is a valid system.
     *
     * @tparam T  Type to check
     *
     * A type satisfies IsSystem if it inherits from System.
     */
    template <typename T>
    concept IsSystem = std::is_base_of_v<System, T>;

    /**
     * @brief Returns a unique identifier for the given system type.
     *
     * @tparam T  System type (must satisfy IsSystem)
     * @return Unique SystemId assigned at first call (static storage)
     *
     * @note IDs are assigned at runtime in declaration order across translation units.
     */
    template <typename T>
        requires IsSystem<T>
    constexpr auto GetSystemId() -> SystemId
    {
        static auto id = SystemCount++;

        return id;
    }
} // namespace FREYR_NAMESPACE
