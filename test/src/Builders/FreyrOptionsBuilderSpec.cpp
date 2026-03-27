#include <gtest/gtest.h>

#include <Freyr/Builders/FreyrOptionsBuilder.hpp>

class FreyrOptionsBuilderSpec : public ::testing::Test
{
  protected:
    void SetUp() override { mFreyrOptionsBuilder = std::make_shared<fr::FreyrOptionsBuilder>(); }

    void TearDown() override { mFreyrOptionsBuilder.reset(); }

    Ref<fr::FreyrOptionsBuilder> mFreyrOptionsBuilder;
};

TEST_F(FreyrOptionsBuilderSpec, BuildShouldReturnDefaultOptions)
{
    // Arrange
    auto defaultOptions = fr::FreyrOptions {};

    // Act
    const auto options = mFreyrOptionsBuilder->Build();

    // Assert
    ASSERT_NE(options, nullptr);
    ASSERT_EQ(options->MaxEntities, defaultOptions.MaxEntities);
    ASSERT_EQ(options->ArchetypeChunkCapacity, defaultOptions.ArchetypeChunkCapacity);
    ASSERT_EQ(options->ThreadCount, defaultOptions.ThreadCount);
    ASSERT_EQ(options->FixedDeltaTime, defaultOptions.FixedDeltaTime);
    ASSERT_EQ(options->ExecutionStrategy, defaultOptions.ExecutionStrategy);
}

TEST_F(FreyrOptionsBuilderSpec, WithMaxEntitiesShouldSetMaxEntities)
{
    // Arrange
    constexpr std::uint64_t expectedMaxEntities = 1000;

    // Act
    const auto options = mFreyrOptionsBuilder->WithMaxEntities(expectedMaxEntities).Build();

    // Assert
    ASSERT_EQ(options->MaxEntities, expectedMaxEntities);
}

TEST_F(FreyrOptionsBuilderSpec, WithArchetypeChunkCapacityShouldSetArchetypeChunkCapacity)
{
    // Arrange
    constexpr std::uint64_t expectedCapacity = 1024;

    // Act
    const auto options = mFreyrOptionsBuilder->WithArchetypeChunkCapacity(expectedCapacity).Build();

    // Assert
    ASSERT_EQ(options->ArchetypeChunkCapacity, expectedCapacity);
}

TEST_F(FreyrOptionsBuilderSpec, WithFixedDeltaTimeShouldSetFixedDeltaTime)
{
    // Arrange
    constexpr float expectedFixedDeltaTime = 1.0f / 60.0f;

    // Act
    const auto options = mFreyrOptionsBuilder->WithFixedDeltaTime(expectedFixedDeltaTime).Build();

    // Assert
    ASSERT_EQ(options->FixedDeltaTime, expectedFixedDeltaTime);
}

TEST_F(FreyrOptionsBuilderSpec, WithThreadCountShouldSetThreadCount)
{
    // Arrange
    constexpr std::uint64_t expectedThreadCount = 8;

    // Act
    const auto options = mFreyrOptionsBuilder->WithThreadCount(expectedThreadCount).Build();

    // Assert
    ASSERT_EQ(options->ThreadCount, expectedThreadCount);
}

TEST_F(FreyrOptionsBuilderSpec, WithExecutionStrategyShouldSetExecutionStrategy)
{
    // Arrange
    constexpr auto expectedExecutionStrategy = fr::FreyrExecutionStategy::DispatchOrder;

    // Act
    const auto options = mFreyrOptionsBuilder->WithExecutionStrategy(expectedExecutionStrategy).Build();

    // Assert
    ASSERT_EQ(options->ExecutionStrategy, expectedExecutionStrategy);
}

TEST_F(FreyrOptionsBuilderSpec, MultipleSettersShouldChainCorrectly)
{
    // Assert
    constexpr std::uint64_t maxEntities       = 5000;
    constexpr std::uint64_t chunkCapacity     = 256;
    constexpr float         fixedDeltaTime    = 1.0f / 30.0f;
    constexpr std::uint64_t threadCount       = 2;
    constexpr auto          executionStrategy = fr::FreyrExecutionStategy::DispatchOrder;

    // Act
    const auto options =
        mFreyrOptionsBuilder->WithMaxEntities(maxEntities)
            .WithArchetypeChunkCapacity(chunkCapacity)
            .WithFixedDeltaTime(fixedDeltaTime)
            .WithThreadCount(threadCount)
            .WithExecutionStrategy(executionStrategy)
            .Build();

    // Assert
    ASSERT_EQ(options->MaxEntities, maxEntities);
    ASSERT_EQ(options->ArchetypeChunkCapacity, chunkCapacity);
    ASSERT_EQ(options->FixedDeltaTime, fixedDeltaTime);
    ASSERT_EQ(options->ThreadCount, threadCount);
    ASSERT_EQ(options->ExecutionStrategy, executionStrategy);
}
