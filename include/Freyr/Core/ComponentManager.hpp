#pragma once

#include "Freyr/Containers/Archetype.hpp"

namespace FREYR_NAMESPACE
{

    struct EntityArchetype
    {
        Entity                     entity;
        std::shared_ptr<Archetype> archetype;
    };

    class ComponentManager
    {
      public:
        ComponentManager(Entity maxEntities) : mMaxEntities(maxEntities)
        {
            mRegisteredComponents.resize(MAX_COMPONENTS);
            mArchetypes.reserve(512);
            mEntityToArchetype.resize(maxEntities);
        }

        ~ComponentManager() { mArchetypes.clear(); }

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
            auto& entityArchetype  = mEntityToArchetype[entity];
            entityArchetype.entity = entity;

            if (entityArchetype.archetype != nullptr)
            {
                Signature signature = entityArchetype.archetype->GetSignature();
                signature[GetComponentId<T>()] = true;

                if (signature != entityArchetype.archetype->GetSignature())
                {
                    std::shared_ptr<Archetype> archetype = nullptr;

                    for (auto existingArchetype : mArchetypes)
                    {
                        if (existingArchetype->GetSignature() == signature)
                        {
                            archetype = existingArchetype;
                            break;
                        }
                    }

                    if (archetype == nullptr)
                    {
                        archetype = std::make_shared<Archetype>(
                            *entityArchetype.archetype);
                        archetype->RegisterComponent<T>();
                        mArchetypes.push_back(archetype);
                    }
                    entityArchetype.archetype->MoveData(entity, archetype);
                    entityArchetype.archetype = archetype;

                    archetype->AddComponent<T>(entity, component);
                }
            }
            else
            {
                std::shared_ptr<Archetype> archetype = nullptr;
                Signature                  signature = {};
                signature[GetComponentId<T>()]       = true;

                for (auto existingArchetype : mArchetypes)
                {
                    if (existingArchetype->GetSignature() == signature)
                    {
                        archetype = existingArchetype;
                        break;
                    }
                }

                if (archetype == nullptr)
                {
                    archetype = std::make_shared<Archetype>(mMaxEntities);
                    archetype->RegisterComponent<T>();
                    mArchetypes.push_back(archetype);
                }

                archetype->AddComponent<T>(entity, component);
                entityArchetype.archetype = archetype;
            }
        }

        template <typename T>
        void RemoveComponent(const Entity& entity)
        {
            auto& entityArchetype = mEntityToArchetype[entity];
            assert(entityArchetype.archetype != nullptr);

            entityArchetype.archetype->RemoveComponent<T>(entity);
        }

        template <typename T>
        T& GetComponent(const Entity& entity)
        {
            auto& entityArchetype = mEntityToArchetype[entity];
            assert(entityArchetype.entity == entity &&
                   entityArchetype.archetype != nullptr);

            return entityArchetype.archetype->GetComponent<T>(entity);
        }

        template <typename T>
        bool HasComponent(const Entity& entity)
        {
            auto& entityArchetype = mEntityToArchetype[entity];
            assert(entityArchetype.entity == entity &&
                   entityArchetype.archetype != nullptr);

            return entityArchetype.archetype->HasComponent<T>();
        }

        void EntityDestroyed(const Entity& entity)
        {
            auto& entityArchetype = mEntityToArchetype[entity];
            assert(entityArchetype.entity == entity &&
                   entityArchetype.archetype != nullptr);

            entityArchetype.archetype->RemoveEntity(entity);
        }

        std::shared_ptr<Archetype> AddArchetype(
            std::shared_ptr<Archetype> archetype)
        {
            auto signature = archetype->GetSignature();

            auto existingArchetype = std::find_if(
                mArchetypes.begin(),
                mArchetypes.end(),
                [&](std::shared_ptr<Archetype> archetype) {
                    return archetype->GetSignature() == signature;
                });

            if (existingArchetype != mArchetypes.end())
            {
                archetype->MoveData(*existingArchetype);
                for (auto entity : archetype->mRegisteredEntities)
                {
                    mEntityToArchetype[entity].entity    = entity;
                    mEntityToArchetype[entity].archetype = *existingArchetype;
                }

                return *existingArchetype;
            }
            else
            {
                mArchetypes.push_back(archetype);
                for (auto entity : archetype->mRegisteredEntities)
                {
                    mEntityToArchetype[entity].entity    = entity;
                    mEntityToArchetype[entity].archetype = archetype;
                }

                return archetype;
            }
        }

        void StartTracing()
        {
            for (auto archetype : mArchetypes)
            {
                archetype->StartTracing();
            }
        }

        void EndTracing()
        {
            for (auto archetype : mArchetypes)
            {
                archetype->EndTracing();
            }
        }

      private:
        friend class ECSManager;

        Entity mMaxEntities;

        SparseSet<ComponentId> mRegisteredComponents;
        std::unordered_map<Signature, std::shared_ptr<Archetype>>
                                                mSignToArchetype;
        std::vector<std::shared_ptr<Archetype>> mArchetypes;
        std::vector<EntityArchetype>            mEntityToArchetype;
    };

} // namespace FREYR_NAMESPACE
