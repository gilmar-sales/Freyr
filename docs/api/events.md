# Events

Freyr's event system provides a **thread-safe publish/subscribe bus** for decoupled communication between systems. Events carry data from publishers to all active subscribers without direct references between systems.

---

## Defining an event

Inherit from `fr::Event` and add your payload fields:

```cpp
#include <Freyr/Freyr.hpp>

struct CollisionEvent : fr::Event {
    fr::Entity entityA;
    fr::Entity entityB;
    float      impactForce = 0.f;
};

struct EntityDestroyedEvent : fr::Event {
    fr::Entity entity;
    std::string reason;
};
```

---

## Publishing an event

Call `Scene::SendEvent` from anywhere that has access to the scene:

```cpp
// From within a system's Update
void Update(float dt) override {
    mScene->ForEach<Position, Collider>(
        [this](fr::Entity a, Position& posA, Collider& colA) {
            // ... detect overlap ...
            mScene->SendEvent(CollisionEvent {
                .entityA     = a,
                .entityB     = b,
                .impactForce = 42.f
            });
        });
}
```

Events are dispatched **immediately** — all subscribers are called synchronously before `SendEvent` returns.

---

## Subscribing to an event

### Via `Scene::AddEventListener` (recommended)

```cpp
class ResponseSystem : public fr::System {
public:
    explicit ResponseSystem(const Ref<fr::Scene>& scene) : System(scene) {
        // Subscribe and store the handle
        mCollisionHandle = scene->AddEventListener<CollisionEvent>(
            [this](const CollisionEvent& ev) {
                onCollision(ev);
            });
    }

private:
    void onCollision(const CollisionEvent& ev) {
        // apply damage, play sound, etc.
    }

    Ref<fr::ListenerHandle> mCollisionHandle;
};
```

### Via `EventManager` (direct injection)

```cpp
class ResponseSystem : public fr::System {
public:
    ResponseSystem(const Ref<fr::Scene>& scene, Ref<fr::EventManager> events)
        : System(scene)
    {
        mHandle = events->Subscribe<CollisionEvent>(
            [](const CollisionEvent& ev) { /* ... */ });
    }

private:
    Ref<fr::ListenerHandle> mHandle;
};
```

---

## Subscription lifetime

The subscription is **active as long as the `ListenerHandle` shared_ptr is alive**. When the `ListenerHandle` is destroyed (or reset), the subscription is automatically removed before the next `Publish` call.

```cpp
// Active — mHandle keeps the subscription alive
Ref<fr::ListenerHandle> mHandle = scene->AddEventListener<MyEvent>(...);

// Unsubscribe explicitly
mHandle.reset(); // subscription is now dead
```

!!! danger "Don't discard the handle"
    If you do not store the returned `Ref<ListenerHandle>`, it is destroyed immediately and the callback is **never** called.

    ```cpp
    // WRONG — handle destroyed, callback never fires
    scene->AddEventListener<MyEvent>([](const MyEvent&) { ... });

    // CORRECT — store the handle
    mHandle = scene->AddEventListener<MyEvent>([](const MyEvent&) { ... });
    ```

---

## Thread safety

`EventManager` is fully thread-safe:

- Multiple threads can `Subscribe` concurrently
- Multiple threads can `Send` concurrently
- Subscriptions registered while a `Publish` is in progress are queued and merged before the next call — they will never receive the current in-flight event

---

## Event ID

Each event type has a unique runtime ID:

```cpp
fr::EventId id = fr::GetEventId<CollisionEvent>();
```

---

## Multiple events example

```cpp
struct DamageEvent : fr::Event {
    fr::Entity target;
    int        amount;
};

struct HealEvent : fr::Event {
    fr::Entity target;
    int        amount;
};

class HealthSystem : public fr::System {
public:
    HealthSystem(const Ref<fr::Scene>& scene) : System(scene) {
        mDamageHandle = scene->AddEventListener<DamageEvent>(
            [this](const DamageEvent& ev) {
                mScene->TryGetComponents<Health>(ev.target,
                    [&](Health& hp) { hp.current -= ev.amount; });
            });

        mHealHandle = scene->AddEventListener<HealEvent>(
            [this](const HealEvent& ev) {
                mScene->TryGetComponents<Health>(ev.target,
                    [&](Health& hp) {
                        hp.current = std::min(hp.current + ev.amount, hp.max);
                    });
            });
    }

private:
    Ref<fr::ListenerHandle> mDamageHandle;
    Ref<fr::ListenerHandle> mHealHandle;
};
```
