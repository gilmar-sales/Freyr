# Query

`fr::Query` provides a fluent API for filtering and querying entities by their component composition. Queries support exclusion filters and various terminal operations for collecting or processing matching entities.

Obtain a Query instance via [`Scene::CreateQuery()`](scene.md#createquery):

```cpp
auto query = scene->CreateQuery();
```

---

## Filter configuration

### `Excluding<Ts...>`

```cpp
template <typename... Ts>
    requires(IsComponent<Ts> and ...)
Query& Excluding();
```

Adds component types to the **exclusion filter**. Entities with any of the specified components will be excluded from query results.

```cpp
query->Excluding<DisabledTag, EditorOnly>();
```

Inclusion filters are specified implicitly via the component template arguments on terminal operations.

---

## Terminal operations

### `Count<Ts...>`

```cpp
template <typename... Ts>
    requires(IsComponent<Ts> and ...)
std::size_t Count();
```

Returns the total number of entities having all specified component types.

```cpp
auto alive = query->Count<Health>();
```

---

### `EntitiesWith<Ts...>`

```cpp
template <typename... Ts>
    requires(IsComponent<Ts> and ...)
std::vector<Entity> EntitiesWith();
```

Returns all entity IDs that have all specified component types.

```cpp
auto players = query->Excluding<DeadTag>()
    ->EntitiesWith<PlayerTag, Health>();
```

---

### `FindUnique<Ts...>`

```cpp
template <typename... Ts>
    requires(IsComponent<Ts> and ...)
std::optional<Entity> FindUnique();
```

Returns the single matching entity, or `std::nullopt` if zero or more than one entity matches. Useful for singleton entities (camera, player).

```cpp
auto camera = query->FindUnique<CameraComponent>();
```

---

### `First<Ts...>`

```cpp
template <typename... Ts>
    requires(IsComponent<Ts> and ...)
std::optional<Entity> First();
```

Returns the first entity matching the query, or `std::nullopt` if none found.

```cpp
auto entity = query->First<PlayerTag>();
```

---

### `Iterate<Ts...>`

```cpp
template <typename... Ts>
    requires(IsComponent<Ts> and ...)
auto Iterate() -> std::vector<std::tuple<Entity, Ts...>>;
```

Collects all matching entities and their components into a vector of tuples.

```cpp
auto entities = query->Iterate<Position, Velocity>();
for (auto&& [entity, pos, vel] : entities) {
    // ...
}
```

---

### `Transform<Ts...>`

```cpp
template <typename... Ts>
    requires(IsComponent<Ts> and ...)
auto Transform(auto&& callback) -> std::vector<decltype(callback(...))>;
```

Maps each entity to a transformed value. Callback can accept either `(Entity, Ts&...)` or `(Ts&...)`.

```cpp
auto distances = query->Transform<Position>([origin](Entity, Position& pos) {
    return std::sqrt(pos.x * pos.x + pos.y * pos.y);
});
```

---

### `Map<Ts...>`

```cpp
template <typename... Ts>
    requires(IsComponent<Ts> and ...)
auto Map(auto&& f) -> std::vector<decltype(callback(...))>;
```

Applies a transform function and returns results as a vector, ordered by entity.

```cpp
auto distances = query->Map<Position>([origin](Entity e, Position& pos) {
    float dx = pos.x - origin.x;
    float dy = pos.y - origin.y;
    return std::sqrt(dx*dx + dy*dy);
});
```

---

### `Reduce<Ts...>`

```cpp
template <typename... Ts>
    requires(IsComponent<Ts> and ...)
auto Reduce(auto&& callback, auto seed) -> decltype(seed);
```

Accumulates values across all matching entities. Callback signature: `(ResultType, Ts&...) -> ResultType`.

```cpp
auto totalHealth = query->Reduce<Health>([](float acc, Health& h) {
    return acc + h.current;
}, 0.f);
```

---

## Iteration

### `Each<Ts...>`

```cpp
template <typename... Ts>
    requires(IsComponent<Ts> and ...)
Query& Each(auto&& action);
```

Synchronous iteration over all matching entities. Callback receives `(Entity, Ts&...)`.

```cpp
query->WithLabel("UpdatePositions")
    ->Each<Position, Velocity>([](Entity e, Position& pos, Velocity& vel) {
        pos.x += vel.dx * dt;
    });
```

---

### `EachAsync<Ts...>`

```cpp
template <typename... Ts>
    requires(IsComponent<Ts> and ...)
Query& EachAsync(auto&& action);
```

Dispatches chunk tasks to the thread pool for parallel execution. Call `ExecuteTasks()` on the scene to wait for completion.

```cpp
query->EachAsync<Position, Velocity>([](Entity e, Position& pos, Velocity& vel) {
    pos.x += vel.dx * dt;
});
scene->ExecuteTasks();
```

---

## Utility

### `WithLabel`

```cpp
Query& WithLabel(const std::string_view name);
```

Assigns a human-readable label for profiling and debugging. When `FREYR_PROFILING=ON`, the label appears in Perfetto traces.

```cpp
query->WithLabel("PhysicsUpdate");
```

---

## Notes

- Query instances should not be stored long-term as they hold references to `ComponentManager`.
- Use `Scene::CreateQuery()` to obtain a fresh query instance when needed.
- The `QueryAggregator` coordinates async query execution across worker threads.