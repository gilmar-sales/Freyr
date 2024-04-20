#pragma once

#include "Freyr/Containers/SparseSet.hpp"
#include "Freyr/Base/Component.hpp"
#include "Freyr/Base/Entity.hpp"

namespace FREYR_NAMESPACE
{

    class IComponentArray
    {
      public:
        virtual ~IComponentArray()                                                     = default;
        virtual void             RemoveEntity(Entity entity)                           = 0;
        virtual void             MoveData(Entity entity, IComponentArray* destination) = 0;
        virtual IComponentArray* Clone()                                               = 0;
    };

    template <typename T>
        requires IsComponent<T>
    class ComponentArray : public IComponentArray
    {
      public:
        explicit ComponentArray(std::uint64_t maxEntities)
        {
            mComponents.resize(maxEntities);
            mEntities.resize(maxEntities);
            mElementSize = sizeof(T);
        }

        void InsertData(const Entity& entity, const T& component)
        {
            mEntities.insert(entity);
            mComponents[mEntities.getIndex(entity)] = component;
        }

        void RemoveData(Entity entity)
        {
            assert(mEntities.contains(entity) && "Removing non-existent component.");

            std::uint64_t indexOfRemovedEntity = mEntities.getIndex(entity);
            std::uint64_t indexOfLastElement   = mEntities.size() - 1;
            mComponents[indexOfRemovedEntity]  = mComponents[indexOfLastElement];
            mEntities.remove(entity);
        }

        T& GetData(Entity entity)
        {
            assert(mEntities.contains(entity) && "Retrieving non-existent component.");

            return mComponents[mEntities.getIndex(entity)];
        }

        void RemoveEntity(Entity entity) override
        {
            if (mEntities.contains(entity))
            {
                RemoveData(entity);
            }
        }

        void MoveData(Entity entity, IComponentArray* destination) override
        {
            auto componentArray = static_cast<ComponentArray<T>*>(destination);

            componentArray->InsertData(entity, GetData(entity));
            RemoveData(entity);
        }

        IComponentArray* Clone() override { return new ComponentArray<T>(mComponents.size()); }

      private:
        std::vector<T>    mComponents;
        SparseSet<Entity> mEntities;
        std::uint64_t     mElementSize;
    };

} // namespace FREYR_NAMESPACE
