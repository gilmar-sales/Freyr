#pragma once

#include "Freyr/Base/TypeNameId.hpp"
#include "Freyr/Pch.hpp"

namespace FREYR_NAMESPACE
{
    /**
     * @brief Unique identifier type for systems.
     */
    using SystemId = unsigned long;

    [[nodiscard]] inline auto SystemCount() -> SystemId
    {
        return static_cast<SystemId>(TypeNameCount(TypeIdKind::System));
    }

    class Registry;

    /**
     * @brief Base class for all systems in the ECS.
     *
     * Systems contain the logic that operates on entities and their components.
     * Override lifecycle methods (PreUpdate, Update, PostUpdate) to define behavior.
     * Systems receive a skr::Arc to the Registry via constructor.
     */
    class System
    {
      public:
        /**
         * @brief Constructs the system with a reference to its registry.
         *
         * @param registry  Reference to the Registry this system belongs to
         */
        explicit System(const skr::Arc<Registry>& registry) : mRegistry(registry) {}

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
        skr::Arc<Registry> mRegistry;
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
     * @brief Returns a process-stable dense identifier for the given system type.
     *
     * @tparam T  System type (must satisfy IsSystem)
     * @return SystemId assigned from the process-global type-name registry
     *
     * @note Identity is keyed by refl::type_name<T>() so host, static libs, and plugins that
     *       share one Freyr copy observe the same id for the same type name.
     *       The function-local static only caches that lookup.
     */
    template <typename T>
        requires IsSystem<T>
    inline auto GetSystemId() -> SystemId
    {
        static const auto id =
            static_cast<SystemId>(RegisterTypeName(TypeIdKind::System, refl::type_name<T>()));
        return id;
    }
} // namespace FREYR_NAMESPACE
