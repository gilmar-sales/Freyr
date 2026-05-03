#pragma once

#if defined(_WIN32)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
    #if !defined(_M_ARM) && !defined(_M_ARM64)
        #include <intrin.h> // x86/x64 only: _mm_pause, __cpuid, etc.
    #endif

#elif defined(__APPLE__)
    #include <sys/sysctl.h>
    #if !defined(__arm__) && !defined(__aarch64__)
        #include <immintrin.h> // x86/x64 only
    #endif

#else // Linux / BSD / other POSIX
    #include <fstream>
    #include <set>
    #include <string>
    #include <unistd.h>
    #if !defined(__arm__) && !defined(__aarch64__)
        #include <immintrin.h> // x86/x64 only
    #endif
#endif

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
        static inline void Pause() noexcept
        {
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
            _mm_pause();
#elif defined(__aarch64__) || defined(_M_ARM64) || defined(__arm__) || defined(_M_ARM)
    #if defined(_MSC_VER)
            __yield();
    #else
            __asm__ __volatile__("yield" ::: "memory");
    #endif
#else
            // Generic fallback: compiler barrier, no CPU hint
            __asm__ __volatile__("" ::: "memory");
#endif
        }

        /**
         * @brief Returns the count of physical processing cores.
         *
         * Excludes logical processors from Hyper-Threading / SMT.
         * Uses OS-level APIs and is architecture-agnostic.
         */
        static int GetPhysicalCoreCount() noexcept
        {
#if defined(_WIN32)
            DWORD length = 0;
            GetLogicalProcessorInformation(nullptr, &length);
            if (length == 0)
                return 1;

            std::vector<SYSTEM_LOGICAL_PROCESSOR_INFORMATION> buffer(
                length / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION));
            if (!GetLogicalProcessorInformation(buffer.data(), &length))
                return 1;

            int count = 0;
            for (const auto& info : buffer)
            {
                if (info.Relationship == RelationProcessorCore)
                    count++;
            }
            return count > 0 ? count : 1;

#elif defined(__APPLE__)
            int    count = 0;
            size_t size  = sizeof(count);
            if (sysctlbyname("hw.physicalcpu", &count, &size, nullptr, 0) == 0)
                return count;
            return 1;

#else // Linux / BSD
            std::ifstream cpuinfo("/proc/cpuinfo");
            if (!cpuinfo.is_open())
                return 1;

            std::set<std::string> cores;
            std::string           line, physical_id, core_id;

            while (std::getline(cpuinfo, line))
            {
                if (line.find("physical id") == 0)
                    physical_id = line;
                if (line.find("core id") == 0)
                {
                    core_id = line;
                    cores.insert(physical_id + core_id);
                }
            }
            return cores.empty() ? 1 : static_cast<int>(cores.size());
#endif
        }
    };

} // namespace FREYR_NAMESPACE