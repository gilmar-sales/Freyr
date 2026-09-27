# Freyr — Design Patterns, Data-Oriented Design & Practices

Patterns actually used in the codebase, with where they live. Follow them when extending Freyr.

## 1. Data-Oriented Design

### Storage layout (archetype → chunk → SoA columns)
- **Archetype** (`Containers/Archetype.hpp`) = unique component `Signature`. Owns a list of fixed-capacity **chunks** (`FreyrOptions::ArchetypeChunkCapacity`, default 512).
- **ArchetypeChunk** (`Containers/ArchetypeChunk.hpp`) = Structure-of-Arrays: one `ComponentArray<T>` (a `std::vector<T>` pre-sized to chunk capacity) per component type + a `LocalSparseSet<Entity>` whose dense array shares the same index space as every column. Index `i` is the same entity in all columns.
- **Change-detection ticks are a parallel column** (`std::vector<ComponentTicks>` next to `std::vector<T>` in `ComponentArray`), not a field inside the component — keeps hot component data compact.
- **Iteration hoists column base pointers** once per chunk (`std::make_tuple(&GetComponent<Ts>(0)...)`) and then walks `index` linearly (`meta::invoke_at_component_pointers`). New iteration paths must do the same, never per-entity `GetComponent` lookups.
- **Spans/views over columns**: `ChunkView` / `GetComponentSpan<T>()` / `GetEntitiesSpan()` expose raw contiguous data for batch/SIMD-friendly processing.

### Entity model
- `Entity` is a dense `uint32_t` index; `EntityHandle {entity, generation}` is the only safe long-lived reference (stale-handle detection via per-slot generation counters in `EntityManager`).
- `ComponentManager::mEntityIndexes` is a flat vector pre-allocated to `MaxEntities`: entity → `{Archetype*, ArchetypeChunk*}` in O(1), no hashing.
- IDs are recycled through a bounded lock-free MPMC free-list; `mAlive` / `mGenerations` are pre-allocated atomic arrays.

### Removal & migration
- **Swap-and-pop** everywhere (SparseSet dense array, `ComponentArray::Remove`): O(1), keeps arrays dense, does **not** preserve order — never rely on iteration order.
- Trivially copyable components take a `memcpy` fast path (`if constexpr (std::is_trivially_copyable_v<T>)`); only trivially copyable components get snapshot codecs (serialization silently skips others).
- Structural change (add/remove component) = archetype **migration**: copy all columns to the target archetype's chunk. Expensive → prefer stable tags or state fields over per-frame add/remove.

### Non-fragmenting relationships
- Hierarchy stores topology in a **side-table** (`HierarchyManager`: dense parent/depth arrays + children map) instead of vectors inside components, so parent links don't split archetypes and migration never copies child lists. Components only hold `ChildOf { EntityHandle }` / `ParentDepth`.

### Queries are cached, not recomputed
- `Signature` is a growable vector of 128-bit bitsets; matching is bitwise AND (`Match` = include ⊆ archetype, `Intersects` for exclusion).
- `ArchetypeMatchIndex` caches `Signature → vector<Archetype*>` and `Filter → vector<Archetype*>`, bootstrapped once and **updated incrementally** in `OnArchetypeAdded`. Bootstrap uses an inverted index (`mArchetypesByComponent`) starting from the rarest component.
- `Archetype::HasComponents<Ts...>` caches its probe signature in a `thread_local`.

### Memory/concurrency hygiene
- `alignas(64)` on hot atomics (`TaskCounter`, `RwLock`, chunk task counters, `HierarchyWorkQueue` counters) to avoid false sharing — do the same for new shared atomics.
- Pre-allocate/reserve (`MaxEntities` arrays, `reserve(1024)` on archetype containers, chunk-capacity-sized columns). Avoid per-frame allocation in hot paths.
- Batched mutation bindings use a 4 KiB stack buffer with heap fallback (`MutationAggregator.cpp`, `kBindingStackBytes`).
- Components are PODs: no owning raw pointers, keep them small (≤ 64 B ideal), split "god" components by access pattern, use empty tag structs for classification.

## 2. Concurrency Patterns

