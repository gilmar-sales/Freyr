# Resources

World-scoped singletons live in `ResourceManager` on the `Registry` — not in archetypes.

```cpp
freyr.WithResource(Time {.delta = 0.016f});

registry->InsertResource(Score {.value = 0});
registry->GetResource<Score>().value += 10;
registry->HasResource<Score>();
registry->TryGetResource<Score>(); // std::optional<std::reference_wrapper<Score>>
registry->RemoveResource<Score>();
```

Resources are typed by `TypeIdKind::Resource` and do not need to be `Component`s. Each `World`
has its own `ResourceManager` (part of `Registry`, not the DI container), so resources inserted
in one world are invisible in another.

!!! warning "Threading"
    Do not mutate resources from `EachAsync` workers without external synchronization.
    Treat them as main-thread / phase-flush state (similar to Bevy non-parallel resources).
