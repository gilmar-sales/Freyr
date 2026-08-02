#pragma once

#include <Freyr/Base/System.hpp>

class DecaySystem : public fr::System
{
  public:
    DecaySystem(const skr::Arc<fr::Registry> registry) : System(registry) {}

    void PreUpdate(float deltaTime) override;
};
