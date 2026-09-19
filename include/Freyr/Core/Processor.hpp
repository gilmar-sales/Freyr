#pragma once

namespace FREYR_NAMESPACE
{

    /**
     * @brief Provides low-level hardware abstraction and topology information.
     */
    class Processor
    {
      public:
        Processor() = delete;

        /**
         * @brief Signals the CPU that the thread is in a busy-wait loop.
         *
         * On x86/x64, executes the PAUSE instruction.
         * On ARM/AArch64, executes the YIELD instruction.
         * On unknown architectures, emits a compiler memory barrier as a fallback.
         */
        static void Pause() noexcept;

        /**
         * @brief Returns the count of physical processing cores.
         *
         * Excludes logical processors from Hyper-Threading / SMT.
         * Uses OS-level APIs and is architecture-agnostic.
         */
        static int GetPhysicalCoreCount() noexcept;
    };

} // namespace FREYR_NAMESPACE