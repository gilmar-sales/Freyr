# Change detection

Freyr tracks per-component **added** and **changed** ticks in SoA arrays next to each component
column. The registry advances `CurrentTick` once per `Update`.

## Filters

```cpp
registry->CreateQuery()
    ->Changed<Health>()
    ->ForEachChunk<Health>(...);

registry->CreateQuery()
    ->Added<PlayerTag>()
    ->Count<PlayerTag>();

registry->CreateQuery()->CountRemoved<Health>();
registry->CreateQuery()->ForEachRemoved<Health>([](fr::EntityHandle h) { ... });
```

- `Changed<T>` / `Added<T>` filter entities whose ticks equal the current tick (dense scan of the
  tick column).
- `Removed<T>` reads a buffer filled on `RemoveComponent` / destroy; it becomes visible after the
  next `AdvanceTick` (start of `Update`).

Mutations via `Mutation::Each` / `EachAsync` bump `changedTick`. Structural adds bump both ticks
(re-adding a component the entity already has overwrites it in place and still counts as an add).
`ComponentManager::SetComponentNow` overwrites an existing component and bumps only `changedTick`
(adds it when missing); the hierarchy uses it so a reparent shows up as `Changed<ChildOf>`.

## Observers

```cpp
registry->ObserveAdd<Collider>([](fr::Entity e) { ... });
registry->ObserveRemove<Collider>([](fr::EntityHandle h) { ... });
```

Callbacks are queued at the structural change and drained in `ExecuteTasks` / end of `Update`
(safe with `EachAsync`).
