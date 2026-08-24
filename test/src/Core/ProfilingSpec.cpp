#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <set>
#include <string>

#include "Freyr/Core/FreyrExtension.hpp"
#include "Freyr/Core/Registry.hpp"

#include "../Components/PositionComponent.hpp"
#include "../Components/VelocityComponent.hpp"
#include "../EmptyApp.hpp"

namespace fs = std::filesystem;

namespace
{
    std::set<fs::path> ListTraceFiles(const fs::path& dir)
    {
        std::set<fs::path> files;
        if (!fs::exists(dir))
            return files;

        for (const auto& entry : fs::directory_iterator(dir))
        {
            if (!entry.is_regular_file())
                continue;

            const auto name = entry.path().filename().string();
            if (name.starts_with("freyr_trace_") && name.ends_with(".pftrace"))
                files.insert(entry.path());
        }
        return files;
    }

    fs::path FindNewTraceFile(const std::set<fs::path>& before, const fs::path& dir)
    {
        for (const auto& path : ListTraceFiles(dir))
        {
            if (!before.contains(path))
                return path;
        }
        return {};
    }

    bool TraceContains(const fs::path& path, const std::string& needle)
    {
        std::ifstream input(path, std::ios::binary);
        if (!input)
            return false;

        const auto data = std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
        return data.find(needle) != std::string::npos;
    }
} // namespace

struct ProfilingSpec : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        mApp = skr::ApplicationBuilder()
                   .WithExtension<fr::FreyrExtension>([](fr::FreyrExtension& freyr) {
                       freyr.WithComponent<PositionComponent>().WithComponent<VelocityComponent>();
                   })
                   .Build<EmptyApp>();

        mRegistry     = mApp->GetRootServiceProvider()->GetService<fr::Registry>();
        mTraceDir     = fs::current_path();
        mTracesBefore = ListTraceFiles(mTraceDir);
    }

    void TearDown() override
    {
        for (const auto& path : ListTraceFiles(mTraceDir))
        {
            if (!mTracesBefore.contains(path))
            {
                std::error_code ec;
                fs::remove(path, ec);
            }
        }

        mRegistry.reset();
        mApp.reset();
    }

    skr::Arc<EmptyApp>     mApp;
    skr::Arc<fr::Registry> mRegistry;
    fs::path               mTraceDir;
    std::set<fs::path>     mTracesBefore;
};

TEST_F(ProfilingSpec, BeginProfilingAndEndProfilingAreCallable)
{
    mRegistry->BeginProfiling();
#ifdef FREYR_PROFILING
    mRegistry->Update(0.016f);
#endif
    mRegistry->EndProfiling();
}

TEST_F(ProfilingSpec, BeginTraceAndEndTraceAreCallable)
{
    mRegistry->BeginTrace("ProfilingSpec::SmokeTrace");
    mRegistry->EndTrace();
}

#ifdef FREYR_PROFILING

TEST_F(ProfilingSpec, EndProfilingWritesNonEmptyTraceFile)
{
    mRegistry->BeginProfiling();
    mRegistry->Update(0.016f);
    mRegistry->EndProfiling();

    const auto trace = FindNewTraceFile(mTracesBefore, mTraceDir);
    ASSERT_FALSE(trace.empty()) << "Expected freyr_trace_*.pftrace in " << mTraceDir;
    EXPECT_GT(fs::file_size(trace), 0u);
    EXPECT_TRUE(TraceContains(trace, "MainThread"));
    EXPECT_TRUE(TraceContains(trace, "Frame"));
}

TEST_F(ProfilingSpec, CustomTraceScopesAreRecordedInTraceFile)
{
    mRegistry->BeginProfiling();
    mRegistry->Update(0.016f);

    mRegistry->BeginTrace("ProfilingSpec::CustomScope");
    mRegistry->EndTrace();

    mRegistry->Update(0.016f);
    mRegistry->EndProfiling();

    const auto trace = FindNewTraceFile(mTracesBefore, mTraceDir);
    ASSERT_FALSE(trace.empty());
    EXPECT_TRUE(TraceContains(trace, "ProfilingSpec::CustomScope"));
}

TEST_F(ProfilingSpec, LabeledMutationIsRecordedDuringProfilingSession)
{
    mRegistry->CreateEntity(PositionComponent { .x = 1.f, .y = 2.f, .z = 3.f });
    mRegistry->CreateEntity(PositionComponent { .x = 4.f, .y = 5.f, .z = 6.f });
    mRegistry->ExecuteTasks();

    mRegistry->BeginProfiling();
    mRegistry->Update(0.016f);

    mRegistry->CreateMutation()->WithLabel("ProfilingSpec::Mutation").Each(
        [](fr::Entity, PositionComponent& position) { position.x += 1.f; });

    mRegistry->CreateQuery()->WithLabel("ProfilingSpec::Query").Reduce(
        [](const float acc, PositionComponent& position) { return acc + position.x; }, 0.f);

    mRegistry->Update(0.016f);
    mRegistry->EndProfiling();

    const auto trace = FindNewTraceFile(mTracesBefore, mTraceDir);
    ASSERT_FALSE(trace.empty());
    EXPECT_TRUE(TraceContains(trace, "ProfilingSpec::Mutation"));
    EXPECT_TRUE(TraceContains(trace, "ProfilingSpec::Query"));
}

TEST_F(ProfilingSpec, MultipleUpdatesProduceSingleTraceFile)
{
    mRegistry->BeginProfiling();
    mRegistry->Update(0.016f);
    mRegistry->Update(0.016f);
    mRegistry->Update(0.016f);
    mRegistry->EndProfiling();

    std::size_t newTraces = 0;
    for (const auto& path : ListTraceFiles(mTraceDir))
    {
        if (!mTracesBefore.contains(path))
            ++newTraces;
    }

    EXPECT_EQ(newTraces, 1u);
}

#endif // FREYR_PROFILING
