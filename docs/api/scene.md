# Scene

`fr::Scene` is the central orchestrator of Freyr. It provides the full entity lifecycle API, component operations, iteration methods, event helpers, and the main update loop.

Obtain an instance from the service provider:

```cpp
class MyApp : public skr::IApplication {
public:
    explicit MyApp(const Ref<skr::ServiceProvider>& sp) : IApplication(sp) {
        mScene = sp->GetService<fr::Scene>();
    }
};
```

---

## Entity management

### `CreateEntity`

```cpp
// Create with components, returns entity ID
Entity CreateEntity(const Ts&... components);

// Create with components and receive the ID via callback
void CreateEntity(TFunc&& callback, const Ts&... components);
```

```cpp
// No components
fr::Entity e = scene->CreateEntity();

// With initial components
fr::Entity e = scene->CreateEntity(Position { .x = 10.f }, Velocity {});

// Via callback
scene->CreateEntity([](fr::Entity e) {
    // e is available here immediately
}, Position {}, Velocity {});
```

---

### `DestroyEntity`

```cpp
void DestroyEntity(const Entity& entity);
```

Destruction is **deferred** — the entity is added to a pending set and removed at the end of the current `Update` call.

```cpp
scene->DestroyEntity(e);
// entity is still alive until Update() returns
```

---

## Component operations

### `AddComponent<T>`

```cpp
void AddComponent<T>(const Entity& entity, const T& component = {});
```

Adds a single component to an existing entity. If the entity does not yet belong to an archetype that includes `T`, it is migrated to the correct one.

```cpp
scene->AddComponent<Health>(entity, Health { .hp = 100 });
```

---

### `AddComponents<Ts...>`

```cpp
void AddComponents<Ts...>(const Entity& entity, const Ts&... components);
```

Adds multiple components in one call. More efficient than calling `AddComponent` repeatedly.

```cpp
scene->AddComponents<Position, Velocity>(entity, Position {}, Velocity { .dx = 1.f });
```

---

### `RemoveComponent<T>`

```cpp
void RemoveComponent<T>(const Entity& entity);
```

Removes a component and migrates the entity to the appropriate archetype.

```cpp
scene->RemoveComponent<Velocity>(entity); // entity stops moving
```

---

### `HasComponent<T>` / `HasComponents<Ts...>`

```cpp
bool HasComponent<T>(const Entity& entity) const;
bool HasComponents<Ts...>(const Entity& entity) const;
```

```cpp
if (scene->HasComponents<Position, Velocity>(entity)) {
    // safe to iterate
}
```

---

### `TryGetComponents<Ts...>`

```cpp
bool TryGetComponents<Ts...>(const Entity& entity, auto&& callback);
```

Invokes `callback(Ts&...)` only if the entity has all requested components. Returns `true` on success.

```cpp
bool found = scene->TryGetComponents<Position, Health>(entity,
    [](Position& pos, Health& hp) {
        pos.x += 1.f;
        hp.current -= 5;
    });
```

---

## Iteration

All iteration methods accept callbacks with signature `(fr::Entity entity, Ts&... components)`.

### `ForEach<Ts...>`

Sequential iteration over all entities that have every component in `Ts`.

```cpp
scene->ForEach<Position, Velocity>([](fr::Entity, Position& pos, Velocity& vel) {
    pos.x += vel.dx;
});

// Labeled overload (used in profiling traces)
scene->ForEach<Position>("UpdatePositions", fn);
```

---

### `ForEachParallel<Ts...>`

Parallel iteration. Chunks are distributed across the thread pool and processed concurrently. The callback must be thread-safe (no shared mutable state between entities).

```cpp
scene->ForEachParallel<Position, Velocity>([dt](fr::Entity, Position& pos, Velocity& vel) {
    pos.x += vel.dx * dt;
});
```

!!! warning "Thread safety"
    The callback is invoked concurrently for different entities. Accessing shared state (e.g. a global counter) requires synchronisation. Modifying components of *other* entities from within the callback is not safe.

---

### `ForEachAsync<Ts...>`

Dispatches chunk tasks to the thread pool without blocking. Call `ExecuteTasks()` to wait for all dispatched tasks to finish.

```cpp
scene->ForEachAsync<Velocity>([](fr::Entity, Velocity& vel) {
    vel.dx *= 0.99f;
});

// do other work here...

scene->ExecuteTasks(); // sync point
```

---

### `Map<Ts...>`

Applies a transform function and returns the results as `std::vector`.

```cpp
auto distances = scene->Map<Position>([origin](fr::Entity, Position& pos) {
    float dx = pos.x - origin.x;
    float dy = pos.y - origin.y;
    return std::sqrt(dx*dx + dy*dy);
});
// distances is std::vector<float>
```

---

## Queries

### `Count<Ts...>`

```cpp
std::size_t Count<Ts...>();
```

Returns the number of entities that have all components in `Ts`.

```cpp
auto alive = scene->Count<Health>();
```

---

### `EntitiesWith<Ts...>`

```cpp
std::vector<Entity> EntitiesWith<Ts...>();
```

Returns all entity IDs that have all components in `Ts`.

```cpp
auto players = scene->EntitiesWith<PlayerTag, Health>();
```

---

### `FindUnique<Ts...>`

```cpp
Entity FindUnique<Ts...>();
```

Asserts that exactly one entity matches, then returns it. Useful for singleton entities (camera, player).

```cpp
auto camera = scene->FindUnique<CameraComponent>();
```

---

## Event helpers

### `AddEventListener<T>`

```cpp
Ref<ListenerHandle> AddEventListener<T>(auto&& listener);
```

Subscribes to event type `T`. The subscription is alive as long as the returned `ListenerHandle` is kept alive.

```cpp
mHandle = scene->AddEventListener<CollisionEvent>(
    [](const CollisionEvent& ev) { /* ... */ });
```

---

### `SendEvent<T>`

```cpp
void SendEvent<T>(T event);
```

Publishes an event immediately to all active subscribers.

```cpp
scene->SendEvent(CollisionEvent { .entityA = a, .entityB = b });
```

---

## Update loop

### `Update(deltaTime)`

```cpp
void Update(float deltaTime);
```

Drives the full update cycle:

1. `PreUpdate` → `Update` → `PostUpdate` for all systems
2. Fixed-timestep accumulation → `PreFixedUpdate` → `FixedUpdate` → `PostFixedUpdate`
3. `ExecuteTasks()` — flush pending async tasks
4. Deferred entity destruction

---

### `ExecuteTasks()`

```cpp
void ExecuteTasks();
```

Blocks until all tasks dispatched via `ForEachAsync` have completed. Called automatically at the end of `Update`, but can be called manually to create explicit sync points.

---

## Archetype builder

```cpp
ArchetypeBuilder CreateArchetypeBuilder() const;
```

Returns an `ArchetypeBuilder` for efficient bulk entity creation. See the [ArchetypeBuilder reference](archetype-builder.md).

---

## Profiling

```cpp
void BeginProfiling();
void EndProfiling() const;   // flushes trace to disk
void BeginTrace(const char* label);
void EndTrace();
```

See the [Profiling guide](../guides/profiling.md).