- **Chunk = unit of parallel work.** Each chunk owns a private `UnboundedMPMCQueue<Task>`; `StartTasks` uses a CAS on `mLocalTaskCounter` so **at most one worker drains a chunk at a time** → tasks on the same chunk are serialized, different chunks run in parallel. This is why structural ops (`RemoveEntity`, `MoveData`) are enqueued on the chunk rather than executed inline.
- **Work-stealing thread pool** (`ThreadPool`): one queue per worker; producers pick a queue with a thread-local LCG; idle workers spin over rotated sibling queues with `Processor::Pause()` (x86 `PAUSE` / ARM `YIELD`) before yielding, then park on a condition variable when the pool is `Idle`. Pool lifecycle is an explicit state machine (`Empty/Resizing/Spawning/Running/Idle`) driven by CAS.
- **Completion barrier**: a global `TaskCounter` using C++20 `atomic::wait/notify_all`. `WaitForAllTasks()` is the sync point at the end of every phase.
- **Deferred structural changes (command-buffer pattern)**: `AddComponent`/`RemoveComponent` push closures into a lock-free `mPendingMutations` queue; `DestroyEntity` inserts into a set. Both are applied between phases (`ExecutePendingMutations`, `DestroyEntities`). Observers (`ObserverManager`) and event-listener subscription (`Publisher::mPendingListeners`) are also queued and merged/flushed at frame boundaries.
- **Mutation batching / loop fusion** (`MutationAggregator::Flush`): all `EachAsync` calls issued in a phase are grouped by include-signature, matched per archetype, and executed per chunk in **one pass** — for each entity index, every matching mutation is applied (`RunBatchedMutations`), maximizing cache reuse. Type erasure uses function pointers (`bind`, `applyBound`) + placement-new bindings, not virtual calls.
- **Custom spin `RwLock`** (single atomic, writer bit + reader count) with RAII `ReadGuard`/`WriteGuard`; `SparseSet` is policy-parameterized on sync (`LockedSync` vs `UnlockedSync`/`LocalSparseSet`) so single-owner data pays no locking cost.
- Hierarchy propagation: `LevelSync` (depth buckets, barrier per level) or `WorkSharing` (Bevy-style: batches of 64 entities pushed to a shared MPMC queue, termination by `published == 0 && busy == 0`).

## 3. Object-Oriented / GoF-style Patterns

| Pattern | Where |
|---|---|
| **Facade** | `Registry` — single entry point over EntityManager, ComponentManager, SystemManager, EventManager, HierarchyManager, ResourceManager, ObserverManager, ThreadPool |
| **Dependency Injection / Service Locator** | Skirnir `ServiceCollection`/`ServiceProvider`; managers are singletons, `Archetype`/`Query`/`Mutation` are transients (`FreyrExtension::ConfigureServices`) |
| **Builder (fluent)** | `FreyrExtension`, `FreyrOptionsBuilder`, `PipelineBuilder`, `ArchetypeBuilder`, `Query`/`Mutation` chaining (`WithLabel`, `Excluding`, `Changed`…) |
| **Deferred configuration (Command list)** | `FreyrExtension` stores `std::vector<Action<T>>` lambdas and replays them in `UseServices` after the container is built |
| **Extension / Plugin** | `FreyrExtension : skr::IExtension`; `WithHierarchyPropagation<P>()` composes components + pipeline + system |
| **Policy-based design (static Strategy)** | `HierarchyPropagationPolicy` concept (`Local`, `World`, `OnRoot`, `Propagate`, `HasChildrenInterest`) with `Mat4TransformPolicy` / `Affine2DTransformPolicy`; `SparseSet<T, Sync>` |
| **Type erasure** | `IComponentArray` → `ComponentArray<T>` (`final`); `IPublisher` → `Publisher<TEvent>`; `PendingMutation` function pointers; `SnapshotCodec` function-pointer table; `ResourceManager` via `std::any` |
| **Observer / Pub-Sub** | `EventManager` (listener lifetime tied to `skr::Arc<ListenerHandle>`; `WeakArc` in the publisher, expired listeners pruned lazily); `ObserverManager` for component add/remove hooks |
| **Template Method (lifecycle hooks)** | `System::PreUpdate/Update/PostUpdate` |
| **Scheduler with DAG ordering** | `SystemManager::SortPipelineSystems` — Kahn topological sort from `After<T>()`/`Before<T>()`, `RunIf` predicates, per-pipeline fixed-rate accumulator (`WithRate`, disabled pipelines drop their accumulator) |
| **Factory** | `SystemManager` per-system factories/detachers; `Archetype::RegisterComponent` stores a `ComponentArrayFactory` to add columns to new chunks |
| **Prototype** | `Prefab` tag + `Registry::Instantiate` / `Clone` (column copy into same archetype) |
| **Null Object / sentinels** | `NullEntity`, `NullHandle`, empty `NoopGuard` in `UnlockedSync` |
| **RAII** | `RwLock` guards, `FreyrScopedTrace`, `ListenerHandle` Arc |

