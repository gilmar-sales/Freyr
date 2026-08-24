# Components

Components are the **data layer** of the ECS. They hold entity state and contain no logic.

---

## Defining a component

Inherit from `fr::Component` and add your fields:

```cpp
#include <Freyr/Freyr.hpp>

struct Transform : fr::Component {
    float x        = 0.f;
    float y        = 0.f;
    float z        = 0.f;
    float scaleX   = 1.f;
    float scaleY   = 1.f;
    float scaleZ   = 1.f;
    float rotX     = 0.f;
    float rotY     = 0.f;
    float rotZ     = 0.f;
};
```

### Rules

- Inherit from `fr::Component` (empty base)
- **No virtual methods** — components are value types
- **No logic** — behaviour belongs in systems
- All fields should have sensible defaults so `T {}` produces a valid state
- Components are copyable (they are duplicated during archetype migration)

---

## Tag components

A component with no fields acts as a **tag** — useful to mark entities without adding data overhead:

```cpp
struct PlayerTag : fr::Component {};
struct DeadTag   : fr::Component {};
struct EnemyTag  : fr::Component {};
struct EditorOnly : fr::Component {};
```

Tags cost **zero bytes** in component storage because they have no fields, but they occupy a slot in the
archetype signature, which means:

- They enable **inclusion filtering**: `mutation->Each([](PlayerTag&, Health& hp) { ... })`
- They enable **exclusion filtering**: `query->Excluding<DeadTag>()->Count<Health>()`
- They prevent archetype collisions: entities with `PlayerTag` are in a different archetype from those without

```cpp
// Mark an entity as dead
registry->AddComponent<DeadTag>(entity);

// Query only living entities — exclude DeadTag
auto alive = registry->CreateQuery()
    ->Excluding<DeadTag>()
    ->Count<Health>();
```

---

## Registration

Every component type must be registered **before** any entity uses it. Prefer bootstrap registration:

```cpp
freyr.WithComponent<Transform>()
     .WithComponent<Velocity>()
     .WithComponent<Health>()
     .WithComponent<PlayerTag>();
```

For plugins / hot-reload you can also register late on the live `Registry`:

```cpp
registry->RegisterComponent<PluginComponent>();
ASSERT_TRUE(registry->IsComponentRegistered<PluginComponent>());

// …strip PluginComponent from all entities first…
ASSERT_TRUE(registry->UnregisterComponent<PluginComponent>());
```

| API | Behaviour |
| --- | --- |
| `RegisterComponent<T>()` | Idempotent — safe to call again after hot-reload |
| `IsComponentRegistered<T>()` | Whether `T` is in the manager’s registered set |
| `UnregisterComponent<T>()` | Returns `false` (no-op) if any entity still has `T`; empty archetypes that still list `T` do **not** block |

!!! warning "Unregistered components trigger assertions"
    If `FREYR_ASSERTIONS` is enabled, using an unregistered component triggers a runtime assertion.
    In release builds without assertions, behaviour is undefined.

### Plugin late registration

- There is **one** process-global type-name → id map backing `GetComponentId<T>()` (keyed by `refl::type_name<T>()`). Plugin `.so` files must **not** link another copy of Freyr (host should export symbols, e.g. `--export-dynamic`).
- Typing stays in C++ templates; Freyr does not expose name/layout-only or `void*` registration in this API.
- Safe detach order: strip `T` from entities → `UnregisterComponent<T>()` → `dlclose`.

---

## Component ID

Each component type receives a unique, dense integer ID at first use:

```cpp
fr::ComponentId id = fr::GetComponentId<Transform>(); // e.g. 0
```

IDs are keyed by the stable type name (`refl::type_name<T>()`), so the same type resolves to the same id across
static libs and plugins that share one Freyr copy in the process. Dense allocation (`0..N-1`) is preserved for
`SparseSet` and indexed arrays. IDs are consistent within a single run but **not** across runs. They are **never**
recycled when you unregister — unregister only removes the type from the manager’s registered set until you call
`RegisterComponent` again.

Inspect live storage from an editor via `Registry::ForEachArchetype` (`GetName`, `Count`, `ChunkCount`,
`ForEachComponent`). `TypeNameOf(TypeIdKind::Component, id)` resolves the same names from a raw id.

```cpp
struct Position : fr::Component {};
struct Velocity : fr::Component {};
struct Health   : fr::Component {};

// Same name → same id for the life of the process (order of first registration assigns the dense index)
fr::GetComponentId<Position>();
fr::GetComponentId<Velocity>();
fr::GetComponentId<Health>();
```

---

## Concept check

The `fr::IsComponent` concept validates that a type derives from `fr::Component`:

```cpp
template <typename T>
concept IsComponent = std::is_base_of_v<fr::Component, std::remove_reference_t<T>>;
```

Template functions in `Registry`, `Query`, `ArchetypeBuilder`, and `FreyrExtension` are constrained by this
concept, giving clear compile-time errors for incorrect types.

---

## Design guidelines

### 1. Keep components small

Split large component types into focused ones:

```cpp
// AVOID: Giant blob
struct Unit : fr::Component {
    float x, y, z;      // every system loads these
    float hp, maxHp;    // only HealthSystem needs these
    int   team;         // only AISystem needs this
    char  name[64];     // only UISystem needs this
};

// PREFER: Split by system responsibility
struct Position : fr::Component { float x, y, z; };
struct Health   : fr::Component { float current, max; };
struct TeamTag   : fr::Component { int teamId; };
struct NameTag   : fr::Component { char name[64]; };
```

Smaller components = less data loaded per system = better cache efficiency.

### 2. Avoid pointers

Components are copied during archetype migrations. Raw pointers inside components will dangle.
Use entity IDs or indices to reference other entities:

```cpp
// WRONG: Pointer becomes invalid after migration
struct Targeting : fr::Component {
    fr::Entity* target; // DANGER: pointer may dangle
};

// RIGHT: Entity ID is stable
struct Targeting : fr::Component {
    fr::Entity targetId = fr::Entity(-1); // -1 = no target
};
```

### 3. Prefer POD types

Components with trivial copy/move constructors let the engine `memcpy` entire arrays during migration:

```cpp
struct Position : fr::Component {
    float x, y, z;   // trivially copyable → fast migration
};
```

Non-trivial types like `std::string` incur element-wise copy overhead:

```cpp
struct StringComponent : fr::Component {
    std::string value; // element-wise copy during migration
};
```

### 4. Use sensible defaults

```cpp
struct Health : fr::Component {
    float current = 100.f;
    float max     = 100.f;
    float regen   = 0.f;
};
```

This ensures `Health {}` always produces a valid, alive entity.

### 5. Group data by access pattern

Components that are always accessed together should be separate but considered as a logical pair:

```cpp
// These are always read together by MovementSystem
struct Position : fr::Component { float x, y; };
struct Velocity : fr::Component { float dx, dy; };

// But they COULD be separate if another system only needs Position
// (e.g. RenderSystem only reads Position)
```
