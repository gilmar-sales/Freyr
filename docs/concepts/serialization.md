# Serialization

Freyr snapshots the world as a **binary SoA stream**: alive entities, then archetypes keyed by
`refl::type_name` (FQN + namespace), then per-chunk entity ids and contiguous component columns.

Dense `ComponentId` values are **not** persisted — only type names. On load, components must already
be registered (`WithComponent` / `RegisterComponent`); layout is validated with `sizeof` / `alignof`.

## Constraints (v1)

- Components in a snapshot must be `std::is_trivially_copyable`
- Non-POD types are rejected at save time (no codec registered)
- Cross-entity refs should use `EntityHandle`; specialize `EntityRemapper<T>` to remap on load
  (`ChildOf` is built-in)

## API

```cpp
#include <Freyr/Freyr.hpp>
#include <sstream>

std::stringstream buffer;
fr::SnapshotWriter{}.Save(*registry, buffer);

// ... later, on a registry with the same components registered ...
buffer.seekg(0);
fr::SnapshotReader{}.Load(*registry, buffer);
```

Save flushes hierarchy + pending tasks first. Load clears live entities, rebuilds components, remaps
`EntityHandle` fields in parallel per chunk, then rebuilds hierarchy side-tables from `ChildOf`.

## Parallelism

Chunk column encoding (save) and handle remapping (load) use the registry `ThreadPool`
(one task per non-empty chunk).
