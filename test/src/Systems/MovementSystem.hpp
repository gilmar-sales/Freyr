#pragma once

#include <Freyr/Freyr.hpp>

class MovementSystem : public fr::System
{
  public:
    MovementSystem(const skr::Arc<fr::Registry> registry) : System(registry) {}

    void Update(float deltaTime) override;
};
