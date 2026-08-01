# Performance Tuning

This guide provides concrete strategies for maximising throughput in Freyr-based applications.

---

## Measure first

Before optimising, **measure**. Freyr's Perfetto integration makes it easy to see where time is spent.

```bash
cmake -B build -DFREYR_PROFILING=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --parallel
./my_app
# Open the .pftrace file in https://ui.perfetto.dev
```

Look for:

- **Time per system** — which system takes the most wall time?
- **Thread utilisation** — are all workers busy? Or are some idle?
- **Chunk iteration time** — how long does each chunk take?
- **Scheduling overhead** — time between `EnqueueTask` and first worker pickup

---

## Chunk capacity tuning

`ArchetypeChunkCapacity` is the single most impactful configuration parameter. It controls task granularity.

| Capacity | Entities per task | Task count (1M entities) | Best for |
|----------|------------------|--------------------------|----------|
| 128      | 128              | ~7,813                   | Very fast callbacks, high thread count |
| 256      | 256              | ~3,906                   | Light callbacks, good load balance |
| 512      | 512              | ~1,953                   | **Recommended starting point** |
| 1024     | 1024             | ~977                     | Medium callbacks, physics |
| 4096     | 4096             | ~244                     | Heavy callbacks, pathfinding |

### Selecting chunk size by workload

**Light callbacks** (microseconds per entity, e.g. simple transforms):

```cpp
opts.WithArchetypeChunkCapacity(256);  // more tasks = better load balance
```

**Medium callbacks** (tens of microseconds, e.g. basic physics):

```cpp
opts.WithArchetypeChunkCapacity(512);  // balanced
```

**Heavy callbacks** (hundreds of microseconds, e.g. AI pathfinding):

```cpp
opts.WithArchetypeChunkCapacity(2048); // fewer tasks = less overhead
```

---

## Thread count

```cpp
opts.WithThreadCount(0);  // 0 = auto-detect (not yet implemented — defaults to 4)
opts.WithThreadCount(8);  // explicit
opts.WithAllPhysicalCores(); // use physical cores only (excludes SMT/HT)
```

Guidelines:

- **Don't oversubscribe** — using more threads than physical cores adds context-switching overhead
- **Use `WithAllPhysicalCores()`** on desktop/server — it queries OS topology to exclude HT/SMT logical cores
- **Reserve 1 core for the main thread** — if the main thread does non-trivial work besides `Registry::Update`

```cpp
// Example: 8 physical cores → 7 workers, 1 for main thread
opts.WithThreadCount(7);
```

---

## Component design

### Keep components small

Smaller components = more fit in cache = faster iteration.

```cpp
// POOR: Giant blob component — all systems pay for data they don't use
struct EntityData : fr::Component {
    float x, y, z;           // position
    float dx, dy, dz;        // velocity
    float maxHp, hp;         // health
    float armor;             // damage reduction
    int   teamId;            // faction
    char  name[64];          // display name
    // ... 20 more fields
};

// BETTER: Split into focused components
struct Position  : fr::Component { float x, y, z; };
struct Velocity  : fr::Component { float dx, dy, dz; };
struct Health    : fr::Component { float max, current; };
struct TeamTag   : fr::Component { int teamId; };
struct NameTag   : fr::Component { char name[64]; };
```

Now `MovementSystem` only reads `Position` + `Velocity` (24 bytes) instead of the full blob (100+ bytes).

### Use POD types

Trivially copyable types enable `memcpy`-based component migration:

```cpp
// FAST: Trivially copyable → memcpy during migration
struct Position : fr::Component {
    float x, y, z;
};

// SLOWER: Non-trivial copy → element-wise copy
struct StringComponent : fr::Component {
    std::string value; // copy involves allocation
};
```

### Use tag components

An empty struct costs **zero bytes** of component storage but is useful for filtering:

```cpp
struct DeadTag   : fr::Component {};
struct PlayerTag : fr::Component {};
struct EnemyTag  : fr::Component {};
```

```cpp
// Exclude dead entities from queries
query->Excluding<DeadTag>()->Each<Position>([](Entity e, Position& p) { ... });
```

---

## Iteration patterns

