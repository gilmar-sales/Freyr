#pragma once

#include "Freyr/Containers/Archetype.hpp"
#include <boost/container/vector.hpp>

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
        explicit ComponentManager(Entity maxEntities) :
            mMaxEntities(maxEntities)
        {
            mRegisteredComponents.resize(512);
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
            auto& [entityA, archetype] = mEntityToArchetype[entity];
            entityA                    = entity;

            if (archetype != nullptr)
            {
                Signature signature = archetype->GetSignature();
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
                        archetype = std::make_shared<Archetype>(*archetype);
                        archetype->RegisterComponent<T>();
                        mArchetypes.push_back(archetype);
                    }
                    archetype->MoveData(entity, archetype);

                    archetype->AddComponent<T>(entity, component);
                }
            }
            else
            {
                Signature signature = MakeSignature<T>();

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
                    archetype = std::make_shared<Archetype>(mMaxEntities);
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
        bool HasComponent(const Entity& entity) const
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
            else
            {
                mArchetypes.push_back(archetype);
                for (const auto entity : archetype->mRegisteredEntities)
                {
                    mEntityToArchetype[entity].entity    = entity;
                    mEntityToArchetype[entity].archetype = archetype;
                }

                return archetype;
            }
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

        SparseSet<ComponentId> mRegisteredComponents;
        boost::container::vector<std::shared_ptr<Archetype>> mArchetypes;
        boost::container::vector<EntityArchetype>            mEntityToArchetype;
    };

} // namespace FREYR_NAMESPACE
