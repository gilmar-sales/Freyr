# Freyr

A multithreaded ECS library with focus on parallelism based on task queues by archetype chunks.

## Concept

**Entity-Component-System (ECS)** is a **software architectural** pattern mostly used in real-time simulation software
like video games, acting in the storage of game world objects. An **ECS** follows the **pattern** of "entities" with "
components" of data.
An **ECS** follows the principle of **composition** over **inheritance**, meaning that every entity is defined not by a
**class**, but by the **components** that are associated with
it [[wiki](https://en.wikipedia.org/wiki/Entity_component_system)].
Components are structures that only store the entity data that serves to systems.
Systems processes the components, giving behaviour for the entities, and adding or removing components can change the
systems that will work on an entity making the entity behaviour change in the run-time.

### Pros

- Data Oriented
    * Data tends to be stored linearly, optimizing the access to it
    * With contiguous data allocation the processor have less idle-times and works faster
- Friendly to parallel-processing
    * Systems that don't depend on other systems can be processed in parallel
    * By the way of the system is structured, many entities and components don't have conflicts and can be processed in
      parallel by two or more systems
- Dynamic Behaviour
    * An entity can change its behaviour in run-time without recompiling
    * Just add or remove a component of an entity to change its behaviour

### Cons

- Overkill designing unique entities
    * If in your game have some unique creatures, some systems will be dedicated just for only one entity
- More complex to implement and understand
    * Make your own ECS will require so much time for understand completely the architecture, and much more for the
      implementation and testing the trade-offs
- Aren't widely spread, most people don't know the pattern
    * Actually almost or none of academic institutes teaches this pattern
    * Mostly people interested in this pattern are self-taught and rarely spread their insights

### Relevant things to know

- Entities are sets of components, in the most ecs design the entity is represented by an ID that make the relation with
  the components, but never forget, the entity is just a set of components.
- Components only store the entity data, the entity behaviour is work for the systems
- The design of how components relate to entities depend upon the Entity Component System being used.

## Design

### Entities

In the **ct-ecs** each entity is represented by a set of components, the components are related to the entity by a
natural number as an ID.
The ID doesn't represent the entity all the time, just in a system iteration, the concrete entity is made only of
components, an entity could change its ID during the run-time in order to manage the memory alignment.

### Components

Components are structures with only one purpose, store the entity data, doesn't have logic. In the **ct-ecs** the
components are store in arrays of structures (AoS) with the same size of the entities count and for each entity there's
a component with de the same ID that represents the relation between them.

### Systems

Systems in the ct-ecs are **CRTP** classes with the registered entities, the update method and are observers of others
systems.

### Manager

The manager is the class that orchestrate everything, resize the capacity, run the systems, forward components and track
the memory alignment

## Future features

- finish the documentation
- compile-time multithreading specification
- change the observer pattern to publish-subscriber (it's a better pattern for multithreading)

## Usage

### Set up the settings

First, you'll need to create the basic design of your game, components and systems:

```cpp
// MyApp.hpp
#include <Freyr/Freyr.hpp>

class MyApp final : public skr::IApplication
{
  public:
    MyApp(const Ref<skr::ServiceProvider>& serviceProvider,
             const Ref<fr::Scene>&            scene) :
        IApplication(serviceProvider), mScene(scene)
    {
    }

    void Run() override
    {
        while(true)
          mScene->Update();
    }

  private:
    Ref<fr::Scene> mScene;
};
```

```cpp
// main.cpp
#include <Freyr/Freyr.hpp>

int main(int argc, char** argv)
{

    auto app =
        skr::ApplicationBuilder()
            .AddExtension<fr::FreyrExtension>([](fr::FreyrExtension& freyr) {
                freyr
                    .WithOptions([](fr::FreyrOptionsBuilder& options) {
                        options
                          .SetMaxEntities(512 * 1024) // Max allowed entities
                          .SetArchetypeChunkCapacity(512); // Archetype chunk capacity (tweak for better performance/task distribution)
                    })
                    .AddComponent<TransformComponent>()
                    .AddComponent<SphereColliderComponent>()
                    .AddSystem<CollisionSystem>()
                    .AddSystem<PhysicsSystem>();
            })
            .Build<MyApp>();

    app->Run();
}
```

### Components implementation

Since the first design is complete, start the implementation of the components, as the transform example:

```cpp
// TransformComponent.hpp
#include <Freyr/Freyr.hpp>

struct TransformComponent : fr::Component {
        glm::vec3 position = {0, 0, 0};
        glm::vec3 scale = {1, 1, 1};
        glm::vec3 rotation = {0, 0, 0};
};
```

#### Component unique identifier

```cpp
#include <Freyr/Freyr.hpp>

fr::GetComponentId<TransformComponent>() // returns 0
```

### Systems foundation

After implementing the components, you could start implementing the systems

```cpp
class CollisionSystem : public fr::System {
public:
  CollisionSystem(const Ref<fr::Scene>& scene) : System(scene) {}

  void PreUpdate(float deltaTime) override;
  void Update(float deltaTime) override;
  void PostUpdate(float deltaTime) override;
}
```

### Communication between systems

If a system depend on an event made by other system? for example the physics system depend on events of the collisions
detected by the collision system.
With freyr, you could communicate between systems with the observer pattern, the implementation is made by injectin the
`EventManager` and subscribing for the event:

```cpp
class PhysicsSystem : public fr::System {
public:
  PhysicsSystem(const Ref<fr::Scene>& scene, Ref<fr::EventManager>& eventManger) : System(scene)
  {
    eventManager->Subscribe<CollisionEvent>([](CollisionEvent&){
      // do collision resolution...
    });
  }
}
```

Now in the collision system you could publish for all subscribers with an specific event:

```cpp
struct CollisionEvent : fr::Event {
    EntityID entity;
    EntityID collider;
};
```

In the update method, when the collision is detected, just **notify** all observers

```cpp
class CollisionSytem : public fr::System {
public:
  CollisionSytem(const Ref<fr::Scene>& scene, Ref<fr::EventManager>& eventManger) :
  System(scene), mEventManager(eventManager)
  {
  }

  void Update(float deltaTime) override
  {
      // ...do collision detection
      auto collisionEvent = CollisionEvent{ entity, other}; 
      mEventManager->Publish<CollisionEvent>(collisionEvent);
  }
}
```
