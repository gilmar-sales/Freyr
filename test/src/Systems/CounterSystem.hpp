#pragma once

#include <Freyr/Base/System.hpp>

class CounterSystem : public fr::System
{
  public:
    explicit CounterSystem(const skr::Arc<fr::Registry> registry) : System(registry) {}

    void Update(float deltaTime) override
    {
        ++UpdateCount;
        LastDeltaTime = deltaTime;
    }

    int   UpdateCount   = 0;
    float LastDeltaTime = 0.f;
};
