# Architecture

## High-level overview

```mermaid
graph TD
    FB["skr::ApplicationBuilder"] -->|AddExtension| FE["FreyrExtension"]
    FE -->|registers| CM["ComponentManager"]
    FE -->|registers| SM["SystemManager"]
    FE -->|configures| OPT["FreyrOptions"]
    FB -->|Build| APP["IApplication"]
    APP -->|GetService| SC["Scene"]

    SC --> CM
    SC --> EM["EntityManager"]
    SC --> EVM["EventManager"]
    SC --> SM
    SC --> TM["ThreadPool"]

    CM --> ARCH["Archetype[]"]
    ARCH --> CHUNK["ArchetypeChunk[]"]
    TM --> WORKERS["Worker Threads"]
    WORKERS -->|process| CHUNK
```

---

## Core components

### Scene

The central orchestrator. All entity and component operations flow through `Scene`, which delegates to the appropriate manager.

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

### ComponentManager

Manages archetype discovery and entity placement. When an entity's component set changes, `ComponentManager` moves it to the correct archetype, migrating its data in place.

### EntityManager

Allocates and recycles entity IDs. Uses a free-list internally so destroyed entity slots are reused immediately.

### SystemManager

Holds all registered system instances. Calls lifecycle hooks in registration order.

### EventManager

Thread-safe publish/subscribe bus. Each event type gets its own `Publisher<T>`, backed by a shared-read/exclusive-write mutex and a pending-listener queue for safe concurrent subscription.

### ThreadPool

A fixed-size thread pool with one **MPMC (multi-producer, multi-consumer) lock-free queue** per worker. Tasks (chunk iterations) are distributed across workers through the thread pool's work-stealing mechanism.

---

## Memory layout

```
Archetype [Position, Velocity]
│
├── ArchetypeChunk (capacity = 512 entities)
│   ├── ComponentArray<Position>   [p0, p1, p2, ..., p511]  ← contiguous
│   └── ComponentArray<Velocity>   [v0, v1, v2, ..., v511]  ← contiguous
│
└── ArchetypeChunk (capacity = 512 entities)
    ├── ComponentArray<Position>   [p512, ..., p1023]
    └── ComponentArray<Velocity>   [v512, ..., v1023]
```

Each `ComponentArray<T>` is a raw contiguous buffer of `T` values. A chunk owns one array per registered component type. When a system iterates via `Query::Each<Position, Velocity>`, it strides through both arrays simultaneously — all data remains in cache.

---

## Execution flow for `Query::EachAsync`

```mermaid
sequenceDiagram
    participant S as System
    participant Q as Query
    participant CM as ComponentManager
    participant TM as ThreadPool
    participant W1 as Worker 1
    participant W2 as Worker 2

    S->>Q: EachAsync<Position, Velocity>(fn)
    Q->>CM: find matching archetypes
    CM-->>Q: [Archetype A, Archetype B]
    Q->>TM: enqueue chunk tasks
    TM->>W1: Chunk 0 task
    TM->>W2: Chunk 1 task
    W1-->>W1: process entities 0–511
    W2-->>W2: process entities 512–1023
    TM-->>Q: all tasks done (on WaitForAllTasks)
```

---


