#pragma once

#include "ComponentArray.hpp"
#include "Containers/SparseSet.hpp"
#include "Types/Component.hpp"

namespace FREYR_NAMESPACE
{

    class Archetype
    {
      public:
        explicit Archetype(Entity maxEntities) : mMaxEntities(maxEntities), mSignature(), mMutex(), internalName("Archetype: ")
        {
            mRegisteredEntities.resize(maxEntities);
            mRegisteredComponents.resize(MAX_COMPONENTS);
            mComponentArrays.resize(MAX_COMPONENTS);
            
        }

        Archetype(const Archetype &other)
            : mRegisteredComponents(other.mRegisteredComponents), mSignature(other.mSignature),
              mMaxEntities(other.mMaxEntities), internalName(other.internalName)
        {
            mRegisteredEntities.resize(other.mMaxEntities);
            mComponentArrays.resize(MAX_COMPONENTS);

            for (auto component : mRegisteredComponents)
            {
                mComponentArrays[mRegisteredComponents.getIndex(component)] =
                    other.mComponentArrays[component]->Clone();
            }
        }

        ~Archetype()
        {
            for (const auto &component : mRegisteredComponents)
            {
                delete (mComponentArrays[component]);
            }
        }

        void StartTracing()
        {
            TRACE_EVENT_BEGIN("ECS", internalName.c_str(), perfetto::Track((uint64_t)this));
        }

        void EndTracing()
        {
            TRACE_EVENT_END("ECS", perfetto::Track((uint64_t)this));
        }

        template<typename T>
        void RegisterComponent()
        {
            assert(!mRegisteredComponents.contains(GetComponentId<T>()) &&
                   "Registering component type more than once.");

            mSignature[GetComponentId<T>()] = true;
            mRegisteredComponents.insert(GetComponentId<T>());
            mComponentArrays[mRegisteredComponents.getIndex(GetComponentId<T>())] = new ComponentArray<T>(mMaxEntities);

            if (internalName.size() > 12)
            {
                internalName += ", ";
            }
                
            internalName += typeid(T).name();
        }

        template<typename T>
        void AddComponent(const Entity &entity, T component)
        {
            mRegisteredEntities.insert(entity);
            GetComponentArray<T>()->InsertData(entity, component);
        }

        template<typename T>
        void RemoveComponent(const Entity &entity)
        {
            GetComponentArray<T>()->RemoveData(entity);
        }

        template<typename T>
        T &GetComponent(const Entity &entity)
        {
            return GetComponentArray<T>()->GetData(entity);
        }

        void RemoveEntity(const Entity &entity)
        {
            for (auto const &component : mRegisteredComponents)
            {
                mComponentArrays[mRegisteredComponents.getIndex(component)]->RemoveEntity(entity);
            }

            mRegisteredEntities.remove(entity);
        }

        const Signature &GetSignature() { return mSignature; }

        template<typename... Components>
        void ForEach(std::string_view label, auto &&f)
        {
            std::scoped_lock lock(mMutexes[GetComponentId<Components>()]...);
            
            TRACE_EVENT_BEGIN("ECS", label.data(), perfetto::Track((uint64_t)this),  "entity_count", mRegisteredEntities.size());
            for (const auto &entity : mRegisteredEntities)
            {
                f(GetComponentArray<Components>()->GetData(entity)...);
            }
            TRACE_EVENT_END("ECS", perfetto::Track((uint64_t)this));
        }

        std::mutex &Mutex() { return mMutex; }

      protected:
        friend class ComponentManager;
        void MoveData(const Entity &entity, Archetype *other)
        {
            for (auto component : mRegisteredComponents)
            {
                mComponentArrays[component]->MoveData(entity, other->mComponentArrays[component]);
            }

            RemoveEntity(entity);
        };

        template<typename T>
        ComponentArray<T> *GetComponentArray()
        {
            assert(mRegisteredComponents.contains(GetComponentId<T>()) && "Component not registered before use.");

            return static_cast<ComponentArray<T> *>(
                mComponentArrays[mRegisteredComponents.getIndex(GetComponentId<T>())]);
        }

      private:
        std::string internalName;
        std::mutex mMutex;
        Signature mSignature;
        std::mutex mMutexes[MAX_COMPONENTS];
        std::vector<IComponentArray *> mComponentArrays;
        SparseSet<ComponentId> mRegisteredComponents;
        SparseSet<Entity> mRegisteredEntities;
        Entity mMaxEntities;
    };

} // namespace FREYR_NAMESPACE
