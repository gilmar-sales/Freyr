# Query

`fr::Query` provides a fluent API for filtering and querying entities by their component composition. Queries support include/exclude filters, various terminal operations for collecting or processing matching entities, and can be scheduled for async execution.

Obtain a Query instance via [`Scene::CreateQuery()`](scene.md#createquery):

```cpp
auto query = scene->CreateQuery();
```

---

## Filter configuration

### `Excluding<Ts...>`

```cpp
Query& Excluding() requires(IsComponent<Ts> and ...);
```

Adds component types to the **exclusion filter**. Entities with any of the specified components will be excluded from query results.

```cpp
query->Excluding<DisabledTag, EditorOnly>();
```

---

## Terminal operations

### `Count<Ts...>`

```cpp
std::size_t Count() requires(IsComponent<Ts> and ...);
```

Returns the total number of entities matching the query.

```cpp
auto alive = query.Count<Health>();
```

---

### `EntitiesWith<Ts...>`

```cpp
std::vector<Entity> EntitiesWith() requires(IsComponent<Ts> and ...);
```

Returns all entity IDs that have all specified component types.

```cpp
auto players = query->EntitiesWith<PlayerTag, Health>();
```

---

### `FindUnique<Ts...>`

```cpp
std::optional<Entity> FindUnique() requires(IsComponent<Ts> and ...);
```

Asserts that exactly one entity matches, then returns it. Returns `std::nullopt` if zero or more than one entity matches. Useful for singleton entities (camera, player).

```cpp
auto camera = query->FindUnique<CameraComponent>();
```

---

### `First<Ts...>`

```cpp
std::optional<Entity> First() requires(IsComponent<Ts> and ...);
```

Returns the first entity matching the query, or `std::nullopt` if none found.

```cpp
auto entity = query->First<PlayerTag>();
```

---

### `Iterate<Ts...>`

```cpp
std::vector<std::tuple<Entity, Ts...>> Iterate() requires(IsComponent<Ts> and ...);
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
auto Map(auto&& f) -> std::vector<decltype(f(...))>;
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
Query& Each(auto&& action) requires(IsComponent<Ts> and ...);
```

Synchronous iteration over all matching entities. Callback receives `(Entity, Ts&...)`.

```cpp
query->Including<Position, Velocity>()
    ->WithLabel("UpdatePositions")
    ->Each<Position, Velocity>([](Entity e, Position& pos, Velocity& vel) {
        pos.x += vel.dx * dt;
    });
```

---

### `EachAsync<Ts...>`

```cpp
Query& EachAsync(auto&& action) requires(IsComponent<Ts> and ...);
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
- The QueryAggregator coordinates async query execution across worker threads.