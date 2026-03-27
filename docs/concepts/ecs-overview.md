# ECS Overview

## What is ECS?

**Entity-Component-System (ECS)** is a software architectural pattern popular in real-time simulations and games. It follows the principle of **composition over inheritance**: objects in the world are not defined by class hierarchies, but by the data attached to them.

| Concept | Role |
|---------|------|
| **Entity** | A lightweight identifier (integer ID) representing a world object |
| **Component** | A plain data structure holding one aspect of an entity's state |
| **System** | Logic that processes all entities matching a specific set of components |

---

## Why ECS?

### Traditional OOP problems

In a typical game, a `Character` class might inherit from `PhysicsObject`, which inherits from `GameObject`. Adding a new behaviour (e.g. swimming) requires modifying the hierarchy or adding awkward mixins. Data and behaviour are tightly coupled, and related data is scattered across unrelated memory addresses.

### ECS advantages

**Data-Oriented Design**
: Components are stored contiguously in memory. When a system processes 1 000 000 positions, it reads a packed array — no pointer chasing, maximum cache efficiency.

**Parallel-friendly**
: Systems that operate on different component sets have no shared state by default. They can run in parallel with no synchronisation. Within a single system, individual entities have no dependencies, so chunk-level parallelism is safe.

**Dynamic behaviour**
: Adding or removing a component at runtime changes which systems process that entity — without recompilation or virtual dispatch.

---

## Pros and cons

### Pros

- Contiguous data layout → fewer cache misses → higher throughput
- Natural fit for multi-core processing
- Entities change behaviour by gaining/losing components, not by subclassing
- Scales to millions of entities

### Cons

- Higher initial complexity than OOP
- Overkill for small numbers of unique, one-off objects
- Requires careful component design to avoid data duplication

---

## ECS in Freyr

Freyr extends the basic ECS model with **archetypes** — groups of entities that share the same set of component types. All entities in an archetype are stored together in fixed-size **chunks**.

```
Archetype [Position, Velocity]
├── Chunk 0   → entities 0–511
├── Chunk 1   → entities 512–1023
└── Chunk 2   → entities 1024–1535

Archetype [Position, Health]
└── Chunk 0   → entities 1536–2047
```

When you call `ForEachParallel<Position, Velocity>`, Freyr:

1. Finds all archetypes whose signature contains `Position` and `Velocity`
2. Enqueues each matching chunk as an independent task
3. Worker threads pull and process chunks concurrently

This design means:
- **Cache locality** — all positions in a chunk are contiguous
- **Zero false sharing** — each chunk belongs to exactly one task at a time
- **Scalable** — more threads = more chunks processed in parallel

---

## Entity identity

An entity is an integer (`fr::Entity`, `uint64_t`). It has no data of its own — its identity is the union of its components.

!!! warning "ID stability"
    An entity's internal ID may change when components are added or removed, as the entity migrates to a different archetype. Do not persist raw entity IDs across frames if the entity's component set changes.
