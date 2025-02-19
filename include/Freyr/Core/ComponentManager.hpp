#pragma once

#include "Freyr/Containers/Archetype.hpp"
#include "Freyr/Core/Profiling.hpp"

namespace FREYR_NAMESPACE
{
    struct EntityArchetype
    {
        Entity                     entity {};
        std::shared_ptr<Archetype> archetype;
    };

    class ComponentManager
    {
      public:
        explicit ComponentManager(
            const std::shared_ptr<FreyrOptions>&    freyrOptions,
            const std::shared_ptr<ServiceProvider>& serviceProvider) :
            mMaxEntities(freyrOptions->InitialCapacity),
            mServiceProvider(serviceProvider)
        {
            mRegisteredComponents.resize(1024);
            mArchetypes.reserve(1024);
            SetMaxEntities(freyrOptions->InitialCapacity);
        }

        ~ComponentManager() { mArchetypes.clear(); }

        void SetMaxEntities(const Entity maxEntities)
        {
            mEntityToArchetype.resize(maxEntities);
        }

        template <typename T>
        void RegisterComponent()
        {
            assert(!mRegisteredComponents.contains(GetComponentId<T>()) &&
                   "Registering component type more than once.");

            mRegisteredComponents.insert(GetComponentId<T>());
        }

        template <typename T>
        ComponentId GetComponentIndex()
        {
            assert(mRegisteredComponents.contains(GetComponentId<T>()) &&
                   "Component not registered before use.");

            return mRegisteredComponents.getIndex(GetComponentId<T>());
        }

        template <typename T>
        void AddComponent(const Entity& entity, T component)
        {
            auto& [entityA, archetype] = mEntityToArchetype[entity];
            entityA                    = entity;

            if (archetype != nullptr)
            {
                auto signature = archetype->GetSignature();
                signature.AddComponent<T>();

                if (signature != archetype->GetSignature())
                {
                    for (const auto& existingArchetype : mArchetypes)
                    {
                        if (existingArchetype->GetSignature() == signature)
                        {
                            archetype = existingArchetype;
                            break;
                        }
                    }

                    if (archetype == nullptr)
                    {
                        archetype = mServiceProvider->GetService<Archetype>();
                        archetype->RegisterComponent<T>();
                        mArchetypes.push_back(archetype);
                    }

                    archetype->MoveData(entity, archetype);

                    archetype->AddComponent<T>(entity, component);
                }
            }
            else
            {
                const Signature signature = MakeSignature<T>();

                for (const auto& existingArchetype : mArchetypes)
                {
                    if (existingArchetype->GetSignature() == signature)
                    {
                        archetype = existingArchetype;
                        break;
                    }
                }

                if (archetype == nullptr)
                {
                    archetype = mServiceProvider->GetService<Archetype>();
                    archetype->RegisterComponent<T>();
                    mArchetypes.push_back(archetype);
                }

                archetype->AddComponent<T>(entity, component);
            }
        }

        template <typename T>
        void RemoveComponent(const Entity& entity)
        {
            auto& [_, archetype] = mEntityToArchetype[entity];
            assert(archetype != nullptr);

            archetype->RemoveComponent<T>(entity);
        }

        template <typename T>
        T& GetComponent(const Entity& entity)
        {
            auto& [entityA, archetype] = mEntityToArchetype[entity];

            assert(entityA == entity && archetype != nullptr);

            return archetype->GetComponent<T>(entity);
        }

        template <typename T>
        [[nodiscard]] bool HasComponent(const Entity& entity) const
        {
            const auto& [entityA, archetype] = mEntityToArchetype[entity];

            assert(entityA == entity && archetype != nullptr);

            return archetype->HasComponent<T>();
        }

        void EntityDestroyed(const Entity& entity) const
        {
            const auto& [entityA, archetype] = mEntityToArchetype[entity];
            assert(entityA == entity && archetype != nullptr);

            archetype->RemoveEntity(entity);
        }

        std::shared_ptr<Archetype> AddArchetype(
            std::shared_ptr<Archetype> archetype)
        {
            FREYR_PROFILING_BEGIN("FREYR",
                                  "ComponentManager::AddArchetype",
                                  perfetto::Track((uint64_t) this));
            const auto signature = archetype->GetSignature();

            if (const auto existingArchetype = std::ranges::find_if(
                    mArchetypes,
                    [&](const std::shared_ptr<Archetype>& arch) {
                        return arch->GetSignature() == signature;
                    });
                existingArchetype != mArchetypes.end())
            {
                archetype->MoveData(*existingArchetype);
                for (const auto entity : archetype->mRegisteredEntities)
                {
                    mEntityToArchetype[entity].entity    = entity;
                    mEntityToArchetype[entity].archetype = *existingArchetype;
                }

                return *existingArchetype;
            }

            mArchetypes.push_back(archetype);
            for (const auto entity : archetype->mRegisteredEntities)
            {
                mEntityToArchetype[entity].entity    = entity;
                mEntityToArchetype[entity].archetype = archetype;
            }

            FREYR_PROFILING_END("FREYR", perfetto::Track((uint64_t) this));

            return archetype;
        }

        void StartTracing()
        {
            for (const auto& archetype : mArchetypes)
            {
                archetype->StartTracing();
            }
        }

        void EndTracing()
        {
            for (const auto& archetype : mArchetypes)
            {
                archetype->EndTracing();
            }
        }

      private:
        friend class Scene;

        Entity mMaxEntities;

        std::shared_ptr<ServiceProvider>        mServiceProvider;
        SparseSet<ComponentId>                  mRegisteredComponents;
        std::vector<std::shared_ptr<Archetype>> mArchetypes;
        std::vector<EntityArchetype>            mEntityToArchetype;
    };

} // namespace FREYR_NAMESPACE
