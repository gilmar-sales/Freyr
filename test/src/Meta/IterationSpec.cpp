#include <string>
#include <tuple>
#include <vector>

#include <Freyr/Meta/Iteration.hpp>
#include <gtest/gtest.h>

class IterationSpec : public ::testing::Test
{
};

TEST_F(IterationSpec, forArgsShouldCallFunctionForEachArgument)
{
    std::vector<int> results;
    fr::meta::forArgs([&results](int x) { results.push_back(x); }, 1, 2, 3);

    ASSERT_EQ(results.size(), 3);
    ASSERT_EQ(results[0], 1);
    ASSERT_EQ(results[1], 2);
    ASSERT_EQ(results[2], 3);
}

TEST_F(IterationSpec, forArgsShouldWorkWithSingleArgument)
{
    int result = 0;
    fr::meta::forArgs([&result](int x) { result = x; }, 42);

    ASSERT_EQ(result, 42);
}

TEST_F(IterationSpec, forArgsShouldWorkWithNoArguments)
{
    int callCount = 0;
    fr::meta::forArgs([&callCount](auto&&...) { ++callCount; });

    ASSERT_EQ(callCount, 0);
}

TEST_F(IterationSpec, forArgsShouldWorkWithDifferentTypes)
{
    std::string result;
    fr::meta::forArgs([&result](auto&& x) { result += std::to_string(x) + ","; }, 1, 2.5f, 3.14, 4L);

    ASSERT_EQ(result, "1,2.500000,3.140000,4,");
}

TEST_F(IterationSpec, forEachShouldIterateOverTuple)
{
    auto             tuple = std::make_tuple(1, 2, 3);
    std::vector<int> results;

    fr::meta::forEach([&results](auto&&... xs) { ((results.push_back(xs)), ...); }, tuple);

    ASSERT_EQ(results.size(), 3);
    ASSERT_EQ(results[0], 1);
    ASSERT_EQ(results[1], 2);
    ASSERT_EQ(results[2], 3);
}

TEST_F(IterationSpec, forEachShouldWorkWithSingleElementTuple)
{
    auto tuple  = std::make_tuple(42);
    int  result = 0;

    fr::meta::forEach([&result](auto&& x) { result = x; }, tuple);

    ASSERT_EQ(result, 42);
}

TEST_F(IterationSpec, forEachShouldWorkWithEmptyTuple)
{
    auto tuple  = std::tuple<>();
    bool called = false;

    fr::meta::forEach([&called](auto&&...) { called = true; }, tuple);

    ASSERT_FALSE(called);
}

TEST_F(IterationSpec, forEachShouldWorkWithStringTuple)
{
    auto        tuple = std::make_tuple(std::string("hello"), std::string("world"));
    std::string result;

    fr::meta::forEach([&result](auto&&... xs) { ((result += xs + " "), ...); }, tuple);

    ASSERT_EQ(result, "hello world ");
}

TEST_F(IterationSpec, forEachShouldWorkWithMixedTypeTuple)
{
    auto        tuple = std::make_tuple(10, 20.4, 3.14f);
    std::string result;

    fr::meta::forEach([&result](auto... xs) { ((result += std::to_string(xs) + ","), ...); }, tuple);

    ASSERT_EQ(result, "10,20.400000,3.140000,");
}