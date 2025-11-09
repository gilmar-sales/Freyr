#include <gtest/gtest.h>

#include <Freyr/Freyr.hpp>

class ComponentSpec : public ::testing::Test
{
};
struct CompA : fr::Component
{
};
struct CompB : fr::Component
{
};
struct CompC : fr::Component
{
};
struct CompD : fr::Component
{
};
struct CompE : fr::Component
{
};
struct CompF : fr::Component
{
};
struct CompG : fr::Component
{
};
struct CompH : fr::Component
{
};
struct CompI : fr::Component
{
};
struct CompJ : fr::Component
{
};
struct CompK : fr::Component
{
};
struct CompL : fr::Component
{
};
struct CompM : fr::Component
{
};
struct CompN : fr::Component
{
};
struct CompO : fr::Component
{
};
struct CompP : fr::Component
{
};
struct CompQ : fr::Component
{
};
struct CompR : fr::Component
{
};
struct CompS : fr::Component
{
};
struct CompT : fr::Component
{
};
struct CompU : fr::Component
{
};
struct CompV : fr::Component
{
};
struct CompW : fr::Component
{
};
struct CompX : fr::Component
{
};
struct CompY : fr::Component
{
};
struct CompZ : fr::Component
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

    ASSERT_EQ(fr::GetComponentId<CompZ>(), 29);
}

TEST_F(ComponentSpec, ComponentCountShouldBeTotalPlus1)
{
    GetComponentsIds();
    ASSERT_EQ(fr::ComponentCount, 30);
}
