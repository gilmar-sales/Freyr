# Architecture

## High-level overview

```mermaid
graph TB
    subgraph Build["Build Phase"]
        FB["skr::ApplicationBuilder"]
        FE["FreyrExtension"]
        OPT["FreyrOptions<br/>MaxEntities, ThreadCount,<br/>ChunkCapacity"]
    end

    subgraph Bootstrap["Bootstrap"]
        SC["Scene<br/>Central orchestrator"]
        CM["ComponentManager<br/>Archetype routing"]
        EM["EntityManager<br/>ID allocation &amp; recycling"]
        SM["SystemManager<br/>Pipeline scheduling"]
        EVM["EventManager<br/>Pub/sub bus"]
        TP["ThreadPool<br/>Work stealing pool"]
    end

    subgraph Storage["Data Layer"]
        ARCH["Archetype[]<br/>Grouped by component signature"]
        CHUNK["ArchetypeChunk[]<br/>Fixed-size storage units"]
    end

    subgraph Execution["Execution"]
        WORKERS["Worker Threads"]
        QUERY["Query / QueryAggregator<br/>Filter &amp; dispatch"]
    end

    FB -->|AddExtension| FE
    FE -->|registers components| CM
    FE -->|registers systems| SM
    FE -->|configures| OPT

    FB -->|Build| SC
    SC --> CM
    SC --> EM
    SC --> SM
    SC --> EVM
    SC --> TP

    CM --> ARCH
    ARCH --> CHUNK

    SM --> QUERY
    QUERY --> CM
    QUERY -->|enqueue chunk tasks| TP
    TP -->|distribute| WORKERS
    WORKERS -->|process| CHUNK

    style SC fill:#1565C0,color:#fff
    style CM fill:#2E7D32,color:#fff
    style TP fill:#E65100,color:#fff
    style QUERY fill:#6A1B9A,color:#fff
```

---

## Scene — the central orchestrator

`Scene` owns all managers and drives the update loop. All entity and component operations flow through `Scene`,
which delegates to the appropriate manager.

```mermaid
graph TB
    subgraph SceneInternals["Internal Structure of Scene"]
        SC["Scene"]
        
        subgraph Managers["Managers"]
            direction TB
            CM["ComponentManager<br/>- Archetype list<br/>- Entity→Archetype map<br/>- Component registration"]
            EM["EntityManager<br/>- Entity generation<br/>- MPMC free list"]
            SM["SystemManager<br/>- Pipeline list<br/>- System factory map"]
            EVM["EventManager<br/>- Publisher map<br/>- Pending listener queues"]
        end

        subgraph Exec["Execution"]
            TP["ThreadPool<br/>- Worker threads<br/>- Per-worker MPMC queues"]
            QA["QueryAggregator<br/>- Pending query batch"]
        end

        subgraph Data["Deferred Data"]
            DT["mEntitiesToDestroy<br/>(SparseSet&lt;Entity&gt;)"]
        end

        SC --> CM
        SC --> EM
        SC --> SM
        SC --> EVM
        SC --> TP
        SC --> QA
        SC --> DT
    end

    style SC fill:#1565C0,color:#fff
    style CM fill:#2E7D32,color:#fff
    style EM fill:#2E7D32,color:#fff
    style SM fill:#2E7D32,color:#fff
    style EVM fill:#2E7D32,color:#fff
    style TP fill:#E65100,color:#fff
    style QA fill:#6A1B9A,color:#fff
```

### Update loop in detail

```
Scene::Update(dt)
│
├─ 1. EventManager::Flush()
│      Merge pending subscribers into active lists
│      Clear expired listener handles
│
├─ 2. ThreadPool::StartWorkers()
│      Signal workers to begin pulling tasks
│
├─ 3. SystemManager::Accumulate(dt)
│      For each Pipeline:
│        accumulator += dt
│        if accumulator >= rate_interval → mark pipeline ready
│
├─ 4. SystemManager::PreUpdate(dt)
│      For each ready pipeline:
│        For each system in pipeline:
│          system->PreUpdate(dt)
│      ThreadPool::WaitForAllTasks()
│      Scene::DestroyEntities()
│
├─ 5. SystemManager::Update(dt)          ← Main work happens here
│      For each ready pipeline:
│        For each system in pipeline:
│          system->Update(dt)  ← systems call Query::Each/EachAsync
│      ThreadPool::WaitForAllTasks()
│      Scene::DestroyEntities()
│
└─ 6. SystemManager::PostUpdate(dt)
       For each ready pipeline:
         For each system in pipeline:
           system->PostUpdate(dt)
       ThreadPool::WaitForAllTasks()
       Scene::DestroyEntities()
```

---

## ComponentManager — archetype routing

The `ComponentManager` maintains:

- A **flat list of `Archetype`** shared pointers
- A **`std::vector<EntityIndex>`** mapping each entity ID to its current `(Archetype*, ArchetypeChunk*)`

When a component is added or removed, `ComponentManager`:

1. Computes the new component signature
2. Searches existing archetypes for a matching signature
3. If found → migrate entity to existing archetype
4. If not found → create new archetype, extend the archetype list
5. Returns the new `(archetype, chunk)` pair

### Archetype migration

