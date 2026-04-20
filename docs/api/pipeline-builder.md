# PipelineBuilder

`PipelineBuilder` defines a named collection of systems that run at a specific update rate. Pipelines allow you to organize systems into logical groups with different execution frequencies.

Obtain a builder from `FreyrExtension::WithPipeline()`:

```cpp
freyr.WithPipeline([](fr::PipelineBuilder& pipeline) {
    pipeline.WithName("Main")
        .WithRate(60.0f)
        .WithSystem<MovementSystem>()
        .WithSystem<RenderSystem>();
});
```

---

## Methods

### `WithName(name)`

Sets the pipeline's display name, used in profiling traces.

| Parameter | Type | Default |
|-----------|------|---------|
| `name` | `std::string_view` | `"Pipeline {id}"` |

```cpp
pipeline.WithName("Physics");
```

---

### `WithRate(rate)`

Sets the target update rate for this pipeline in Hz. The pipeline will be scheduled to run at this frequency.

| Parameter | Type | Default |
|-----------|------|---------|
| `rate` | `float` | `0.0` (runs every frame) |

```cpp
pipeline.WithRate(60.0f); // 60 Hz
pipeline.WithRate(30.0f); // 30 Hz
pipeline.WithRate(0.0f);  // every frame
```

---

### `WithSystem<T>()`

Registers a system type to this pipeline. Systems are constructed in registration order.

```cpp
pipeline.WithSystem<InputSystem>()
        .WithSystem<MovementSystem>()
        .WithSystem<CollisionSystem>();
```

**Template parameter:** `T` — must satisfy `fr::IsSystem` (i.e. inherit from `fr::System`).

Systems are automatically added as singletons to the DI container and can receive injected dependencies:

```cpp
class PhysicsSystem : public fr::System {
public:
    PhysicsSystem(const Ref<fr::Scene>& scene, Ref<fr::EventManager> events)
        : System(scene), mEvents(events) {}

    void Update(float dt) override {
        // ...
    }

private:
    Ref<fr::EventManager> mEvents;
};
```

---

## Multiple pipelines

You can define multiple pipelines with different rates:

```cpp
freyr
    .WithPipeline([](fr::PipelineBuilder& physics) {
        physics.WithName("Physics")
            .WithRate(60.0f)
            .WithSystem<PhysicsSystem>()
            .WithSystem<CollisionSystem>();
    })
    .WithPipeline([](fr::PipelineBuilder& render) {
        render.WithName("AI")
            .WithRate(5.0f)
            .WithSystem<MonsterSystem>()
            .WithSystem<NPCSystem>();
    });
```

---

## System execution order

Within a pipeline, systems execute in registration order. If `SystemB` depends on results produced by `SystemA`, register `SystemA` first:

```cpp
pipeline.WithSystem<InputSystem>()      // runs first
        .WithSystem<MovementSystem>()   // uses input
        .WithSystem<CollisionSystem>()  // uses movement
        .WithSystem<RenderSystem>();    // runs last
```

For data dependencies *within* a system iteration, use `ForEach` (sequential) rather than `ForEachAsync` when the callback reads from other entities.
