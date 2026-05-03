# Scene

`fr::Scene` is the central orchestrator of Freyr. It provides the full entity lifecycle API, component operations, event helpers, and the main update loop.

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
void AddComponents<Ts...>(const Entity entity, const Ts&... components);
```

Adds multiple components in one call. More efficient than calling `AddComponent` repeatedly.

```cpp
scene->AddComponents<Position, Velocity>(entity, Position {}, Velocity { .dx = 1.f });
```

---

### `RemoveComponent<T>`

```cpp
void RemoveComponent<T>(const Entity entity);
```

Removes a component and migrates the entity to the appropriate archetype.

```cpp
scene->RemoveComponent<Velocity>(entity); // entity stops moving
```

---

### `RemoveComponents<Ts...>`

```cpp
void RemoveComponents<Ts...>(const Entity entity);
```

Removes multiple components from an entity in a single call.

```cpp
scene->RemoveComponents<Velocity, Health>(entity);
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

## Queries

### `CreateQuery`

```cpp
Ref<Query> CreateQuery() const;
template <typename TQuery> requires(std::is_base_of_v<Query, TQuery>)
Ref<TQuery> CreateQuery();
```

Creates a new Query instance for entity searching. Query instances should not be stored long-term as they hold references to `ComponentManager`.

```cpp
auto query = scene->CreateQuery();
auto players = query->Excluding<DeadTag>()
    ->EntitiesWith<PlayerTag, Health>();
```

See the [Query reference](query.md) for the full fluent query API.

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

```
Scene::Update(dt)
├─ Flush() → merge pending event listeners
├─ StartWorkers()
├─ Accumulate(dt) → track elapsed time per pipeline
├─ PreUpdate(dt) → all systems across all pipelines
│  ├─ WaitForAllTasks()
│  └─ DestroyEntities()
├─ Update(dt)   → all systems across all pipelines (systems call Query::Each / EachAsync)
│  ├─ WaitForAllTasks()
│  └─ DestroyEntities()
└─ PostUpdate(dt) → all systems across all pipelines
   ├─ WaitForAllTasks()
   └─ DestroyEntities()
```

---

### `ExecuteTasks()`

```cpp
void ExecuteTasks();
```

Starts all workers, flushes the query aggregator, and waits for all enqueued chunk tasks to complete. Can be called manually to create explicit sync points between async `Query::EachAsync` calls.

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
