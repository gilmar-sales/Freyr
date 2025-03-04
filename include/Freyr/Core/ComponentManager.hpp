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
            mMaxEntities(freyrOptions->MaxEntities),
            mRegisteredComponents(1024), mServiceProvider(serviceProvider)
        {
            mArchetypes.reserve(1024);
            SetMaxEntities(mMaxEntities);
        }

        ~ComponentManager() { mArchetypes.clear(); }

        void SetMaxEntities(const Entity maxEntities)
        {
            mEntityToArchetype.resize(maxEntities);
        }

        template <typename T>
        void RegisterComponent()
        {
            FREYR_ASSERT(!mRegisteredComponents.contains(GetComponentId<T>()) &&
                         "Registering component type more than once.");

            mRegisteredComponents.insert(GetComponentId<T>());
        }

        template <typename T>
        ComponentId GetComponentIndex()
        {
            FREYR_ASSERT(mRegisteredComponents.contains(GetComponentId<T>()) &&
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
                    std::shared_ptr<Archetype> newArchetype = nullptr;

                    for (const auto& existingArchetype : mArchetypes)
                    {
                        if (existingArchetype->GetSignature() == signature)
                        {
                            newArchetype = existingArchetype;
                            break;
                        }
                    }

                    if (newArchetype == nullptr)
                    {
                        newArchetype =
                            mServiceProvider->GetService<Archetype>();
                        newArchetype->RegisterComponent<T>();
                        mArchetypes.push_back(newArchetype);
                    }

                    archetype->MoveData(entity, newArchetype);

                    newArchetype->AddComponent<T>(entity, component);
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
            auto& [_, archetype] = GetEntityArchetype(entity);

            FREYR_ASSERT(archetype != nullptr);

            archetype->RemoveComponent<T>(entity);
        }

        template <typename T>
        T& GetComponent(const Entity& entity)
        {
            auto& [entityA, archetype] = GetEntityArchetype(entity);

            FREYR_ASSERT(entityA == entity && archetype != nullptr);

            return archetype->GetComponent<T>(entity);
        }

        template <typename... Ts>
        std::tuple<Ts&...> GetComponents(const Entity& entity)
        {
            auto& [entityA, archetype] = GetEntityArchetype(entity);

            FREYR_ASSERT(entityA == entity && archetype != nullptr);

            return archetype->GetComponents<Ts...>(entity);
        }

        template <typename T>
        [[nodiscard]] bool HasComponent(const Entity& entity)
        {
            const auto& [entityA, archetype] = GetEntityArchetype(entity);

            FREYR_ASSERT(entityA == entity && archetype != nullptr);

            return archetype->HasComponent<T>();
        }

        void EntityDestroyed(const Entity& entity)
        {
            const auto& [entityA, archetype] = GetEntityArchetype(entity);
            FREYR_ASSERT(entityA == entity && archetype != nullptr);

            archetype->RemoveEntity(entity);
        }

        EntityArchetype& GetEntityArchetype(const Entity& entity)
        {
            return mEntityToArchetype[entity];
        }

        std::shared_ptr<Archetype> AddArchetype(
            std::shared_ptr<Archetype> archetype)
        {
            FREYR_PROFILING_BEGIN("FREYR",
                                  "ComponentManager::AddArchetype",
                                  perfetto::Track(1));
            const auto signature = archetype->GetSignature();

            if (const auto existingArchetype = std::ranges::find_if(
                    mArchetypes,
                    [&](const std::shared_ptr<Archetype>& arch) {
                        return arch->GetSignature() == signature;
                    });
                existingArchetype != mArchetypes.end())
            {
                archetype->MoveData(*existingArchetype);
                for (const auto entity :
                     archetype->mRegisteredEntities.getDense())
                {
                    mEntityToArchetype[entity].entity    = entity;
                    mEntityToArchetype[entity].archetype = *existingArchetype;
                }

                FREYR_PROFILING_END("FREYR", perfetto::Track(1));

                return *existingArchetype;
            }

            mArchetypes.push_back(archetype);

            for (const auto entity : archetype->mRegisteredEntities.getDense())
            {
                mEntityToArchetype[entity].entity    = entity;
                mEntityToArchetype[entity].archetype = archetype;
            }

            FREYR_PROFILING_END("FREYR", perfetto::Track(1));

            return archetype;
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
