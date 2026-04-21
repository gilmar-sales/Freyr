# Freyr

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
  - [Scene](#scene)
  - [ArchetypeBuilder](#archetypebuilder)
  - [EventManager](#eventmanager)
- [Execution Strategies](#execution-strategies)
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
Scene (orchestrator)
├── ComponentManager  → organizes entities into Archetypes → Chunks
├── EntityManager     → creates and recycles entity IDs
├── SystemManager     → registers and drives system lifecycle
├── EventManager      → publish/subscribe event bus
└── ThreadPool       → thread pool + lock-free MPMC queues

Update loop:
  System::PreUpdate / Update / PostUpdate
  System::PreFixedUpdate / FixedUpdate / PostFixedUpdate
  ForEach / ForEachParallel / ForEachAsync (per-archetype iteration)
  Event publishing
  Deferred entity destruction
```

---

## Requirements

- C++23 compatible compiler (GCC 13+, Clang 16+, MSVC 19.37+)
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
    explicit MovementSystem(const Ref<fr::Scene>& scene) : System(scene) {}

    void Update(float deltaTime) override {
        mScene->ForEach<Position, Velocity>([deltaTime](fr::Entity, Position& pos, Velocity& vel) {
            pos.x += vel.dx * deltaTime;
            pos.y += vel.dy * deltaTime;
            pos.z += vel.dz * deltaTime;
        });
    }
};

// 3. Define the application
class MyApp : public skr::IApplication {
public:
    explicit MyApp(const Ref<skr::ServiceProvider>& sp) : IApplication(sp) {
        mScene = sp->GetService<fr::Scene>();

        // Bulk-create 100,000 entities using ArchetypeBuilder
        mScene->CreateArchetypeBuilder()
            .WithComponent(Position {})
            .WithComponent(Velocity { .dx = 1.f })
            .WithEntities(100'000)
            .Build();
    }

    void Run() override {
        while (true)
            mScene->Update(1.0f / 60.0f);
    }

private:
    Ref<fr::Scene> mScene;
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
                .WithSystem<MovementSystem>();
        })
        .Build<MyApp>();

    app->Run();
}
```

---

## Core Concepts

### Entities

An entity is a plain integer ID (`fr::Entity`, aliased to `uint64_t`). It has no data of its own — its identity comes from the components attached to it.

> **Note:** An entity's internal ID may change during runtime as the memory layout is optimised. Do not store raw IDs long-term across frames if components are being added or removed.

```cpp
// Create an entity without components
fr::Entity e = scene->CreateEntity();

// Create an entity with components (returns immediately)
fr::Entity e = scene->CreateEntity(Position { .x = 10.f }, Velocity {});

// Create an entity and receive it via callback
scene->CreateEntity([](fr::Entity e) { /* use e */ }, Position {});

// Destroy (deferred — processed at end of Update)
scene->DestroyEntity(e);
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

Component operations on a `Scene`:

```cpp
// Add a single component
scene->AddComponent<Position>(entity, Position { .x = 5.f });

// Add multiple components at once
scene->AddComponents<Position, Velocity>(entity, Position {}, Velocity {});

// Remove a component (triggers archetype migration)
scene->RemoveComponent<Velocity>(entity);

// Query presence
bool has = scene->HasComponent<Position>(entity);
bool hasAll = scene->HasComponents<Position, Velocity>(entity);

// Access components safely via callback (returns false if not found)
bool found = scene->TryGetComponents<Position, Velocity>(entity,
    [](Position& pos, Velocity& vel) {
        pos.x += vel.dx;
    });
```

### Systems

Systems inherit from `fr::System` and override one or more lifecycle hooks:

| Method | Called |
|--------|--------|
| `PreUpdate(dt)` | Before the main update |
| `Update(dt)` | Main update step |
| `PostUpdate(dt)` | After the main update |
| `PreFixedUpdate(dt)` | Before the fixed timestep update |
| `FixedUpdate(dt)` | Fixed timestep update (default 1/50 s) |
| `PostFixedUpdate(dt)` | After the fixed timestep update |

```cpp
class PhysicsSystem : public fr::System {
public:
    explicit PhysicsSystem(const Ref<fr::Scene>& scene) : System(scene) {}

    void FixedUpdate(float deltaTime) override {
        // Parallel iteration over all entities with both components
        mScene->ForEachParallel<Position, Velocity>(
            [deltaTime](fr::Entity, Position& pos, const Velocity& vel) {
                pos.x += vel.dx * deltaTime;
            });
    }
};
```

Register systems via `FreyrExtension::WithSystem<T>()`. Systems are instantiated as singletons and injected with their dependencies via Skirnir's DI container.

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
mScene->SendEvent(CollisionEvent { .entityA = a, .entityB = b, .impactForce = 50.f });
```

**Subscribing** to an event (typically in a system's constructor):

```cpp
class ResponseSystem : public fr::System {
public:
    ResponseSystem(const Ref<fr::Scene>& scene) : System(scene) {
        mHandle = scene->AddEventListener<CollisionEvent>(
            [](const CollisionEvent& ev) {
                // respond to collision...
            });
    }

private:
    Ref<fr::ListenerHandle> mHandle; // keep alive to remain subscribed
};
```

> Subscriptions are automatically removed when the `ListenerHandle` shared_ptr is destroyed.

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
            .WithSystem<MySystem>();
    })
    .Build<MyApp>();
```

| Method | Description |
|--------|-------------|
| `WithComponent<T>()` | Register a component type (required before use) |
| `WithSystem<T>()` | Register a system type and add it to the DI container |
| `WithOptions(fn)` | Configure runtime options via `FreyrOptionsBuilder` |

---

### FreyrOptionsBuilder

Configures Freyr runtime parameters. All methods return `*this` for chaining.

| Method | Default | Description |
|--------|---------|-------------|
| `WithMaxEntities(n)` | 16,777,216 | Maximum number of live entities |
| `WithArchetypeChunkCapacity(n)` | 512 | Entities per archetype chunk (tune for task granularity) |
| `WithThreadCount(n)` | 4 | Worker thread count for parallel iteration |
| `WithFixedDeltaTime(dt)` | 1/50 s | Fixed timestep interval in seconds |
| `WithExecutionStrategy(s)` | `ChunkAffinity` | Task scheduling strategy (see [Execution Strategies](#execution-strategies)) |

```cpp
freyr.WithOptions([](fr::FreyrOptionsBuilder& opts) {
    opts.WithMaxEntities(1'000'000)
        .WithArchetypeChunkCapacity(512)
        .WithThreadCount(std::thread::hardware_concurrency())
        .WithFixedDeltaTime(1.0f / 60.0f)
        .WithExecutionStrategy(fr::FreyrExecutionStategy::ChunkAffinity);
});
```

---

### Scene

`fr::Scene` is the central orchestrator. Obtain it from the service provider:

```cpp
mScene = serviceProvider->GetService<fr::Scene>();
```

#### Entity Management

```cpp
Entity CreateEntity(const Ts&... components);
void   CreateEntity(TFunc&& callback, const Ts&... components);
void   DestroyEntity(const Entity& entity);  // deferred
```

#### Component Operations

```cpp
void AddComponent<T>(const Entity& entity, const T& component = {});
void AddComponents<Ts...>(const Entity& entity, const Ts&... components);
void RemoveComponent<T>(const Entity& entity);
bool HasComponent<T>(const Entity& entity) const;
bool HasComponents<Ts...>(const Entity& entity) const;
bool TryGetComponents<Ts...>(const Entity& entity, auto&& callback);
```

#### Iteration

| Method | Description |
|--------|-------------|
| `ForEach<Ts...>(fn)` | Sequential iteration over matching entities |
| `ForEachParallel<Ts...>(fn)` | Parallel iteration using the thread pool |
| `ForEachAsync<Ts...>(fn)` | Asynchronous task dispatch (call `ExecuteTasks()` to sync) |
| `Map<Ts...>(fn)` | Functional transform — returns `std::vector` of results |

All iteration callbacks receive `(fr::Entity entity, Ts&... components)`.

```cpp
// Sequential
scene->ForEach<Position, Velocity>([](fr::Entity e, Position& pos, Velocity& vel) {
    pos.x += vel.dx;
});

// Parallel (thread-safe per entity)
scene->ForEachParallel<Position>([](fr::Entity, Position& pos) {
    pos.x *= 0.99f;
});

// Async (fire-and-forget, sync manually)
scene->ForEachAsync<Velocity>([](fr::Entity, Velocity& vel) {
    vel.dx *= 0.9f;
});
scene->ExecuteTasks(); // wait for async tasks to complete

// Map to a new vector
auto positions = scene->Map<Position>([](fr::Entity, Position& pos) {
    return pos.x + pos.y;
}); // returns std::vector<float>
```

Labeled overloads are available for profiling:

```cpp
scene->ForEach<Position>("UpdatePositions", fn);
```

#### Query

```cpp
std::size_t         Count<Ts...>();
std::vector<Entity> EntitiesWith<Ts...>();
Entity              FindUnique<Ts...>(); // asserts exactly one match
```

#### Event Helpers

```cpp
Ref<ListenerHandle> AddEventListener<T>(auto&& listener);
void                SendEvent<T>(T event);
```

#### Update Loop

```cpp
void Update(float deltaTime);  // drives all systems and tasks
void ExecuteTasks();           // manually flush async tasks
```

---

### ArchetypeBuilder

`ArchetypeBuilder` efficiently creates large numbers of entities with the same component layout. Prefer it over individual `CreateEntity` calls when spawning thousands of entities at startup.

```cpp
auto archetype = scene->CreateArchetypeBuilder()
    .WithComponent(Position { .x = 0.f })    // component with initial value
    .WithComponent(Velocity { .dx = 1.f })
    .WithEntities(100'000)                   // number of entities to create
    .ForEach<Velocity>([](fr::Entity e, Velocity& vel) {
        vel.dx = static_cast<float>(e);      // per-entity customisation
    })
    .Build();                                // returns Ref<Archetype> or nullptr if 0 entities
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

The `EventManager` implements a thread-safe publish/subscribe bus. It is accessed indirectly via `Scene::AddEventListener` and `Scene::SendEvent`, but can also be injected directly:

```cpp
class MySystem : public fr::System {
public:
    MySystem(const Ref<fr::Scene>& scene, Ref<fr::EventManager>& events)
        : System(scene)
    {
        mHandle = events->Subscribe<DamageEvent>([](const DamageEvent& ev) {
            // handle damage
        });
    }

    void Update(float dt) override {
        // ...
        mScene->SendEvent(DamageEvent { .amount = 10.f });
    }

private:
    Ref<fr::ListenerHandle> mHandle;
};
```

| Method | Description |
|--------|-------------|
| `Subscribe<T>(fn)` | Subscribe to event type `T`; returns a `ListenerHandle` |
| `Send<T>(event)` | Publish an event to all active subscribers |
| `Cleanup()` | Remove expired listener handles (called automatically) |

---

## Execution Strategies

Freyr supports two parallel task scheduling strategies, configured via `WithExecutionStrategy`:

| Strategy | Enum value | Description |
|----------|------------|-------------|
| **ChunkAffinity** *(default)* | `FreyrExecutionStategy::ChunkAffinity` | Tasks are pinned to the worker thread that last processed the same chunk. Maximises L1/L2 cache reuse across frames. |
| **DispatchOrder** | `FreyrExecutionStategy::DispatchOrder` | Tasks are dispatched in creation order across workers. Simpler scheduling with predictable ordering. |

```cpp
opts.WithExecutionStrategy(fr::FreyrExecutionStategy::ChunkAffinity);
```

**Tuning tip:** The `ArchetypeChunkCapacity` setting directly controls task granularity. Smaller chunks mean more tasks (better load balancing), while larger chunks reduce scheduling overhead. The ideal value depends on your workload and hardware — benchmark with values between 128 and 4096.

---

## Profiling

Freyr integrates with [Perfetto](https://perfetto.dev) for trace-based profiling. Enable it at compile time:

```cmake
target_compile_definitions(your_target PRIVATE FREYR_PROFILING)
```

Then wrap your update loop:

```cpp
mScene->BeginProfiling();

for (int i = 0; i < 1000; i++)
    mScene->Update(1.0f / 60.0f);

mScene->EndProfiling(); // writes trace to disk
```

Open the resulting `.perfetto-trace` file in [ui.perfetto.dev](https://ui.perfetto.dev) to inspect system timings, chunk iteration durations, and thread utilisation.

Individual spans can also be added manually:

```cpp
mScene->BeginTrace("MyLabel");
// ... work ...
mScene->EndTrace();
```

---

## Examples

### Profiling Example

The `examples/Profiling` directory demonstrates batch entity creation, system registration, and profiling:

```cpp
// Create 2M entities with Position only, and 2M with Position + Velocity
mScene->CreateArchetypeBuilder()
    .WithComponent(Position {})
    .WithEntities(2'000'000)
    .Build();

mScene->CreateArchetypeBuilder()
    .WithComponent(Position {})
    .WithComponent(Velocity {})
    .WithEntities(2'000'000)
    .Build();

for (auto i = 0; i < 100; i++)
    mScene->Update(1.0f);
```

### Inter-System Communication

```cpp
struct CollisionEvent : fr::Event {
    fr::Entity entityA;
    fr::Entity entityB;
};

class CollisionSystem : public fr::System {
public:
    CollisionSystem(const Ref<fr::Scene>& scene) : System(scene) {}

    void Update(float dt) override {
        mScene->ForEach<Position>([this](fr::Entity a, Position& posA) {
            mScene->ForEach<Position>([this, a](fr::Entity b, Position& posB) {
                if (a != b && /* overlap check */ false)
                    mScene->SendEvent(CollisionEvent { a, b });
            });
        });
    }
};

class ResponseSystem : public fr::System {
public:
    ResponseSystem(const Ref<fr::Scene>& scene) : System(scene) {
        mHandle = scene->AddEventListener<CollisionEvent>(
            [](const CollisionEvent& ev) {
                // resolve collision between ev.entityA and ev.entityB
            });
    }

private:
    Ref<fr::ListenerHandle> mHandle;
};
```
