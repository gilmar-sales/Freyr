# Systems

Systems contain the **logic** of the simulation. Each system declares which components it needs and processes
all matching entities during the update loop.

---

## Defining a system

Inherit from `fr::System` and override the lifecycle hooks you need:

```cpp
#include <Freyr/Freyr.hpp>

class MovementSystem : public fr::System {
public:
    explicit MovementSystem(const Ref<fr::Registry>& registry) : System(registry) {}

    void Update(float deltaTime) override {
        mRegistry->CreateQuery()->EachAsync<Position, Velocity>(
            [deltaTime](fr::Entity, Position& pos, const Velocity& vel) {
                pos.x += vel.dx * deltaTime;
                pos.y += vel.dy * deltaTime;
                pos.z += vel.dz * deltaTime;
            });
    }
};
```

The constructor **must** accept `const Ref<fr::Registry>&` as its first argument. Additional dependencies are
resolved by Skirnir's DI container.

---

## Lifecycle hooks

```mermaid
graph TB
    subgraph Frame["Single Frame Update"]
        subgraph PreUpdatePhase["PreUpdate Phase"]
            PU["System::PreUpdate(dt)"]
            PU_WAIT["WaitForAllTasks()"]
            PU_DESTROY["DestroyEntities()"]
        end

        subgraph UpdatePhase["Update Phase"]
            U["System::Update(dt)"]
            U_QUERY["← Query::Each / EachAsync"]
            U_WAIT["WaitForAllTasks()"]
            U_DESTROY["DestroyEntities()"]
        end

        subgraph PostUpdatePhase["PostUpdate Phase"]
            POU["System::PostUpdate(dt)"]
            POU_WAIT["WaitForAllTasks()"]
            POU_DESTROY["DestroyEntities()"]
        end

        PU --> PU_WAIT --> PU_DESTROY --> U --> U_QUERY --> U_WAIT --> U_DESTROY --> POU --> POU_WAIT --> POU_DESTROY
    end

```

Systems are called in this order every frame by `Registry::Update(dt)`:

```
PreUpdate(dt) → Update(dt) → PostUpdate(dt)
```

| Hook             | Typical use                         | Notes |
|------------------|-------------------------------------|-------|
| `PreUpdate(dt)`  | Input gathering, reset accumulators | Runs before main work |
| `Update(dt)`     | Main simulation logic               | Primary hook — most systems use this |
| `PostUpdate(dt)` | Post-processing, late reads         | Runs after all Update work |

All hooks have a default empty implementation — override only those you need.

---

## Dependency injection

Systems are singletons registered in Skirnir's DI container. Any service registered in the container can be
injected via the constructor:

```cpp
class CollisionSystem : public fr::System {
public:
    CollisionSystem(const Ref<fr::Registry>& registry,           // required: first parameter
                    Ref<fr::EventManager> events)          // injected automatically
        : System(registry), mEvents(events) {}

    void Update(float dt) override {
        mRegistry->CreateQuery()->EachAsync<Position, Collider>(
            [this](fr::Entity a, Position& posA, Collider& colA) {
                // detect collisions and publish events
                mRegistry->SendEvent(CollisionEvent { .entityA = a });
            });
    }

private:
    Ref<fr::EventManager> mEvents;
};
```

### Injectable types

Any service registered via Skirnir can be injected:

- `Ref<fr::Registry>` — the registry (always available)
- `Ref<fr::EventManager>` — event bus
- `Ref<fr::ComponentManager>` — component registry
- Custom services registered in your application

---

## Registration

Register systems within a pipeline using `FreyrExtension::WithPipeline()`:

```cpp
freyr
    .WithPipeline([](fr::PipelineBuilder& pipeline) {
        pipeline.WithName("Main")
            .WithRate(60.0f)
            .WithSystem<InputSystem>()      // registered first, runs first
            .WithSystem<MovementSystem>()
            .WithSystem<CollisionSystem>()
            .WithSystem<RenderSystem>();    // registered last, runs last
    });
```

Systems are instantiated and wired in registration order. Multiple pipelines can be defined with different rates.

### What registration does

`WithSystem<T>()` performs two steps:

1. **Registers a factory** in `SystemManager`: `mSystemFactories[GetSystemId<T>()] = fn`
2. **Registers a singleton** in Skirnir's DI container: `services.AddSingleton<T>()`

This means the first pipeline that declares a system "owns" it. If a system appears in multiple pipelines,
it is constructed once and shared.

---

## System ID

Each system type has a unique runtime ID, assigned at first use:

```cpp
fr::SystemId id = fr::GetSystemId<MovementSystem>();
```

IDs are assigned sequentially in declaration order across translation units.

---

## Accessing the registry

The protected `mRegistry` member provides access to all `Registry` operations:

```cpp
class HealthSystem : public fr::System {
public:
    explicit HealthSystem(const Ref<fr::Registry>& registry) : System(registry) {}

    void Update(float dt) override {
        // Query entities
        mRegistry->CreateQuery()->Each<Health>([](fr::Entity e, Health& hp) {
            hp.current = std::min(hp.current + hp.regen * dt, hp.max);
        });

        // Destroy entities
        mRegistry->CreateQuery()->Each<Health>([this](fr::Entity e, Health& hp) {
            if (hp.current <= 0)
                mRegistry->DestroyEntity(e);
        });

        // Add/remove components
        mRegistry->CreateQuery()->Each<Health>([this](fr::Entity e, Health& hp) {
            if (hp.isPoisoned)
                mRegistry->RemoveComponent<PoisonedTag>(e);
        });

        // Send events
        mRegistry->SendEvent(HealthCheckEvent {});
    }
};
```

---

## System execution patterns

### 1. Parallel processing (preferred)

```cpp
void Update(float dt) override {
    mRegistry->CreateQuery()->EachAsync<Position, Velocity>(
        [dt](fr::Entity, Position& pos, Velocity& vel) {
            pos.x += vel.dx * dt;
        });
}
```

Use when entities are independent. Chunks are distributed across all worker threads.

### 2. Sequential processing

```cpp
void Update(float dt) override {
    mRegistry->CreateQuery()->Each<Position, Velocity>(
        [dt](fr::Entity e1, Position& pos1, Velocity& vel1) {
            // safe to read/write other entities
            mRegistry->CreateQuery()->Each<Position>(
                [&](fr::Entity e2, Position& pos2) {
                    // compute interaction between e1 and e2
                });
        });
}
```

Use when entities interact. Runs on the calling thread.

### 3. Mixed parallelism

```cpp
void Update(float dt) override {
    // Parallel: movement is independent per entity
    mRegistry->CreateQuery()->EachAsync<Position, Velocity>(
        [dt](fr::Entity e, Position& p, Velocity& v) {
            p.x += v.dx * dt;
        });

    // Sequential: AI may read other entities
    mRegistry->CreateQuery()->Each<AIState>(
        [this](fr::Entity e, AIState& ai) {
            ai.think(mRegistry);
        });

    // Sync parallel work
    mRegistry->ExecuteTasks();
}
```

---

## Ordering and dependencies

Systems run in registration order, sequentially per pipeline phase. If `SystemB` depends on results produced
by `SystemA`, register `SystemA` first:

```cpp
pipeline
    .WithSystem<PhysicsSystem>()     // computes positions
    .WithSystem<CollisionSystem>()   // reads positions
    .WithSystem<RenderSystem>();     // reads everything
```

!!! warning "Pipeline execution is sequential"
    While systems within a pipeline run sequentially, multiple pipelines accumulate time independently.
    Two pipelines at different rates may interleave their execution across frames.
