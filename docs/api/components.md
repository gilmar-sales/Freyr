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

A component with no fields acts as a tag — useful to mark entities without adding data:

```cpp
struct PlayerTag : fr::Component {};
struct DeadTag   : fr::Component {};
```

```cpp
// Mark an entity as dead
scene->AddComponent<DeadTag>(entity);

// Query only living entities
scene->ForEach<Health>([&](fr::Entity e, Health& hp) {
    if (hp.current <= 0)
        scene->AddComponent<DeadTag>(e);
});
```

---

## Registration

Every component type must be registered with `FreyrExtension::WithComponent<T>()` **before** any entity uses it:

```cpp
freyr.WithComponent<Transform>()
     .WithComponent<Velocity>()
     .WithComponent<Health>()
     .WithComponent<PlayerTag>();
```

---

## Component ID

Each component type receives a unique, stable integer ID at first use:

```cpp
fr::ComponentId id = fr::GetComponentId<Transform>(); // e.g. 0
```

IDs are assigned in the order `GetComponentId<T>()` is first called. They are consistent within a single run but **not** across runs.

---

## Concept check

The `fr::IsComponent` concept validates that a type derives from `fr::Component`:

```cpp
template <typename T>
concept IsComponent = std::is_base_of_v<fr::Component, std::remove_reference_t<T>>;
```

Template functions in `Scene` and `ArchetypeBuilder` are constrained by this concept, giving clear compile-time errors for unregistered or incorrect types.

---

## Design tips

**Keep components small.** A component should represent a single, coherent aspect of an entity. Split large components into focused ones — `Position`, `Rotation`, and `Scale` rather than one `Transform` blob. This lets systems subscribe only to the data they actually need.

**Avoid pointers.** Components are copied during archetype migrations. Raw pointers inside components will dangle. Use entity IDs or indices to reference other entities.

**Prefer POD types.** Components with trivial copy/move constructors let the engine memcpy entire arrays during migration, which is much faster than element-wise construction.
