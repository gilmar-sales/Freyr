# Parallel Processing

Freyr provides two main patterns for processing entities in parallel: **Queries** for flexible filtering with complex
operations, and **ForEach** for direct iteration. Choosing the right pattern and configuring chunk size correctly are
the most impactful performance decisions you can make.

---

## Queries — flexible filtering

[`fr::Query`](api/query.md) provides a fluent API for filtering and querying entities by their component composition.
Queries are ideal when you need complex filters or aggregation operations.

### Creating a Query

```cpp
auto query = mScene->CreateQuery();
```

### Configuring filters

```cpp
query->Including<Position, Velocity>()    // entities with Position AND Velocity
    ->Excluding<DisabledTag, EditorOnly>(); // excluding entities with these components
```

### Terminal operations

| Method                          | Description                                                  |
|---------------------------------|--------------------------------------------------------------|
| `Count<Ts...>()`                | Returns total number of matching entities                    |
| `EntitiesWith<Ts...>()`         | Returns vector of entity IDs                                 |
| `FindUnique<Ts...>()`           | Returns exactly one entity (or `std::nullopt`)               |
| `First<Ts...>()`                | Returns first entity or `std::nullopt`                       |
| `Iterate<Ts...>()`              | Collects all entities and components into a vector of tuples |
| `Transform<Ts...>(callback)`    | Maps each entity to a transformed value                      |
| `Map<Ts...>(callback)`          | Applies transform and returns vector ordered by entity       |
| `Reduce<Ts...>(callback, seed)` | Accumulates values across matching entities                  |

### Example: counting and aggregation

```cpp
auto query = mScene->CreateQuery();

// Count alive entities
auto aliveCount = query->Including<Health>()
    ->Excluding<DeadTag>()
    ->Count<Health>();

// Reduce to total health
auto totalHealth = query->Including<Health>()
    ->Reduce<Health>([](float acc, Health& h) {
        return acc + h.current;
    }, 0.f);
```

---

## Synchronous iteration with `Each`

```cpp
query->Including<Position, Velocity>()
    ->WithLabel("Physics::Integrate")
    ->Each<Position, Velocity>([](fr::Entity e, Position& pos, Velocity& vel) {
        pos.x += vel.dx * dt;
        pos.y += vel.dy * dt;
    });
```

Processes entities one at a time, in chunk order. Safe for any operation, including read/write to other entities.

---

## Asynchronous iteration with `EachAsync`

```cpp
// schedule async iteration
query->Including<Position, Velocity>()
    ->EachAsync<Position, Velocity>([](fr::Entity e, Position& pos, Velocity& vel) {
        pos.x += vel.dx * dt;
        pos.y += vel.dy * dt;
    });
    
// start task execution
mScene->ExecuteTasks();

// do sequential work while tasks run...
mScene->CreateQuery()->Each<AIState>([](fr::Entity, AIState& ai) { ... });
    
// sync before reading results
mScene->Sync();
```

Use `EachAsync` to overlap CPU work: start a long parallel computation, do unrelated sequential work, then sync.

---

## Direct iteration with `ForEach`

For direct iteration without creating a query first, use the methods on [`Scene`](api/scene.md):

```cpp
// Synchronous — ordered logic, cross-entity writes, debugging
mScene->CreateQuery()->Each<Position, Velocity>([dt](fr::Entity e, Position& pos, Velocity& vel) {
    pos.x += vel.dx * dt;
});

// Asynchronous — fire-and-forget; sync later with ExecuteTasks()
mScene->CreateQuery()->EachAsync<Velocity>([](fr::Entity e, Velocity& vel) {
    vel.dx *= 0.99f;
});
```

| Method      | Blocking | Thread pool | Use when                                 |
|-------------|----------|-------------|------------------------------------------|
| `Each`      | Yes      | No          | Query with complex filters, synchronous  |
| `EachAsync` | No       | Yes         | Query with complex filters, asynchronous |

