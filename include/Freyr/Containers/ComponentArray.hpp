#pragma once

#include "Freyr/Base/Component.hpp"
#include "Freyr/Base/Entity.hpp"
#include "Freyr/Containers/SparseSet.hpp"

namespace FREYR_NAMESPACE
{

    class IComponentArray
    {
      public:
        virtual ~IComponentArray() = default;

        virtual void Resize(size_t size)                                   = 0;
        virtual void AddEntity(Entity entity)                              = 0;
        virtual void CopyEntity(Entity from,
                                Entity to,
                                void*  componentArray = nullptr)            = 0;
        virtual void RemoveEntity(Entity entity)                           = 0;
        virtual void MoveData(Entity entity, IComponentArray* destination) = 0;
        virtual void MoveData(IComponentArray* destination)                = 0;
        virtual IComponentArray* Clone()                                   = 0;
        virtual void             Swap(const Entity& a, const Entity& b)    = 0;
    };

    template <typename T>
        requires IsComponent<T>
    class ComponentArray : public IComponentArray
    {
      public:
        explicit ComponentArray(std::uint64_t maxEntities) :
            mEntities(maxEntities)
        {
            mComponents.resize(maxEntities);
            mElementSize = sizeof(T);
        }

        void InsertData(const Entity& entity, const T& component)
        {
            mEntities.insert(entity);
            mComponents[mEntities.getIndex(entity)] = component;
        }

        void Resize(size_t size) override
        {
            mComponents.resize(size);
            mEntities.resize(size);
        }

        void RemoveData(const Entity entity)
        {
            FREYR_ASSERT(mEntities.contains(entity) &&
                   "Removing non-existent component.");

            std::uint64_t indexOfRemovedEntity = mEntities.getIndex(entity);
            std::uint64_t indexOfLastElement   = mEntities.size() - 1;
            mComponents[indexOfRemovedEntity] = mComponents[indexOfLastElement];
            mEntities.remove(entity);
        }

        T& GetData(const Entity entity)
        {
            FREYR_ASSERT(mEntities.contains(entity) &&
                   "Retrieving non-existent component.");

            return mComponents[mEntities.getIndex(entity)];
        }

        void AddEntity(const Entity entity) override
        {
            mEntities.insert(entity);
        }

        void CopyEntity(const Entity from,
                        const Entity to,
                        void*        componentArray = nullptr) override
        {
            const auto compo = static_cast<ComponentArray*>(
                componentArray != nullptr ? componentArray : this);

            if (mEntities.contains(from) && compo->mEntities.contains(to))
            {
                compo->mComponents[mEntities.getIndex(to)] =
                    mComponents[mEntities.getIndex(from)];
            }
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

        void MoveData(IComponentArray* destination) override
        {
            auto componentArray = static_cast<ComponentArray<T>*>(destination);

            auto lastComponent = componentArray->mComponents.begin() +
                                 componentArray->mEntities.size();

            componentArray->mComponents.insert(
                lastComponent,
                mComponents.begin(),
                mComponents.begin() + mEntities.size());

            for (auto entity : mEntities)
            {
                componentArray->AddEntity(entity);
            }
        }

        IComponentArray* Clone() override
        {
            return new ComponentArray<T>(mEntities.size());
        }

        void Swap(const Entity& a, const Entity& b) override
        {
            mEntities.swap(a, b);
        }

        Entity Count() { return mEntities.size(); }

      private:
        std::vector<T>    mComponents;
        SparseSet<Entity> mEntities;
        std::uint64_t     mElementSize;
    };

} // namespace FREYR_NAMESPACE
