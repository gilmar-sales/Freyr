#pragma once

#include <Freyr/Freyr.hpp>

class MovementSystem : public fr::System
{
  public:
    MovementSystem(const Ref<fr::Scene> scene) : System(scene) {}

    void Update(float deltaTime) override;
};
