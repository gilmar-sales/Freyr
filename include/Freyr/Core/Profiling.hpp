#pragma once

#include <memory>

namespace perfetto
{
    class TracingSession;
}

#ifdef FREYR_PROFILING

namespace FREYR_NAMESPACE
{
    class FreyrScopedTrace
    {
      public:
        FreyrScopedTrace(const char* category, const char* name);
        ~FreyrScopedTrace();

        FreyrScopedTrace(const FreyrScopedTrace&)            = delete;
        FreyrScopedTrace& operator=(const FreyrScopedTrace&) = delete;

      private:
        const char* mCategory;
    };

    void FreyrTraceBegin(const char* category, const char* name);
    void FreyrTraceEnd(const char* category);

    std::unique_ptr<perfetto::TracingSession> FreyrStartTracingSession();
    void FreyrStopTracingSession(perfetto::TracingSession& session);
} // namespace FREYR_NAMESPACE

    #ifndef FREYR_CONCAT
        #define FREYR_CONCAT_INNER(a, b) a##b
        #define FREYR_CONCAT(a, b)       FREYR_CONCAT_INNER(a, b)
    #endif

    #define FREYR_TRACE(category, name)                                                            \
        ::FREYR_NAMESPACE::FreyrScopedTrace FREYR_CONCAT(_freyr_trace_, __LINE__)(category, name)
    #define FREYR_TRACE_BEGIN(category, name, ...)                                                 \
        ::FREYR_NAMESPACE::FreyrTraceBegin(category, name)
    #define FREYR_TRACE_END(category, ...) ::FREYR_NAMESPACE::FreyrTraceEnd(category)

#else

    #define FREYR_TRACE(category, name)
    #define FREYR_TRACE_BEGIN(category, name, ...)
    #define FREYR_TRACE_END(category, ...)

#endif // FREYR_PROFILING
