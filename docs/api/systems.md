# Systems

Systems contain the **logic** of the simulation. Each system declares which components it needs and processes all matching entities during the update loop.

---

## Defining a system

Inherit from `fr::System` and override the lifecycle hooks you need:

```cpp
#include <Freyr/Freyr.hpp>

class MovementSystem : public fr::System {
public:
    explicit MovementSystem(const Ref<fr::Scene>& scene) : System(scene) {}

    void Update(float deltaTime) override {
        mScene->ForEachAsync<Position, Velocity>(
            [deltaTime](fr::Entity, Position& pos, const Velocity& vel) {
                pos.x += vel.dx * deltaTime;
                pos.y += vel.dy * deltaTime;
                pos.z += vel.dz * deltaTime;
            });
    }
};
```

The constructor **must** accept `const Ref<fr::Scene>&` as its first argument. Additional dependencies are resolved by Skirnir's DI container.

---

## Lifecycle hooks

Systems are called in this order every frame by `Scene::Update(dt)`:

```
PreUpdate(dt)
Update(dt)
PostUpdate(dt)
```

| Hook | Typical use |
|------|-------------|
| `PreUpdate(dt)` | Input gathering, reset accumulators |
| `Update(dt)` | Main simulation logic |
| `PostUpdate(dt)` | Post-processing, late reads |

All hooks have a default empty implementation — override only those you need.

---

## Dependency injection

Systems are singletons registered in Skirnir's DI container. Any service registered in the container can be injected via the constructor:

```cpp
class CollisionSystem : public fr::System {
public:
    CollisionSystem(const Ref<fr::Scene>& scene, Ref<fr::EventManager> events)
        : System(scene), mEvents(events) {}

    void FixedUpdate(float dt) override {
        mScene->ForEachAsync<Position, Collider>(
            [this](fr::Entity a, Position& posA, Collider& colA) {
                // detect collisions and publish events
                mScene->SendEvent(CollisionEvent { .entityA = a });
            });
    }

private:
    Ref<fr::EventManager> mEvents;
};
```

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

---

## System ID

Each system type has a unique runtime ID:

```cpp
fr::SystemId id = fr::GetSystemId<MovementSystem>();
```

---

## Accessing the scene

The protected `mScene` member provides access to all `Scene` operations:

```cpp
void Update(float dt) override {
    // iterate
    mScene->ForEach<Health>([](fr::Entity e, Health& hp) { ... });

    // create/destroy entities
    mScene->DestroyEntity(deadEnemy);

    // add/remove components
    mScene->AddComponent<DeadTag>(entity);

    // send events
    mScene->SendEvent(MyEvent {});
}
```

---

## Ordering and dependencies

Systems run in registration order, sequentially. If `SystemB` depends on results produced by `SystemA`, register `SystemA` first.

For data dependencies *within* a system, use `ForEach` (sequential) rather than `ForEachAsync` when the callback reads from other entities.
