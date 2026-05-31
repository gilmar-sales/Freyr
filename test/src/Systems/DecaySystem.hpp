#pragma once

#include <Freyr/Freyr.hpp>

class DecaySystem : public fr::System
{
  public:
    DecaySystem(const Ref<fr::Registry> registry) : System(registry) {}

    void PreUpdate(float deltaTime) override;
};
