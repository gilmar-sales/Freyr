# Prefabs and clone

## Clone

```cpp
Entity clone = registry->Clone(source);
```

Copies all components of `source` into a new entity in the same archetype. Children are **not**
cloned in v1. Entity handles stored in components still point at the same targets.

## Prefab tag

`fr::Prefab` is registered by default. Queries and mutations **exclude** prefab entities unless
you call `IncludingPrefabs()`.

```cpp
auto prefab = registry->CreateEntity(Mesh {}, fr::Prefab {});
auto instance = registry->Instantiate(prefab); // Clone + remove Prefab + SetEnabled(true)
```

`Instantiate` always enables the instance. Snapshot serialization treats prefabs as normal entities
that carry the `Prefab` tag.