### Prefer `EachAsync` for independent work

If entities don't interact within a system, use `EachAsync` for free parallelism:

```cpp
// Parallel — no entity reads another's data
mRegistry->CreateMutation()->EachAsync<Position, Velocity>(fn);
```

### Use `Each` for sequential dependencies

When a system needs to read data written by another entity in the same iteration, use synchronous `Each`:

```cpp
// Sequential — safe for cross-entity reads
mRegistry->CreateMutation()->Each<AIState>([](Entity e, AIState& ai) {
    // reads data from other entities
});
```

### Batch parallel work

```cpp
void Update(float dt) override {
    // Start parallel work
    mRegistry->CreateMutation()->EachAsync<Position, Velocity>("Integrate", [dt](auto e, auto& p, auto& v) {
        p.x += v.dx * dt;
    });

    // Do sequential work while integration runs
    mRegistry->CreateMutation()->Each<AIState>("AI", [dt](auto e, auto& ai) {
        ai.think(dt);
    });

    // Wait for parallel work
    mRegistry->ExecuteTasks();

    // Now positions are consistent
}
```

---

## ArchetypeBuilder for bulk creation

`ArchetypeBuilder` is **much faster** than calling `CreateEntity` in a loop because it:

1. Pre-allocates all required chunks upfront
2. Registers components once for the entire batch
3. Creates entities in bulk with a single archetype lookup

```cpp
// FAST: ArchetypeBuilder — single allocation, single registration
registry->CreateArchetypeBuilder()
    .WithComponent(Position {})
    .WithComponent(Velocity {})
    .WithEntities(100'000)
    .Build();

// SLOW: Loop — 100k separate allocations and lookups
for (int i = 0; i < 100'000; i++) {
    registry->CreateEntity(Position {}, Velocity {});
}
```

---

## Event system overhead

Events have minimal overhead for the dispatch itself, but listeners add cost:

- Each `SendEvent` calls every active listener synchronously
- Listener callbacks run on the publisher's thread
- Use events for communication between **dissimilar** systems, not for high-frequency data passing

```cpp
// APPROPRIATE: Collision events between unrelated systems
registry->SendEvent(CollisionEvent {.entityA = a, .entityB = b});

// OVERKILL: Don't use events for position updates — just add components
registry->SendEvent(PositionUpdateEvent {.entity = e, .x = 10});  // ☹ avoid
```

---

## Avoid archetype thrashing

Adding and removing components every frame causes repeated archetype migration, which copies all component data:

```cpp
// BAD: Every frame moves entity between archetypes
void Update(float dt) override {
    mRegistry->AddComponent<FrozenTag>(entity);   // migrate → Archetype A + FrozenTag
    // ... do frozen logic ...
    mRegistry->RemoveComponent<FrozenTag>(entity); // migrate back
}

// BETTER: Check tag in system logic instead
struct FrozenTag : fr::Component { float timer; };

// In system: check timer instead of adding/removing
mRegistry->CreateMutation()->Each<FrozenTag>([dt](Entity e, FrozenTag& f) {
    f.timer -= dt;
    if (f.timer <= 0)
        mRegistry->RemoveComponent<FrozenTag>(e);
});
```

---

## Memory allocation

Freyr pre-allocates all entity index slots at startup (`MaxEntities`). Additional allocations happen when:

- New archetypes are created (component set never seen before)
- New chunks are created (existing chunks are full)
- Query results are collected (vector allocations)

For predictable performance, ensure:

1. `MaxEntities` covers worst-case entity count
2. All component combinations you'll use are registered upfront
3. Avoid queries that collect large result sets every frame (prefer `ForEach` iteration)

---

## Checklist

- [ ] Profile with Perfetto before optimising
- [ ] Tune chunk capacity to match callback weight
- [ ] Use `WithAllPhysicalCores()` for thread count
- [ ] Split large components into focused types
- [ ] Use tag components instead of runtime checks
- [ ] Prefer `EachAsync` for entity-independent work
- [ ] Use `ArchetypeBuilder` for bulk creation
- [ ] Avoid archetype migration every frame
- [ ] Keep components trivially copyable
- [ ] Avoid pointers inside components
