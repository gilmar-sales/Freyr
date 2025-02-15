#include <Freyr/Containers/SparseSet.hpp>
#include <gtest/gtest.h>

class SparseSetSpec : public ::testing::Test
{
  protected:
    void SetUp() override {}

    void TearDown() override {}

    fr::SparseSet<std::uint64_t> mSparseSet;
};

TEST_F(SparseSetSpec, SparseSetInsertShouldBeThreadSafe)
{
    constexpr int numThreads  = 10;
    constexpr int numEntities = 100'000;
    constexpr int batchSize   = numEntities / numThreads;

    mSparseSet.resize(numEntities);

    std::vector<std::thread> threads;

    for (int i = 0; i < numThreads; ++i)
    {
        threads.emplace_back([this, i = i] {
            const auto first = batchSize * i;
            const auto last  = first + batchSize;

            for (int j = first; j < last; ++j)
            {
                mSparseSet.insert(j);
            }
        });
    }

    for (auto& t : threads)
    {
        if (t.joinable())
            t.join();
    }

    for (int i = 0; i < numEntities; ++i)
    {
        ASSERT_TRUE(mSparseSet.contains(i));
    }
}

TEST_F(SparseSetSpec, SparseSetTryInsertShouldBeThreadSafe)
{
    constexpr int numThreads  = 10;
    constexpr int numEntities = 100'000;
    constexpr int batchSize   = numEntities / numThreads;

    mSparseSet.resize(numEntities);

    std::vector<std::thread> threads;

    for (int i = 0; i < numThreads; ++i)
    {
        threads.emplace_back([this, i = i] {
            const auto first = batchSize * i;
            const auto last  = first + batchSize;

            for (int j = first; j < last; ++j)
            {
                mSparseSet.try_insert(j);
            }
        });
    }

    for (auto& t : threads)
    {
        if (t.joinable())
            t.join();
    }

    for (int i = 0; i < numEntities; ++i)
    {
        ASSERT_TRUE(mSparseSet.contains(i));
    }
}

TEST_F(SparseSetSpec, SparseSetRemoveShouldBeThreadSafe)
{
    constexpr int numThreads  = 10;
    constexpr int numEntities = 100'000;
    constexpr int batchSize   = numEntities / numThreads;

    mSparseSet.resize(numEntities);

    std::vector<std::thread> threads;

    for (int i = 0; i < numEntities; ++i)
    {
        mSparseSet.insert(i);
    }

    for (int i = 0; i < numThreads; ++i)
    {
        threads.emplace_back([this, i = i] {
            const auto first = batchSize * i;
            const auto last  = first + batchSize;

            for (int j = first; j < last; ++j)
            {
                mSparseSet.remove(j);
            }
        });
    }

    for (auto& t : threads)
    {
        if (t.joinable())
            t.join();
    }

    for (int i = 0; i < numEntities; ++i)
    {
        ASSERT_FALSE(mSparseSet.contains(i));
    }
}

TEST_F(SparseSetSpec, SparseSetTryRemoveShouldBeThreadSafe)
{
    constexpr int numThreads  = 10;
    constexpr int numEntities = 100'000;
    constexpr int batchSize   = numEntities / numThreads;

    mSparseSet.resize(numEntities);

    std::vector<std::thread> threads;

    for (int i = 0; i < numEntities; ++i)
    {
        mSparseSet.try_insert(i);
    }

    for (int i = 0; i < numThreads; ++i)
    {
        threads.emplace_back([this, i = i] {
            const auto first = batchSize * i;
            const auto last  = first + batchSize;

            for (int j = first; j < last; ++j)
            {
                mSparseSet.try_remove(j);
            }
        });
    }

    for (auto& t : threads)
    {
        if (t.joinable())
            t.join();
    }

    for (int i = 0; i < numEntities; ++i)
    {
        ASSERT_FALSE(mSparseSet.contains(i));
    }
}