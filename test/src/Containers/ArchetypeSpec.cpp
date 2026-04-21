#include "gtest/gtest.h"
#include <Freyr/Freyr.hpp>

#include "../Components/ModelComponent.hpp"
#include "../Components/PositionComponent.hpp"

class ArchetypeSpec : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        auto app = skr::ApplicationBuilder().AddExtension<fr::FreyrExtension>([](Ref<fr::FreyrExtension> freyr) {
            freyr->WithOptions([](fr::FreyrOptionsBuilder& builder) { builder.WithArchetypeChunkCapacity(2048); });
        });

        const auto provider = app.GetServiceCollection()->CreateServiceProvider();

        mArchetype = provider->GetService<fr::Archetype>();
    }

    void TearDown() override { mArchetype.reset(); }

    Ref<fr::Archetype> mArchetype;
};

TEST_F(ArchetypeSpec, ArchetypeShouldRegisterComponent)
{
    mArchetype->RegisterComponent<PositionComponent>();

    ASSERT_TRUE(mArchetype->HasComponent<PositionComponent>());
}
