# Freyr

**Freyr** is a high-performance, multithreaded **Entity-Component-System (ECS)** library for C++23, designed for
real-time simulations and games.

Its core idea is simple: organize entities into **archetype chunks** — contiguous memory blocks grouped by component
signature — and distribute those chunks across a thread pool as independent tasks. The result is predictable cache
behaviour and straightforward parallelism without manual synchronization.

---

## Highlights

<div class="grid cards" markdown>

-   :material-lightning-bolt:{ .lg .middle } **High performance by design**

    ---

    Entities are stored in contiguous archetype chunks, minimising cache misses. Two scheduling strategies let you tune between cache affinity and load balancing.

-   :material-cogs:{ .lg .middle } **Simple, composable API**

    ---

    Fluent builder pattern throughout — configure components, systems, and options with a single, readable call chain.

-   :material-view-parallel:{ .lg .middle } **Built-in multithreading**

    ---

    `ForEachAsync` distribute chunk processing across a lock-free thread pool with zero boilerplate.

-   :material-broadcast:{ .lg .middle } **Decoupled event system**

    ---

    Thread-safe publish/subscribe bus lets systems communicate without direct dependencies.

</div>

---

## At a glance

```cpp
#include <Freyr/Freyr.hpp>

struct Position : fr::Component { float x, y, z; };
struct Velocity : fr::Component { float dx, dy, dz; };

class MovementSystem : public fr::System {
public:
    explicit MovementSystem(const Ref<fr::Scene>& scene) : System(scene) {}

    void Update(float dt) override {
        mScene->ForEachAsync<Position, Velocity>(
            [dt](fr::Entity, Position& pos, Velocity& vel) {
                pos.x += vel.dx * dt;
                pos.y += vel.dy * dt;
                pos.z += vel.dz * dt;
            });
    }
};

int main() {
    auto app = skr::ApplicationBuilder()
        .AddExtension<fr::FreyrExtension>([](fr::FreyrExtension& freyr) {
            freyr
                .WithOptions([](fr::FreyrOptionsBuilder& opts) {
                    opts.WithMaxEntities(1'000'000).WithThreadCount(8);
                })
                .WithComponent<Position>()
                .WithComponent<Velocity>()
                .WithPipeline([](fr::PipelineBuilder& pipeline) {
                    pipeline.WithName("Physics")
                        .WithRate(60.0f)
                        .WithSystem<MovementSystem>();
                });
        })
        .Build<MyApp>();

    app->Run();
}
```

---

## Requirements

| Requirement  | Minimum version |
|--------------|-----------------|
| C++ standard | C++23           |
| CMake        | 3.29            |
| GCC          | 13              |
| Clang        | 16              |
| MSVC         | 19.37           |

---

## Next steps

- [Installation](getting-started/installation.md) — Add Freyr to your project with CMake FetchContent
- [Quick Start](getting-started/quick-start.md) — Build a full example in minutes
- [ECS Overview](concepts/ecs-overview.md) — Understand the concepts behind the library