```mermaid
graph LR
    subgraph Before["Before: AddComponent&lt;Health&gt; to Entity 5"]
        A1["Archetype [Position, Velocity]"]
        A1 --> C1["Chunk: Ent 0, Ent 1, Ent 5, Ent 2"]
    end

    subgraph After["After"]
        A1_b["Archetype [Position, Velocity]"]
        A2["Archetype [Position, Velocity, Health]"]
        A1_b --> C1_b["Chunk: Ent 0, Ent 1, Ent 2"]
        A2 --> C2["Chunk: Ent 5, ..."]
    end

    Before -->|"ComponentManager<br/>moves entity 5"| After
```

!!! note "Data is copied, not moved"
    During migration, component data is **copied** from the source chunk to the target chunk using
    `ComponentArray<T>::CopyComponent`. Components should be cheap to copy (prefer POD types).

---

## EntityManager — ID allocation

The `EntityManager` uses:

- A **`rigtorp::MPMCQueue<Entity>`** (lock-free multi-producer/multi-consumer queue) for recycled IDs
- An **`std::atomic<Entity>`** counter for new entity generation

When `CreateEntity()` is called:

1. Try to pop from the free list (MPMC queue) → fast path for recycled IDs
2. If empty, atomically increment the living count → new sequential ID

When `DestroyEntity()` is called:

1. Push the ID back onto the free list for immediate reuse

```cpp
Entity CreateEntity() {
    if (Entity entity; mAvailableEntities.try_pop(entity))
        return entity;                // recycled ID
    return mLivingEntityCount++;      // new ID
}

void DestroyEntity(Entity entity) {
    mAvailableEntities.try_push(entity);  // return to pool
}
```

---

## SystemManager — pipeline scheduling

The `SystemManager` holds:

- A **`std::vector<Pipeline>`** — each pipeline has a name, rate, accumulator, and list of system IDs
- A **`SparseSet<SystemId>`** of registered systems
- A **`std::vector<skr::ServiceFactory>`** — factory functions for lazy system construction

### Pipeline timing

```cpp
struct Pipeline {
    std::string_view Name;
    float            Rate;           // update interval in seconds
    float            Accumulator;    // elapsed time since last execution
    std::vector<SystemId> Systems;
};
```

Pipelines track their own elapsed time. A pipeline with `WithRate(60.0f)` has `Rate = 1/60 ≈ 0.0167s`.
The accumulator is incremented each frame by `dt`. When `accumulator >= Rate`, the pipeline executes.

```mermaid
timeline
    title Pipeline Execution Over Frames
    Frame 1 : dt = 16ms : accumulator = 0 → 16 : Physics pipe executes
    Frame 2 : dt = 8ms  : accumulator = 0 → 8  : doesn't execute
    Frame 3 : dt = 12ms : accumulator = 8 → 20 : Physics pipe executes again
```

---

## EventManager — pub/sub bus

The `EventManager` is fully thread-safe:

- **`Publisher<T>`** instances per event type, indexed by `EventId`
- **Pending listener queue** — subscribers added during `Publish()` are queued and merged before the next flush
- **Expired handle cleanup** — listeners with destroyed handles are removed during `Flush()`

```mermaid
graph TB
    subgraph EventSystem["Event Manager Internals"]
        EVM["EventManager"]
        EVM --> P1["Publisher&lt;CollisionEvent&gt;"]
        EVM --> P2["Publisher&lt;DamageEvent&gt;"]
        EVM --> P3["Publisher&lt;HealEvent&gt;"]

        subgraph Pub1["Publisher&lt;T&gt;"]
            direction LR
            ACTIVE["Active Listeners<br/>(vector)"]
            PENDING["Pending Listeners<br/>(vector)"]
            LOCK["RwLock"]
        end

        P1 --> Pub1
    end

    S1["System A<br/>subscribes"] -->|Subscribe| PENDING
    S2["System B<br/>publishes"] -->|Publish| ACTIVE
    ACTIVE -->|Flush| CLEANUP["Clear expired<br/>Merge pending"]
```

---

## ThreadPool — work stealing

The `ThreadPool` uses:

- **One `rigtorp::MPMCQueue<Task>` per worker** — MPMC queues allow any thread to push, any thread to pop
- **Work stealing via LCG hashing** — `AddTask` distributes tasks across queues using a linear congruential generator
- **`TaskCounter`** — atomic counter tracking pending tasks for synchronisation

```cpp
void AddTask(auto&& func) {
    mTaskCounter->AddTasks(1);
    mQueueLcgState = mQueueLcgState * LCG_MULTIPLIER + LCG_INCREMENT;
    const auto nextQueue = mQueueLcgState % mWorkerQueues.size();
    mWorkerQueues[nextQueue]->push(std::forward<decltype(func)>(func));
}
```

When a worker finishes its queue, it tries to pop from other workers' queues — this is work stealing.

---

## Key design decisions

| Decision | Rationale |
|----------|-----------|
| Archetype-based storage | Maximises cache locality — entities with same components are stored together |
| Fixed-size chunks | Enables uniform task granularity for parallel dispatch |
| Per-worker MPMC queues | Minimises contention — producers hash to different queues |
| Deferred entity destruction | Prevents iterator invalidation during iteration |
| RwLock on archetypes | Allows concurrent reads (multiple queries) with exclusive writes (migration) |
| Skirnir DI integration | Systems can inject any dependency via constructor |
