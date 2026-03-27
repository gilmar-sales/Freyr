# ArchetypeBuilder

`ArchetypeBuilder` efficiently creates large numbers of entities with the same component layout. It pre-allocates all chunks at once and populates them in parallel, making it **significantly faster** than calling `Scene::CreateEntity` in a loop for bulk spawning.

Obtain a builder from the scene:

```cpp
auto builder = scene->CreateArchetypeBuilder();
```

---

## Basic usage

```cpp
scene->CreateArchetypeBuilder()
    .WithComponent(Position { .x = 0.f, .y = 0.f })
    .WithComponent(Velocity { .dx = 1.f })
    .WithEntities(100'000)
    .Build();
```

This creates 100 000 entities, each with a `Position` initialised to `{0, 0}` and a `Velocity` initialised to `{dx: 1}`.

---

## Methods

### `WithComponent<T>(value)`

Declares a component type and sets the **default value** applied to every entity created by this builder.

```cpp
builder.WithComponent(Health { .max = 100, .current = 100 });
```

**Template parameter:** `T` is inferred from the argument type — must satisfy `fr::IsComponent`.

Calling `WithComponent` with the same type more than once is ignored.

---

### `WithEntities(count)`

Sets the number of entities to create.

```cpp
builder.WithEntities(50'000);
```

Calling `Build()` with `count = 0` returns `nullptr` without creating any entities.

---

### `ForEach<Ts...>(fn)`

Registers a post-creation callback to customise individual entities. The callback is called with `(fr::Entity entity, Ts&... components)` for every entity after they are created.

```cpp
builder
    .WithComponent(Position {})
    .WithEntities(1000)
    .ForEach<Position>([](fr::Entity e, Position& pos) {
        pos.x = static_cast<float>(e) * 2.f; // spread entities along X
    });
```

Multiple `ForEach` calls are executed in registration order.

---

### `Build()`

Commits the builder, creates the entities, and returns a `Ref<Archetype>`.

```cpp
auto archetype = builder.Build(); // nullptr if WithEntities(0)
```

**Archetype reuse:** if an archetype with the same component signature already exists (from a previous `Build()` call or from `CreateEntity`), the new entities are **appended to the existing archetype** rather than creating a new one. The returned `Ref<Archetype>` points to the shared archetype.

```cpp
auto a1 = scene->CreateArchetypeBuilder()
    .WithComponent(Position { .x = 0.f })
    .WithEntities(100)
    .Build();

auto a2 = scene->CreateArchetypeBuilder()
    .WithComponent(Position { .x = 999.f })
    .WithEntities(50)
    .Build();

assert(a1 == a2);        // same archetype object
assert(a1->Count() == 150); // 150 total entities
```

---

## Full example

```cpp
// Spawn a grid of 10×10 enemies, each with unique positions
scene->CreateArchetypeBuilder()
    .WithComponent(Position {})
    .WithComponent(Health { .max = 50, .current = 50 })
    .WithComponent(EnemyTag {})
    .WithEntities(100)
    .ForEach<Position>([](fr::Entity e, Position& pos) {
        pos.x = static_cast<float>(e % 10) * 5.f;
        pos.y = static_cast<float>(e / 10) * 5.f;
    })
    .Build();
```

---

## When to prefer `CreateEntity`

Use `ArchetypeBuilder` when spawning **many entities at startup** with the same layout.

Use `Scene::CreateEntity` for **one-off** entity creation during gameplay:

```cpp
// Player fires a bullet — single entity, immediate
scene->CreateEntity(
    Position { mPlayer.x, mPlayer.y },
    Velocity { .dx = 0.f, .dy = 20.f },
    BulletTag {}
);
```
