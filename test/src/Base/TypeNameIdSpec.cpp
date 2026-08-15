#include <gtest/gtest.h>

#include <Freyr/Base/Component.hpp>
#include <Freyr/Base/Event.hpp>
#include <Freyr/Base/System.hpp>
#include <Freyr/Base/TypeNameId.hpp>

#include <atomic>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace
{
    struct StableIdComponentA : fr::Component
    {
    };

    struct StableIdComponentB : fr::Component
    {
    };

    struct StableIdEventA : fr::Event
    {
    };

    class StableIdSystemA : public fr::System
    {
      public:
        using System::System;
    };
} // namespace

TEST(TypeNameIdSpec, RegisterTypeNameSameKeyReturnsSameId)
{
    const auto first  = fr::RegisterTypeName(fr::TypeIdKind::Component, "freyr.test.StableNameOnce");
    const auto second = fr::RegisterTypeName(fr::TypeIdKind::Component, "freyr.test.StableNameOnce");

    EXPECT_EQ(first, second);
}

TEST(TypeNameIdSpec, RegisterTypeNameAllocatesDenseIds)
{
    const auto a = fr::RegisterTypeName(fr::TypeIdKind::Component, "freyr.test.DenseA");
    const auto b = fr::RegisterTypeName(fr::TypeIdKind::Component, "freyr.test.DenseB");
    const auto c = fr::RegisterTypeName(fr::TypeIdKind::Component, "freyr.test.DenseC");

    std::set<fr::ComponentId> ids { a, b, c };
    ASSERT_EQ(ids.size(), 3u);

    const auto minId = *ids.begin();
    const auto maxId = *ids.rbegin();
    EXPECT_EQ(maxId - minId, 2u);
}

TEST(TypeNameIdSpec, GetComponentIdMatchesPriorNameRegistration)
{
    const auto expected =
        fr::RegisterTypeName(fr::TypeIdKind::Component, refl::type_name<StableIdComponentA>());

    EXPECT_EQ(fr::GetComponentId<StableIdComponentA>(), expected);
    EXPECT_EQ(fr::GetComponentId<StableIdComponentA>(), fr::GetComponentId<StableIdComponentA>());
}

TEST(TypeNameIdSpec, GetIdsAreStableAcrossKindsByTypeName)
{
    const auto componentId = fr::GetComponentId<StableIdComponentB>();
    const auto eventId     = fr::GetEventId<StableIdEventA>();
    const auto systemId    = fr::GetSystemId<StableIdSystemA>();

    EXPECT_EQ(componentId,
              fr::RegisterTypeName(fr::TypeIdKind::Component, refl::type_name<StableIdComponentB>()));
    EXPECT_EQ(eventId, fr::RegisterTypeName(fr::TypeIdKind::Event, refl::type_name<StableIdEventA>()));
    EXPECT_EQ(systemId,
              static_cast<fr::SystemId>(
                  fr::RegisterTypeName(fr::TypeIdKind::System, refl::type_name<StableIdSystemA>())));
}

TEST(TypeNameIdSpec, TypeNameOfReturnsRegisteredName)
{
    const auto id = fr::RegisterTypeName(fr::TypeIdKind::Component, "freyr.test.TypeNameOfLookup");

    EXPECT_EQ(fr::TypeNameOf(fr::TypeIdKind::Component, id), "freyr.test.TypeNameOfLookup");
    EXPECT_TRUE(fr::TypeNameOf(fr::TypeIdKind::Component, id + 1'000'000).empty());
}

TEST(TypeNameIdSpec, KindRegistriesAreIndependent)
{
    constexpr std::string_view name = "freyr.test.SharedAcrossKinds";

    const auto componentId = fr::RegisterTypeName(fr::TypeIdKind::Component, name);
    const auto eventId     = fr::RegisterTypeName(fr::TypeIdKind::Event, name);
    const auto systemId    = fr::RegisterTypeName(fr::TypeIdKind::System, name);

    EXPECT_EQ(fr::RegisterTypeName(fr::TypeIdKind::Component, name), componentId);
    EXPECT_EQ(fr::RegisterTypeName(fr::TypeIdKind::Event, name), eventId);
    EXPECT_EQ(fr::RegisterTypeName(fr::TypeIdKind::System, name), systemId);
}

TEST(TypeNameIdSpec, ConcurrentRegisterTypeNameSameKeyIsStable)
{
    constexpr int              threadCount = 16;
    constexpr std::string_view name        = "freyr.test.ConcurrentStableName";

    std::atomic<bool>           startGate { false };
    std::vector<std::uint64_t>  results(threadCount);
    std::vector<std::thread>    threads;
    threads.reserve(threadCount);

    for (int i = 0; i < threadCount; ++i)
    {
        threads.emplace_back([&results, &startGate, i]() {
            while (!startGate.load(std::memory_order_acquire))
            {
            }
            results[i] = fr::RegisterTypeName(fr::TypeIdKind::Component, name);
        });
    }

    startGate.store(true, std::memory_order_release);

    for (auto& thread : threads)
        thread.join();

    const auto expected = results[0];
    for (const auto id : results)
        EXPECT_EQ(id, expected);
}
