# Parallel Processing

Freyr provides three iteration modes. Choosing the right one for each situation is the most impactful performance decision you can make.

---

## Iteration modes

| Method | Blocking | Thread pool | Use when |
|--------|----------|-------------|----------|
| `ForEach` | Yes | No | Ordered logic, cross-entity writes, debugging |
| `ForEachAsync` | No | Yes | Fire-and-forget; sync later with `ExecuteTasks()` |

---

## `ForEach` — sequential

```cpp
mScene->ForEach<Position, Velocity>([dt](fr::Entity, Position& pos, Velocity& vel) {
    pos.x += vel.dx * dt;
});
```

Processes entities one at a time, in chunk order. Safe for any operation, including reading/writing other entities.

---

## `ForEachAsync` — fire-and-forget

```cpp
// Dispatch tasks without waiting
mScene->ForEachAsync<Velocity>([](fr::Entity, Velocity& vel) {
    vel.dx *= 0.99f;
});

// Do other sequential work here while tasks run...
mScene->ForEach<Health>([](fr::Entity, Health& hp) { ... });

// Wait for async tasks before reading Velocity results
mScene->ExecuteTasks();
```

Use `ForEachAsync` to overlap CPU work: start a long parallel computation, do unrelated sequential work, then sync.

---

## Execution strategies

The strategy controls how chunks are assigned to worker threads. Configure it in `FreyrOptionsBuilder`:

```cpp
opts.WithExecutionStrategy(fr::FreyrExecutionStategy::ChunkAffinity);
```

### `ChunkAffinity` (default)

Each chunk is "pinned" to the last worker thread that processed it. On subsequent frames, that same thread handles the chunk again.

**Effect:** the chunk's component arrays stay warm in the thread's private L1/L2 caches across frames.

**Best for:** simulations where the same systems run every frame on the same entities (the common case for games).

### `DispatchOrder`

Chunks are dispatched to workers in creation order, round-robin style.

**Effect:** simpler scheduling, more even distribution if entity populations change frequently.

**Best for:** workloads with high entity churn or one-shot batch operations.

---

## Tuning chunk capacity

`ArchetypeChunkCapacity` controls the number of entities per chunk, which directly determines task granularity:

```
Total entities: 1 000 000
Chunk capacity: 512  → ~1 953 tasks
Chunk capacity: 4096 → ~244 tasks
```

| Capacity | Task count | Overhead | Load balance |
|----------|-----------|----------|--------------|
| 128 | High | High | Excellent |
| 512 | Medium | Low | Good |
| 1024 | Low | Very low | Fair |
| 4096 | Very low | Minimal | Poor for small N |

**Guideline:** start with 512. If you have many short-running callbacks and high thread counts, try 256. If each callback does substantial work (e.g. physics), try 1024–4096.

---

## Labelled overloads

All iteration methods accept an optional label string used in Perfetto traces:

```cpp
mScene->ForEachAsync<Position, Velocity>("Physics::Integrate", fn);
```

This makes it easy to identify hotspots in profiling output. See the [Profiling guide](profiling.md).

---

## Example: overlapping parallel work

```cpp
void Update(float dt) override {
    // Start async physics integration
    mScene->ForEachAsync<Position, Velocity>("Integrate", [dt](fr::Entity, Position& pos, Velocity& vel) {
        pos.x += vel.dx * dt;
        pos.y += vel.dy * dt;
    });

    // Do sequential AI work while integration runs
    mScene->ForEach<AIState>("AI::Think", [dt](fr::Entity, AIState& ai) {
        ai.thinkTimer -= dt;
        if (ai.thinkTimer <= 0.f)
            ai.nextAction = computeNextAction(ai);
    });

    // Sync before anything reads Position
    mScene->ExecuteTasks();
}
```
