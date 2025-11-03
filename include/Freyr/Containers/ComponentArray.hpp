#pragma once

#include "Freyr/Base/Component.hpp"
#include "Freyr/Base/Entity.hpp"
#include "Freyr/Containers/SparseSet.hpp"

namespace FREYR_NAMESPACE
{
    class Archetype;
    class ArchetypeChunk;

    using ComponentArrayFactory =
        std::function<void(Archetype*, ArchetypeChunk*)>;

    struct ComponentEntry
    {
        ComponentId           componentId;
        ComponentArrayFactory factory;

        operator size_t() const { return componentId; }
    };

    class IComponentArray
    {
      public:
        virtual ~IComponentArray() = default;

        operator size_t() { return GetComponentId(); }

        virtual ComponentId GetComponentId() = 0;

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
        explicit ComponentArray(const Ref<FreyrOptions>& freyrOptions) :
            mFreyrOptions(freyrOptions), mEntities(freyrOptions->MaxEntities)
        {
            mComponents.resize(freyrOptions->ArchetypeChunkCapacity);
            mElementSize = sizeof(T);
        }

        ComponentId GetComponentId() override
        {
            return fr::GetComponentId<T>();
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

            auto indexOfRemovedEntity = mEntities.getIndex(entity);
            auto indexOfLastElement   = mEntities.size() - 1;

            mEntities.remove(entity);
            mComponents[indexOfRemovedEntity] = mComponents[indexOfLastElement];
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
            const auto targetComponentArray = static_cast<ComponentArray*>(
                componentArray != nullptr ? componentArray : this);

            if (mEntities.contains(from) &&
                targetComponentArray->mEntities.contains(to))
            {
                targetComponentArray
                    ->mComponents[targetComponentArray->mEntities.getIndex(
                        to)] = mComponents[mEntities.getIndex(from)];
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
            return new ComponentArray<T>(mFreyrOptions);
        }

        void Swap(const Entity& a, const Entity& b) override
        {
            mEntities.swap(a, b);
        }

        Entity Count() { return mEntities.size(); }

      private:
        Ref<FreyrOptions> mFreyrOptions;

        std::vector<T>    mComponents;
        SparseSet<Entity> mEntities;
        std::uint64_t     mElementSize;
    };

} // namespace FREYR_NAMESPACE
