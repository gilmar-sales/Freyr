#pragma once

#include <Freyr/Freyr.hpp>

#include "../components/position.hpp"
#include "../events/collision.hpp"

class CollisionSystem final : public fr::System
{
  public:
    explicit CollisionSystem(const std::shared_ptr<fr::Scene>& scene) : System(scene) {}

    void PreUpdate(float deltaTime, const Ref<fr::Scheduler>& scheduler) override
    {
        scheduler->Run<Position>("CollisionCheck", [this](fr::Entity entity, Position& position) {
            mScene->SendEvent(CollisionEvent {});
        });

        scheduler->Run<Position>("CollisionUpdate", [](fr::Entity entity, Position& position) { position.x += 1; });
    }
};