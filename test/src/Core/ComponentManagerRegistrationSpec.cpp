#include "ComponentManagerTestSupport.hpp"

#include "../Components/ModelComponent.hpp"
#include "../Components/NameComponent.hpp"
#include "../Components/PositionComponent.hpp"
#include "../Components/VelocityComponent.hpp"
#include "../Components/DecayComponent.hpp"

struct LateRegisteredPluginComponent : fr::Component
{
    int value = 0;
};

TEST_F(ComponentManagerSpec, GetComponentIndexShouldReturnDistinctIndexes)
{
    AllTestComponents::RegisterAll(*mComponentManager);
    AllTestComponents::AssertDistinctIndexes(*mComponentManager);
}

TEST_F(ComponentManagerSpec, RegisterComponentShouldBeIdempotentWhenAlreadyRegistered)
{
    mComponentManager->RegisterComponent<LateRegisteredPluginComponent>();
    mComponentManager->RegisterComponent<LateRegisteredPluginComponent>();

    ASSERT_TRUE(mComponentManager->IsComponentRegistered<LateRegisteredPluginComponent>());
}

TEST_F(ComponentManagerSpec, UnregisterComponentShouldFailWhileEntitiesStillHaveComponent)
{
    mComponentManager->RegisterComponent<LateRegisteredPluginComponent>();

    constexpr fr::Entity entity = 1;
    mComponentManager->AddComponent(entity, LateRegisteredPluginComponent { .value = 42 });
    mRegistry->ExecuteTasks();

    ASSERT_FALSE(mComponentManager->UnregisterComponent<LateRegisteredPluginComponent>());
    ASSERT_TRUE(mComponentManager->IsComponentRegistered<LateRegisteredPluginComponent>());
}

TEST_F(ComponentManagerSpec, UnregisterComponentShouldSucceedAfterEntitiesNoLongerHaveComponent)
{
    mComponentManager->RegisterComponent<LateRegisteredPluginComponent>();

    constexpr fr::Entity entity = 1;
    mComponentManager->AddComponent(entity, LateRegisteredPluginComponent { .value = 7 });
    mRegistry->ExecuteTasks();

    mComponentManager->RemoveComponent<LateRegisteredPluginComponent>(entity);
    mRegistry->ExecuteTasks();

    ASSERT_TRUE(mComponentManager->UnregisterComponent<LateRegisteredPluginComponent>());
    ASSERT_FALSE(mComponentManager->IsComponentRegistered<LateRegisteredPluginComponent>());
}

TEST_F(ComponentManagerSpec, RegistryLateRegisterShouldExposeIsRegisteredAndUnregister)
{
    ASSERT_FALSE(mRegistry->IsComponentRegistered<LateRegisteredPluginComponent>());

    mRegistry->RegisterComponent<LateRegisteredPluginComponent>();
    ASSERT_TRUE(mRegistry->IsComponentRegistered<LateRegisteredPluginComponent>());

    mRegistry->RegisterComponent<LateRegisteredPluginComponent>();
    ASSERT_TRUE(mRegistry->IsComponentRegistered<LateRegisteredPluginComponent>());

    const auto entity = mRegistry->CreateEntity(LateRegisteredPluginComponent { .value = 1 });
    mRegistry->ExecuteTasks();

    ASSERT_FALSE(mRegistry->UnregisterComponent<LateRegisteredPluginComponent>());

    mRegistry->RemoveComponent<LateRegisteredPluginComponent>(entity);
    mRegistry->ExecuteTasks();

    ASSERT_TRUE(mRegistry->UnregisterComponent<LateRegisteredPluginComponent>());
    ASSERT_FALSE(mRegistry->IsComponentRegistered<LateRegisteredPluginComponent>());
}

