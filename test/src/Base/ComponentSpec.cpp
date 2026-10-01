#include <gtest/gtest.h>

#include <Freyr/Base/Component.hpp>
#include <Freyr/Base/Event.hpp>

#include <memory>

class ComponentSpec : public ::testing::Test
{
};

struct PlainData
{
    float x = 0.f;
};

struct MoveOnlyData
{
    std::unique_ptr<int> value;
};

struct WithCtor
{
    WithCtor() : x(0) {}
    int x;
};

struct SampleEvent : fr::Event
{
    int x = 0;
};

static_assert(fr::IsComponent<PlainData>);
static_assert(fr::IsCopyableComponent<PlainData>);
static_assert(fr::IsComponent<MoveOnlyData>);
static_assert(!fr::IsCopyableComponent<MoveOnlyData>);
static_assert(!fr::IsComponent<int>);
static_assert(!fr::IsComponent<SampleEvent>);
static_assert(!fr::IsComponent<fr::Remove<PlainData>>);
static_assert(!fr::IsComponent<decltype([] {})>);
static_assert(!fr::IsComponent<WithCtor>);
struct CompA
{
};
struct CompB
{
};
struct CompC
{
};
struct CompD
{
};
struct CompE
{
};
struct CompF
{
};
struct CompG
{
};
struct CompH
{
};
struct CompI
{
};
struct CompJ
{
};
struct CompK
{
};
struct CompL
{
};
struct CompM
{
};
struct CompN
{
};
struct CompO
{
};
struct CompP
{
};
struct CompQ
{
};
struct CompR
{
};
struct CompS
{
};
struct CompT
{
};
struct CompU
{
};
struct CompV
{
};
struct CompW
{
};
struct CompX
{
};
struct CompY
{
};
struct CompZ
{
};

void GetComponentsIds()
{
    fr::GetComponentId<CompA>();
    fr::GetComponentId<CompB>();
    fr::GetComponentId<CompC>();
    fr::GetComponentId<CompD>();
    fr::GetComponentId<CompE>();
    fr::GetComponentId<CompF>();
    fr::GetComponentId<CompG>();
    fr::GetComponentId<CompH>();
    fr::GetComponentId<CompI>();
    fr::GetComponentId<CompJ>();
    fr::GetComponentId<CompK>();
    fr::GetComponentId<CompL>();
    fr::GetComponentId<CompM>();
    fr::GetComponentId<CompN>();
    fr::GetComponentId<CompO>();
    fr::GetComponentId<CompP>();
    fr::GetComponentId<CompQ>();
    fr::GetComponentId<CompR>();
    fr::GetComponentId<CompS>();
    fr::GetComponentId<CompT>();
    fr::GetComponentId<CompU>();
    fr::GetComponentId<CompV>();
    fr::GetComponentId<CompW>();
    fr::GetComponentId<CompX>();
    fr::GetComponentId<CompY>();
    fr::GetComponentId<CompZ>();
}
void GetComponentsIdsReverse()
{
    fr::GetComponentId<CompA>();
    fr::GetComponentId<CompB>();
    fr::GetComponentId<CompC>();
    fr::GetComponentId<CompD>();
    fr::GetComponentId<CompE>();
    fr::GetComponentId<CompF>();
    fr::GetComponentId<CompG>();
    fr::GetComponentId<CompH>();
    fr::GetComponentId<CompI>();
    fr::GetComponentId<CompJ>();
    fr::GetComponentId<CompK>();
    fr::GetComponentId<CompL>();
    fr::GetComponentId<CompM>();
    fr::GetComponentId<CompN>();
    fr::GetComponentId<CompO>();
    fr::GetComponentId<CompP>();
    fr::GetComponentId<CompQ>();
    fr::GetComponentId<CompR>();
    fr::GetComponentId<CompS>();
    fr::GetComponentId<CompT>();
    fr::GetComponentId<CompU>();
    fr::GetComponentId<CompV>();
    fr::GetComponentId<CompW>();
    fr::GetComponentId<CompX>();
    fr::GetComponentId<CompY>();
    fr::GetComponentId<CompZ>();
}

TEST_F(ComponentSpec, ComponentShouldExecuteInCompileTimeToBeThreadSafe)
{
    auto threads = std::vector<std::thread>();

    threads.emplace_back(GetComponentsIds);
    threads.emplace_back(GetComponentsIdsReverse);
    threads.emplace_back(GetComponentsIds);
    threads.emplace_back(GetComponentsIdsReverse);

    for (auto& thread : threads)
        if (thread.joinable())
            thread.join();
    ASSERT_EQ(fr::GetComponentId<CompX>(), fr::GetComponentId<CompX>());
    ASSERT_EQ(fr::GetComponentId<CompY>(), fr::GetComponentId<CompY>());
    ASSERT_EQ(fr::GetComponentId<CompZ>(), fr::GetComponentId<CompZ>());

    std::set ids = {
        fr::GetComponentId<CompA>(), fr::GetComponentId<CompB>(), fr::GetComponentId<CompC>(),
        fr::GetComponentId<CompD>(), fr::GetComponentId<CompE>(), fr::GetComponentId<CompF>(),
        fr::GetComponentId<CompG>(), fr::GetComponentId<CompH>(), fr::GetComponentId<CompI>(),
        fr::GetComponentId<CompJ>(), fr::GetComponentId<CompK>(), fr::GetComponentId<CompL>(),
        fr::GetComponentId<CompM>(), fr::GetComponentId<CompN>(), fr::GetComponentId<CompO>(),
        fr::GetComponentId<CompP>(), fr::GetComponentId<CompQ>(), fr::GetComponentId<CompR>(),
        fr::GetComponentId<CompS>(), fr::GetComponentId<CompT>(), fr::GetComponentId<CompU>(),
        fr::GetComponentId<CompV>(), fr::GetComponentId<CompW>(), fr::GetComponentId<CompX>(),
        fr::GetComponentId<CompY>(), fr::GetComponentId<CompZ>(),
    };
    ASSERT_EQ(ids.size(), 26);
}

TEST_F(ComponentSpec, ComponentShouldHaveThreadSafeInitialization)
{
    constexpr int threadCount = 16;

    std::atomic<bool>            startGate { false };
    std::vector<fr::ComponentId> results(threadCount);
    std::vector<std::thread>     threads;
    threads.reserve(threadCount);

    for (int i = 0; i < threadCount; ++i)
    {
        threads.emplace_back([&results, &startGate, i]() {
            while (!startGate.load(std::memory_order_acquire))
            {
            }
            results[i] = fr::GetComponentId<CompA>();
        });
    }

    startGate.store(true, std::memory_order_release);

    for (auto& t : threads)
        t.join();

    const fr::ComponentId expected = results[0];
    for (const auto& id : results)
        EXPECT_EQ(id, expected);
}
