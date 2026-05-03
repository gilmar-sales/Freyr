# Quick Start

This guide walks through building a minimal simulation: entities with `Position` and `Velocity` components updated by a `MovementSystem`.

---

## 1. Define components

Components are plain data structs that inherit from `fr::Component`. No logic, no methods — just fields.

```cpp
// components.hpp
#include <Freyr/Freyr.hpp>

struct Position : fr::Component {
    float x = 0.f;
    float y = 0.f;
    float z = 0.f;
};

struct Velocity : fr::Component {
    float dx = 0.f;
    float dy = 0.f;
    float dz = 0.f;
};
```

---

## 2. Define a system

Systems inherit from `fr::System` and override lifecycle hooks. The constructor must accept a `Ref<fr::Scene>`.

```cpp
// movement_system.hpp
#include <Freyr/Freyr.hpp>
#include "components.hpp"

class MovementSystem : public fr::System {
public:
    explicit MovementSystem(const Ref<fr::Scene>& scene) : System(scene) {}

    void Update(float deltaTime) override {
        mScene->CreateQuery()
            ->EachAsync<Position, Velocity>(
                [deltaTime](fr::Entity, Position& pos, const Velocity& vel) {
                    pos.x += vel.dx * deltaTime;
                    pos.y += vel.dy * deltaTime;
                    pos.z += vel.dz * deltaTime;
                });
    }
};
```

---

## 3. Define the application

```cpp
// app.hpp
#include <Freyr/Freyr.hpp>
#include "components.hpp"

class MyApp : public skr::IApplication {
public:
    explicit MyApp(const Ref<skr::ServiceProvider>& sp) : IApplication(sp) {
        mScene = sp->GetService<fr::Scene>();

        // Bulk-create 1 million entities with default values
        mScene->CreateArchetypeBuilder()
            .WithComponent(Position {})
            .WithComponent(Velocity { .dx = 1.f, .dy = 0.5f })
            .WithEntities(1'000'000)
            .Build();
    }

    void Run() override {
        constexpr float dt = 1.0f / 60.0f;
        while (true)
            mScene->Update(dt);
    }

private:
    Ref<fr::Scene> mScene;
};
```

---

## 4. Bootstrap with FreyrExtension

```cpp
// main.cpp
#include <Freyr/Freyr.hpp>
#include "app.hpp"
#include "components.hpp"
#include "movement_system.hpp"

int main() {
    auto app = skr::ApplicationBuilder()
        .AddExtension<fr::FreyrExtension>([](fr::FreyrExtension& freyr) {
            freyr
                .WithOptions([](fr::FreyrOptionsBuilder& opts) {
                    opts
                        .WithMaxEntities(1'500'000)        // max live entities
                        .WithArchetypeChunkCapacity(512)   // entities per chunk
                        .WithThreadCount(8);               // worker threads
                })
                .WithComponent<Position>()
                .WithComponent<Velocity>()
                .WithPipeline([](fr::PipelineBuilder& pipeline) {
                    pipeline.WithName("Main")
                        .WithRate(60.0f)
                        .WithSystem<MovementSystem>();
                });
        })
        .Build<MyApp>();

    app->Run();
    return 0;
}
```

---

## 5. What happens at runtime

```
main()
 └─ ApplicationBuilder::Build<MyApp>()
     ├─ FreyrExtension registers Position, Velocity, MovementSystem
     ├─ Scene is created and added to the DI container
     └─ MyApp constructor runs
         └─ ArchetypeBuilder creates 1 000 000 entities in chunks of 512

MyApp::Run() loop
 └─ Scene::Update(dt) each frame
     ├─ MovementSystem::PreUpdate(dt)
     ├─ MovementSystem::Update(dt)   ← Query::EachAsync distributes chunks across 8 threads
     ├─ MovementSystem::PostUpdate(dt)
     └─ deferred entity destruction (none here)
```

---

## Next steps

- [ECS Overview](../concepts/ecs-overview.md) — understand the model behind the API
- [Scene API](../api/scene.md) — full reference for entity and component operations
- [ArchetypeBuilder](../api/archetype-builder.md) — efficient bulk entity creation
- [PipelineBuilder](../api/pipeline-builder.md) — organize systems into pipelines
- [Parallel Processing guide](../guides/parallel-processing.md) — tune parallelism for your workload