---

## Labels for profiling

All iteration methods accept an optional label that appears in Perfetto traces:

```cpp
query->WithLabel("Physics::Integrate")->EachAsync<...>(fn);
```

See the [profiling guide](profiling.md) for details.

---

## Performance Tuning

### Chunk size

`ArchetypeChunkCapacity` controls the number of entities per chunk, which directly determines task granularity:

```
Total entities: 1 000 000
Chunk capacity: 512  → ~1 953 tasks
Chunk capacity: 4096 → ~244 tasks
```

| Capacity | Task count  | Overhead | Load balance     |
|----------|-------------|----------|------------------|
| 128      | High        | High     | Excellent        |
| 256      | Medium-High | Moderate | Good             |
| 512      | Medium      | Low      | Good             |
| 1024     | Low         | Very low | Fair             |
| 4096     | Very low    | Minimal  | Poor for small N |

**General guideline:** start with 512. If you have many short-running callbacks and high thread counts, try 256. If each
callback does substantial work (e.g. physics), try 1024–4096.

### When to increase chunk size

- **Heavy callbacks:** physics, complex AI, pathfinding — larger chunks reduce scheduling overhead
- **Few total entities:** if you have only a few thousand entities, fewer chunks with more entities per chunk improves
  cache locality

### When to decrease chunk size

- **Very fast callbacks:** if each entity is processed in microseconds, more chunks allow better distribution across
  threads
- **High time variance:** if some entities take much longer than others (e.g. heavy AI vs. simple movement), smaller
  chunks enable better work stealing

### Cache locality

Smaller chunks (128-256) keep data "hotter" in cache but generate more scheduling overhead. Larger chunks (1024-4096)
have less overhead but may not fit entirely in the core's L2 cache.

```cpp
// Example: physics needs more work per entity
opts.WithArchetypeChunkCapacity(1024); // larger chunks for heavy callbacks

// Example: simple movement with many entities
opts.WithArchetypeChunkCapacity(256); // smaller chunks for light callbacks
```

### Work stealing

The thread pool uses work stealing: idle threads grab tasks from other threads. This means you don't need to balance
perfectly — threads that finish early will pick up work from busy threads.

### Measurement

Scheduling overhead is typically negligible compared to per-entity work. Use real profiling to verify:

```cpp
// Enable profiling to see time spent in scheduling vs. work
// Compile with -DFREYR_PROFILING=ON
```

---

## Example: overlapping parallel work

```cpp
void Update(float dt) override {
    // Schedule physics integration
    mScene->CreateQuery()->EachAsync<Position, Velocity>("Integrate", [dt](fr::Entity e, Position& pos, Velocity& vel) {
        pos.x += vel.dx * dt;
        pos.y += vel.dy * dt;
    });

    // Sync 
    mScene->ExecuteTasks();
    // Now Position is updated and consistent

    // Do sequential AI work while integration runs
    auto query = mScene->CreateQuery();
    query
        ->Excluding<StunnedTag>()
        ->Each<AIState>("AI::Think", [dt](fr::Entity e, AIState& ai) {
            ai.thinkTimer -= dt;
            if (ai.thinkTimer <= 0.f)
                ai.nextAction = computeNextAction(ai);
        });
}
```

---

## Avoiding dependencies

The biggest impact on parallel performance is avoiding dependencies between tasks. If task B needs the result of task A,
you lose parallelism.

**Data layout in Freyr:**

- Each chunk is processed independently
- Components are stored by archetype — all components of a type are contiguous in memory
- Chunk-based iteration ensures related data stays together

**Tips:**

1. **Don't modify archetype structure during iteration** — adding/removing components is deferred until end of Update
2. **Avoid reading data written by another task in the same frame** — use `ExecuteTasks()` to sync
3. **Prefer contiguous data** — accessing contiguous arrays is much faster than scattered random access
