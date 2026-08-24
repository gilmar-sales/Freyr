# Freyr

[![Coverage](https://codecov.io/gh/gilmar-sales/Freyr/branch/main/graph/badge.svg)](https://codecov.io/gh/gilmar-sales/Freyr)

A multithreaded ECS (Entity-Component-System) library focused on parallelism, based on task queues organized by archetype chunks.

## Table of Contents

- [Concept](#concept)
- [Architecture](#architecture)
- [Requirements](#requirements)
- [Installation](#installation)
- [Quick Start](#quick-start)
- [Core Concepts](#core-concepts)
  - [Entities](#entities)
  - [Components](#components)
  - [Systems](#systems)
  - [Events](#events)
- [API Reference](#api-reference)
  - [FreyrExtension](#freyrextension)
  - [FreyrOptionsBuilder](#freyroptionsbuilder)
  - [Registry](#registry)
  - [ArchetypeBuilder](#archetypebuilder)
  - [EventManager](#eventmanager)
- [Profiling](#profiling)
- [Examples](#examples)

---

## Concept

**Entity-Component-System (ECS)** is a software architectural pattern mainly used in real-time simulation software such as video games. It organizes game world objects following the principle of **composition over inheritance**: every entity is defined not by a class hierarchy, but by the **components** associated with it.

- **Entities** — lightweight identifiers (numeric IDs) representing world objects
- **Components** — plain data structures holding entity state (no logic)
- **Systems** — logic processors that operate on entities that match a specific set of components

### Pros

- **Data-Oriented** — components are stored contiguously in memory, reducing cache misses and improving throughput
- **Parallel-Friendly** — independent systems and non-conflicting component groups can be processed in parallel
- **Dynamic Behaviour** — add or remove components at runtime to change entity behaviour without recompilation

### Cons

- Overkill for unique, one-off entities
- Higher initial complexity compared to OOP designs
- Less mainstream; fewer learning resources available

---

## Architecture

Freyr organizes components into **archetypes** — groups of entities that share the same set of components. Each archetype is split into fixed-size **chunks**, which are the unit of parallel work distribution.

```
FreyrExtension (configuration)
    │
    ▼
Registry (orchestrator)
├── ComponentManager  → organizes entities into Archetypes → Chunks
├── EntityManager     → creates and recycles entity IDs
├── SystemManager     → registers and drives system lifecycle
├── EventManager      → publish/subscribe event bus
└── ThreadPool        → worker threads + lock-free MPMC queues

Update loop:
  Pipeline rate accumulation
  System::PreUpdate / Update / PostUpdate (per ready pipeline)
  MutationAggregator flush + chunk tasks
  Deferred entity destruction (after task drain)
```

---

## Requirements

- C++26 compatible compiler with reflection support (GCC 16+, Clang 22+)
- CMake 3.29+
- [Skirnir](https://github.com/gilmar-sales/skirnir) (fetched automatically via CMake)

- Perfetto — enables performance tracing (CMake option `FREYR_PROFILING=ON`)

---

## Installation

Add Freyr to your CMake project with `FetchContent`:

```cmake
include(FetchContent)

FetchContent_Declare(
    freyr
    GIT_REPOSITORY https://github.com/gilmar-sales/freyr.git
    GIT_TAG        main
)

FetchContent_MakeAvailable(freyr)

target_link_libraries(your_target PRIVATE Freyr)
```

Then include the main header:

```cpp
#include <Freyr/Freyr.hpp>
```

---

## Quick Start

```cpp
// main.cpp
#include <Freyr/Freyr.hpp>

// 1. Define components
struct Position : fr::Component {
    float x = 0.f, y = 0.f, z = 0.f;
};

struct Velocity : fr::Component {
    float dx = 0.f, dy = 0.f, dz = 0.f;
};

// 2. Define a system
class MovementSystem : public fr::System {
public:
    explicit MovementSystem(const skr::Arc<fr::Registry>& registry) : System(registry) {}

    void Update(float deltaTime) override {
        mRegistry->CreateMutation()->EachAsync(
            [deltaTime](fr::Entity, Position& pos, Velocity& vel) {
                pos.x += vel.dx * deltaTime;
                pos.y += vel.dy * deltaTime;
                pos.z += vel.dz * deltaTime;
            });
    }
};

// 3. Define the application
class MyApp : public skr::IApplication {
public:
    explicit MyApp(const skr::Arc<skr::ServiceProvider>& sp) : IApplication(sp) {
        mRegistry = sp->GetService<fr::Registry>();

        mRegistry->CreateArchetypeBuilder()
            .WithComponent(Position {})
            .WithComponent(Velocity { .dx = 1.f })
            .WithEntities(100'000)
            .Build();
    }

    void Run() override {
        while (true)
            mRegistry->Update(1.0f / 60.0f);
    }

private:
    skr::Arc<fr::Registry> mRegistry;
};

// 4. Bootstrap
int main() {
    auto app = skr::ApplicationBuilder()
        .AddExtension<fr::FreyrExtension>([](fr::FreyrExtension& freyr) {
            freyr
                .WithOptions([](fr::FreyrOptionsBuilder& opts) {
                    opts.WithMaxEntities(200'000)
                        .WithArchetypeChunkCapacity(1024)
                        .WithThreadCount(8);
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
}
```

---

## Core Concepts

### Entities

An entity is a plain integer ID (`fr::Entity`, aliased to `std::uint32_t`). It has no data of its own — its identity comes from the components attached to it.

> **Note:** Prefer not to persist raw entity IDs across long lifetimes if you destroy and recreate entities — IDs are recycled after deferred destruction completes.

```cpp
// Create an entity without components
fr::Entity e = registry->CreateEntity();

// Create an entity with components
fr::Entity e = registry->CreateEntity(Position { .x = 10.f }, Velocity {});

// Create an entity and receive it via callback
registry->CreateEntity([](fr::Entity e) { /* use e */ }, Position {});

// Destroy (deferred — component removal is queued, ID recycled after task drain)
registry->DestroyEntity(e);
```

### Components

Components derive from `fr::Component` and contain **only data** — no logic.

```cpp
struct Transform : fr::Component {
    float x = 0.f, y = 0.f, z = 0.f;
    float scaleX = 1.f, scaleY = 1.f, scaleZ = 1.f;
};
```

Each component type receives a unique compile-time ID:

```cpp
fr::ComponentId id = fr::GetComponentId<Transform>(); // e.g. 0
```

Component operations on a `Registry`:

```cpp
// Add a single component
registry->AddComponent(entity, Position { .x = 5.f });

// Add multiple components at once
registry->AddComponents(entity, Position {}, Velocity {});

// Remove a component (triggers archetype migration)
registry->RemoveComponent<Velocity>(entity);

// Query presence
bool has = registry->HasComponent<Position>(entity);
bool hasAll = registry->HasComponents<Position, Velocity>(entity);

// Access components safely via callback (returns false if not found)
bool found = registry->TryGetComponents<Position, Velocity>(entity,
    [](Position& pos, Velocity& vel) {
        pos.x += vel.dx;
    });
```

### Systems

Systems inherit from `fr::System` and override one or more lifecycle hooks:

| Method | Called |
|--------|--------|
| `PreUpdate(dt)` | Before the main update of a ready pipeline |
| `Update(dt)` | Main update step |
| `PostUpdate(dt)` | After the main update |

Pipelines control *when* systems run (`WithRate` in Hz). Use rate `0` (or omit) for every frame; use e.g. `60.0f` for a fixed cadence.

```cpp
class PhysicsSystem : public fr::System {
public:
    explicit PhysicsSystem(const skr::Arc<fr::Registry>& registry) : System(registry) {}

    void Update(float deltaTime) override {
        mRegistry->CreateMutation()->EachAsync(
            [deltaTime](fr::Entity, Position& pos, Velocity& vel) {
                pos.x += vel.dx * deltaTime;
            });
    }
};
```

Register systems via `PipelineBuilder::WithSystem<T>()` inside `FreyrExtension::WithPipeline(...)`. Systems are instantiated as singletons and injected with their dependencies via Skirnir's DI container.

Each system type has a runtime ID:

```cpp
fr::SystemId id = fr::GetSystemId<PhysicsSystem>();
```

### Events

Events derive from `fr::Event` and carry data between systems without tight coupling.

```cpp
struct CollisionEvent : fr::Event {
    fr::Entity entityA;
    fr::Entity entityB;
    float      impactForce;
};
```

**Publishing** an event from within a system:

```cpp
mRegistry->SendEvent(CollisionEvent { .entityA = a, .entityB = b, .impactForce = 50.f });
```

**Subscribing** to an event (typically in a system's constructor):

```cpp
class ResponseSystem : public fr::System {
public:
    ResponseSystem(const skr::Arc<fr::Registry>& registry) : System(registry) {
        mHandle = registry->AddEventListener<CollisionEvent>(
            [](const CollisionEvent& ev) {
                // respond to collision...
            });
    }

private:
    skr::Arc<fr::ListenerHandle> mHandle; // keep alive to remain subscribed
};
```

> Subscriptions are automatically removed when the `ListenerHandle` Arc is destroyed.

---

## API Reference

### FreyrExtension

`fr::FreyrExtension` integrates Freyr into a Skirnir application. Configure it inside `AddExtension<fr::FreyrExtension>(...)`.

```cpp
skr::ApplicationBuilder()
    .AddExtension<fr::FreyrExtension>([](fr::FreyrExtension& freyr) {
        freyr
            .WithOptions(/* see FreyrOptionsBuilder */)
            .WithComponent<MyComponent>()
            .WithPipeline([](fr::PipelineBuilder& pipeline) {
                pipeline.WithName("Main").WithRate(0.0f).WithSystem<MySystem>();
            });
    })
    .Build<MyApp>();
```

| Method | Description |
|--------|-------------|
| `WithComponent<T>()` | Register a component type (required before use) |
| `WithPipeline(fn)` | Configure a named pipeline and its systems via `PipelineBuilder` |
| `WithOptions(fn)` | Configure runtime options via `FreyrOptionsBuilder` |

---

### FreyrOptionsBuilder

Configures Freyr runtime parameters. All methods return `*this` for chaining.

| Method | Default | Description |
|--------|---------|-------------|
| `WithMaxEntities(n)` | 1,048,576 | Maximum number of live entities |
| `WithArchetypeChunkCapacity(n)` | 512 | Entities per archetype chunk (tune for task granularity) |
| `WithThreadCount(n)` | 4 | Worker thread count for parallel iteration |
| `WithAllPhysicalCores()` | — | Set thread count to physical core count |

```cpp
freyr.WithOptions([](fr::FreyrOptionsBuilder& opts) {
    opts.WithMaxEntities(1'000'000)
        .WithArchetypeChunkCapacity(512)
        .WithThreadCount(std::thread::hardware_concurrency());
});
```

---

### Registry

`fr::Registry` is the central orchestrator. Obtain it from the service provider:

```cpp
mRegistry = serviceProvider->GetService<fr::Registry>();
```

#### Entity Management

```cpp
Entity CreateEntity(const Ts&... components);
void   CreateEntity(TFunc&& callback, const Ts&... components);
void   DestroyEntity(const Entity& entity);  // deferred
```

#### Component Operations

```cpp
void AddComponent(const Entity entity, T component);
void AddComponents(const Entity entity, const Ts&... components);
void RemoveComponent<T>(const Entity entity);
bool HasComponent<T>(const Entity entity) const;
bool HasComponents<Ts...>(const Entity entity) const;
bool TryGetComponents<Ts...>(const Entity entity, auto&& callback);
```

#### Query (read) and Mutation (write)

```cpp
// Read-only query
auto count = registry->CreateQuery()->Count<Position, Velocity>();
auto ids   = registry->CreateQuery()->EntitiesWith<Position>();

// Write via mutation (sync or async per chunk)
registry->CreateMutation()->Each(
    [](fr::Entity, Position& pos, Velocity& vel) {
        pos.x += vel.dx;
    });

registry->CreateMutation()->EachAsync(
    [](fr::Entity, Position& pos) {
        pos.x *= 0.99f;
    });
registry->ExecuteTasks(); // wait for async chunk tasks when needed outside Update
```

#### Event Helpers

```cpp
skr::Arc<ListenerHandle> AddEventListener<T>(auto&& listener);
void                     SendEvent<T>(T event);
```

#### Update Loop

```cpp
void Update(float deltaTime);  // drives pipelines, systems, mutations, and deferred destroys
void ExecuteTasks();           // manually flush async tasks
```

---

### ArchetypeBuilder

`ArchetypeBuilder` efficiently creates large numbers of entities with the same component layout. Prefer it over individual `CreateEntity` calls when spawning thousands of entities at startup.

```cpp
auto archetype = registry->CreateArchetypeBuilder()
    .WithComponent(Position { .x = 0.f })    // component with initial value
    .WithComponent(Velocity { .dx = 1.f })
    .WithEntities(100'000)                   // number of entities to create
    .ForEach<Velocity>([](fr::Entity e, Velocity& vel) {
        vel.dx = static_cast<float>(e);      // per-entity customisation
    })
    .Build();                                // returns skr::Arc<Archetype> or nullptr if 0 entities
```

| Method | Description |
|--------|-------------|
| `WithComponent<T>(value)` | Add a component type with a default value for all entities |
| `WithEntities(count)` | Set the number of entities to create |
| `ForEach<Ts...>(fn)` | Post-creation callback to customize individual entities |
| `Build()` | Commit and return the archetype (`nullptr` if count is 0) |

Calling `Build()` on an existing archetype (same component signature) appends to it rather than creating a new one.

---

### EventManager

The `EventManager` implements a thread-safe publish/subscribe bus. It is accessed indirectly via `Registry::AddEventListener` and `Registry::SendEvent`, but can also be injected directly:

```cpp
class MySystem : public fr::System {
public:
    MySystem(const skr::Arc<fr::Registry>& registry, skr::Arc<fr::EventManager> events)
        : System(registry)
    {
        mHandle = events->Subscribe<DamageEvent>([](const DamageEvent& ev) {
            // handle damage
        });
    }

    void Update(float dt) override {
        // ...
        mRegistry->SendEvent(DamageEvent { .amount = 10.f });
    }

private:
    skr::Arc<fr::ListenerHandle> mHandle;
};
```

| Method | Description |
|--------|-------------|
| `Subscribe<T>(fn)` | Subscribe to event type `T`; returns a `ListenerHandle` |
| `Send<T>(event)` | Publish an event to all active subscribers |
| `Flush()` | Merge pending listeners and remove expired handles |

---

## Profiling

Freyr integrates with [Perfetto](https://perfetto.dev) for trace-based profiling. Enable it at configure time:

```cmake
cmake -B build -DFREYR_PROFILING=ON
```

Then wrap your update loop:

```cpp
mRegistry->BeginProfiling();

for (int i = 0; i < 1000; i++)
    mRegistry->Update(1.0f / 60.0f);

mRegistry->EndProfiling(); // writes trace to disk
```

Open the resulting `.pftrace` file in [ui.perfetto.dev](https://ui.perfetto.dev) to inspect system timings, chunk iteration durations, and thread utilisation.

Individual spans can also be added manually:

```cpp
mRegistry->BeginTrace("MyLabel");
// ... work ...
mRegistry->EndTrace();
```

---

## Examples

### Profiling Example

The `examples/Profiling` directory demonstrates batch entity creation, system registration, and profiling:

```cpp
mRegistry->CreateArchetypeBuilder()
    .WithComponent(Position {})
    .WithEntities(2'000'000)
    .Build();

mRegistry->CreateArchetypeBuilder()
    .WithComponent(Position {})
    .WithComponent(Velocity {})
    .WithEntities(2'000'000)
    .Build();

for (auto i = 0; i < 100; i++)
    mRegistry->Update(1.0f);
```

### Inter-System Communication

```cpp
struct CollisionEvent : fr::Event {
    fr::Entity entityA;
    fr::Entity entityB;
};

class CollisionSystem : public fr::System {
public:
    CollisionSystem(const skr::Arc<fr::Registry>& registry) : System(registry) {}

    void Update(float dt) override {
        // Broadphase / narrowphase omitted — publish results as events
        mRegistry->SendEvent(CollisionEvent { .entityA = a, .entityB = b });
    }
};

class ResponseSystem : public fr::System {
public:
    ResponseSystem(const skr::Arc<fr::Registry>& registry) : System(registry) {
        mHandle = registry->AddEventListener<CollisionEvent>(
            [](const CollisionEvent& ev) {
                // resolve collision between ev.entityA and ev.entityB
            });
    }

private:
    skr::Arc<fr::ListenerHandle> mHandle;
};
```