## 4. Modern C++ Idioms

- **Concepts as marker-type contracts**: `IsComponent`, `IsSystem`, `IsEvent`, `IsHierarchyLocal`, `HierarchyPropagationPolicy`; all public templates are `requires`-constrained. Components/events are empty-base marker structs (`struct Foo : fr::Component`), systems are the only polymorphic types.
- **C++26 static reflection** (`Meta/CallableComponents.hpp`): component types are deduced from the lambda's call-operator parameters (`Each([](Entity, Position&, Velocity&){})`). Rules: optional leading `Entity` typed explicitly (not `auto`), then concrete component references; violations are compile-time `std::meta::exception`s.
- **Process-stable type IDs** keyed by `refl::type_name<T>()` through a global registry (`TypeNameId.cpp`), cached in a function-local static — same ID across host/static libs/plugins sharing one Freyr copy. Separate ID spaces per `TypeIdKind` (Component/Event/System/Resource).
- Fold expressions over parameter packs for per-type work; `if constexpr` for compile-time branching (`Remove<T>` wrapper to express signature deltas).
- `[[nodiscard]]` on all queries/getters, `[[likely]]` on hot branches, explicit `std::memory_order` on every atomic.
- Zero-cost optional features via macros that compile to nothing: `FREYR_ASSERT` (only with `FREYR_ASSERTIONS`), `FREYR_TRACE*` (only with `FREYR_PROFILING`). Assertion messages use the `cond && "message"` idiom.
- `FREYR_NAMESPACE` macro (default `fr`) — use it, not a hard-coded `fr`, inside library headers.
- Cross-platform code isolated behind `Processor` (CPU pause, physical core count).

## 5. Practices & Rules of Thumb

**ECS usage**
- `EachAsync` for entity-independent work; `Each` when a callback reads other entities; never read another entity's components from an `EachAsync` callback.
- Structural changes and destroys are only visible at the end of the current phase; call `registry->ExecuteTasks()` for an explicit sync point outside `Update`.
- Don't call `Registry::Update` from callbacks, don't throw from callbacks.
- Store `EntityHandle` (not raw `Entity`) for references that outlive a frame; resolve with `Registry::Resolve`/`IsAlive`.
- Use `ArchetypeBuilder` for bulk spawning (one archetype lookup, pre-allocated chunks).
- Use tags (`Disabled`, `Prefab`, custom empty structs) instead of runtime flags; `Disabled`/`Prefab` are excluded by `Filter` by default.
- Events are for decoupled, low-frequency communication; listeners run synchronously on the sender's thread. Keep the `ListenerHandle` Arc alive as a member.
- Label mutations/queries (`WithLabel`) so they show up in Perfetto traces.

**Performance tuning** (`docs/guides/performance-tuning.md`)
- Profile first (`FREYR_PROFILING=ON`, RelWithDebInfo, open `.pftrace` in ui.perfetto.dev).
- Tune `ArchetypeChunkCapacity` to callback weight (smaller for light callbacks → more tasks; larger for heavy callbacks).
- `WithAllPhysicalCores()`; don't oversubscribe; leave a core for the main thread if it does real work.
- Size `MaxEntities` for worst case; avoid per-frame result-collecting queries (`EntitiesWith`, `Iterate`) in hot paths.

**Engineering**
- Every bug fix ships with a focused regression test named after the broken behavior, next to the module's specs.
- Tests: GoogleTest, Arrange-Act-Assert, fixtures build a real app via `skr::ApplicationBuilder().WithExtension<fr::FreyrExtension>(...).Build<EmptyApp>()` and tear down Registry before App.
- Benchmarks (Google Benchmark) under `benchmarks/`, compared against baselines with `_compare_baselines.py`.
- Formatting via `.clang-format`; static analysis via `.clang-tidy` (bugprone/cert checks).
