# Worlds

A `World` owns an isolated `Registry` (and its service scope). There is no additive merge between
worlds in v1 — transfer data with Snapshot save/load or manual clone. Each world configures its
own components via its `FreyrExtension` lambda; component `TypeId`s are process-global (keyed by
type name), but registration, archetypes, entity IDs, and resources are all per-world.

```cpp
auto combat = fr::World::Create([](fr::FreyrExtension& freyr) {
    freyr.WithComponent<Health>();
});

auto& registry = combat.Get(); // fr::Registry&; GetRegistry() returns skr::Arc<Registry>
fr::SnapshotReader{}.Load(registry, stream);
```

Unload by letting the `World` go out of scope. Entity IDs are independent per world.
