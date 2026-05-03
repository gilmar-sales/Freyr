# FreyrExtension

`fr::FreyrExtension` integrates Freyr into a [Skirnir](https://github.com/gilmar-sales/skirnir) application. It registers all services (Scene, managers, thread pool) into the DI container and wires up components and systems before the application starts.

---

## Registration

Pass it to `skr::ApplicationBuilder::AddExtension` with a configuration lambda:

```cpp
skr::ApplicationBuilder()
    .AddExtension<fr::FreyrExtension>([](fr::FreyrExtension& freyr) {
        freyr
            .WithOptions([](fr::FreyrOptionsBuilder& opts) {
                opts.WithMaxEntities(500'000)
                    .WithThreadCount(4);
            })
            .WithComponent<Position>()
            .WithComponent<Velocity>()
            .WithPipeline([](fr::PipelineBuilder& pipeline) {
                pipeline.WithName("Main")
                    .WithRate(60.0f)
                    .WithSystem<MovementSystem>()
                    .WithSystem<CollisionSystem>();
            });
    })
    .Build<MyApp>();
```

All `With*` calls return `*this`, so they can be chained freely.

---

## Methods

### `WithComponent<T>()`

Registers a component type with the `ComponentManager`.

```cpp
freyr.WithComponent<TransformComponent>();
```

!!! warning
    Every component type used in the application **must** be registered before the first entity that uses it is created. Failing to register a component results in a runtime assertion.

**Template parameter:** `T` — must satisfy `fr::IsComponent` (i.e. inherit from `fr::Component`).

---

### `WithPipeline(fn)`

Defines a pipeline containing systems. Systems are constructed in registration order within each pipeline. If a system needs an `EventManager`, Skirnir resolves it automatically:

```cpp
class PhysicsSystem : public fr::System {
public:
    PhysicsSystem(const Ref<fr::Scene>& scene, Ref<fr::EventManager> events)
        : System(scene), mEvents(events) {}
    // ...
};
```

**Callback parameter:** `fn` — receives a `PipelineBuilder` to configure the pipeline.

See [`PipelineBuilder`](pipeline-builder.md) for pipeline configuration options.

---

### `WithOptions(fn)`

Configures runtime parameters via a `FreyrOptionsBuilder` callback.

```cpp
freyr.WithOptions([](fr::FreyrOptionsBuilder& opts) {
    opts.WithMaxEntities(1'000'000)
        .WithArchetypeChunkCapacity(512)
        .WithThreadCount(std::thread::hardware_concurrency());
});
```

See [`FreyrOptionsBuilder`](options-builder.md) for all available options.
