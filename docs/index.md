# Freyr

**Freyr** is a high-performance, multithreaded **Entity-Component-System (ECS)** library for C++26, designed for
real-time simulations and games. Named after the Norse god of prosperity, Freyr brings abundance of performance through
data-oriented design and lock-free parallelism.

Its core idea is simple: organize entities into **archetype chunks** — contiguous memory blocks grouped by component
signature — and distribute those chunks across a thread pool as independent tasks. The result is predictable cache
behaviour and straightforward parallelism without manual synchronization.

---

## The big picture

```mermaid
graph TB
    subgraph Runtime["Runtime Registry"]
        SC["Registry"]
        CM["ComponentManager"]
        EM["EntityManager"]
        EVM["EventManager"]
        SM["SystemManager"]
        TP["ThreadPool"]
    end

    subgraph Storage["Archetype Storage"]
        A1["Archetype [Pos, Vel]"]
        A2["Archetype [Pos, Health]"]
        A3["Archetype [Pos, Vel, Mesh]"]
    end

    subgraph Workers["Worker Threads"]
        W1["Worker 0"]
        W2["Worker 1"]
        W3["Worker 2"]
        W4["Worker 3"]
    end

    SC --> CM
    SC --> EM
    SC --> EVM
    SC --> SM
    SC --> TP

    CM --> A1
    CM --> A2
    CM --> A3

    A1 -->|chunk tasks| TP
    A2 -->|chunk tasks| TP
    A3 -->|chunk tasks| TP

    TP -->|distributes| W1
    TP -->|distributes| W2
    TP -->|distributes| W3
    TP -->|distributes| W4
```

---

## Highlights

<div class="grid cards" markdown>

-   :material-lightning-bolt:{ .lg .middle } **High performance by design**

    ---

    Entities are stored in contiguous archetype chunks, minimising cache misses. The work-stealing thread pool
    distributes chunk processing across all available cores. Component arrays are plain vectors — no pointer chasing.

-   :material-cogs:{ .lg .middle } **Simple, composable API**

    ---

    Fluent builder pattern throughout — configure components, systems, and options with a single, readable call chain.
    No complex registration macros, no code generation.

-   :material-view-parallel:{ .lg .middle } **Built-in multithreading**

    ---

    `Query::EachAsync` distributes chunk processing across a lock-free thread pool with zero boilerplate. Each chunk
    becomes an independent task — perfect load balancing with work stealing.

-   :material-broadcast:{ .lg .middle } **Decoupled event system**

    ---

    Thread-safe publish/subscribe bus lets systems communicate without direct dependencies. Events are delivered
    synchronously with safe concurrent subscription.

-   :material-chip:{ .lg .middle } **Archetype-based storage**

    ---

    Entities sharing the same component types are grouped into archetypes. Component addition/removal triggers automatic
    archetype migration — no manual management needed.

-   :material-chart-bell-curve:{ .lg .middle } **Built-in profiling**

    ---

    Optional Perfetto integration provides detailed execution traces. Visualize system timings, chunk iteration
    duration, and thread utilisation in the Perfetto UI.

</div>

---

## At a glance

```cpp
#include <Freyr/Freyr.hpp>

struct Position : fr::Component { float x, y, z; };
struct Velocity : fr::Component { float dx, dy, dz; };

class MovementSystem : public fr::System {
public:
    explicit MovementSystem(const Ref<fr::Registry>& registry) : System(registry) {}

    void Update(float dt) override {
        // EachAsync dispatches one task per chunk — all 8 threads share the work
        mRegistry->CreateQuery()->EachAsync<Position, Velocity>(
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

| Requirement  | Minimum version | Notes                                    |
|--------------|-----------------|------------------------------------------|
| C++ standard | C++26           | Requires reflection (`-freflection`), `std::format` |
| CMake        | 3.29            | FetchContent support                     |
| GCC          | 13              | Fully tested                             |
| Clang        | 16              | Fully tested                             |
| MSVC         | 19.37           | Visual Studio 2022 17.7+                 |

### Dependencies

| Library        | Version  | Purpose                          |
|----------------|----------|----------------------------------|
| Skirnir        | ≥0.22.0  | DI container, application framework |
| Perfetto       | latest   | Profiling (optional, via submodule) |
| Google Test    | ≥1.17.0  | Testing (dev only)               |

All dependencies except Perfetto are fetched automatically via CMake `FetchContent`.

---

## Next steps

- [Installation](getting-started/installation.md) — Add Freyr to your project with CMake FetchContent
- [Quick Start](getting-started/quick-start.md) — Build a full example in minutes
- [ECS Overview](concepts/ecs-overview.md) — Understand the concepts behind the library
- [Architecture](concepts/architecture.md) — Deep dive into the internal design
