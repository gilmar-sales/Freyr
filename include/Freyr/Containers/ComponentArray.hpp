#pragma once

#include "Freyr/Base/Component.hpp"
#include "Freyr/Containers/SparseSet.hpp"

namespace FREYR_NAMESPACE
{
    class Archetype;
    class ArchetypeChunk;

    using ComponentArrayFactory = std::function<void(Archetype*, ArchetypeChunk*)>;

    struct ComponentEntry
    {
        ComponentId           componentId;
        const char*           componentName;
        ComponentArrayFactory factory;

        operator size_t() const { return componentId; }
    };

    class IComponentArray
    {
      public:
        virtual ~IComponentArray() = default;

        operator size_t() const { return GetComponentId(); }

        virtual ComponentId GetComponentId() const = 0;

        virtual void Remove(size_t index, size_t lastIndex)                                 = 0;
        virtual void CopyComponent(size_t from, size_t to, IComponentArray* componentArray) = 0;
        virtual void Swap(size_t a, size_t b)                                               = 0;
    };

    template <typename T>
        requires IsComponent<T>
    class ComponentArray final : public IComponentArray
    {
      public:
        explicit ComponentArray(const Ref<FreyrOptions>& freyrOptions) : mFreyrOptions(freyrOptions)
        {
            mComponents.resize(freyrOptions->ArchetypeChunkCapacity);
            mElementSize = sizeof(T);
        }

        ComponentId GetComponentId() const override { return fr::GetComponentId<T>(); }

        T& operator[](size_t index) { return mComponents.data()[index]; }

        void Remove(size_t index, size_t lastIndex) override
        {
            FREYR_ASSERT(index < mComponents.size() && "Removing non-existent component.");

            mComponents[index] = mComponents[lastIndex];
        }

        T& GetComponent(const size_t index)
        {
            FREYR_ASSERT(index < mComponents.size() && "Accessing non-existent component.");

            return mComponents.data()[index];
        }

        void CopyComponent(const size_t from, const size_t to, IComponentArray* componentArray) override
        {
            const auto targetComponentArray =
                static_cast<ComponentArray*>(componentArray != nullptr ? componentArray : this);

            if (from < mComponents.size() && to < targetComponentArray->mComponents.size())
            {
                targetComponentArray->mComponents[to] = mComponents[from];
            }
        }

        void Swap(const size_t a, const size_t b) override { std::swap(mComponents[a], mComponents[b]); }

      private:
        Ref<FreyrOptions> mFreyrOptions;

        std::vector<T> mComponents;
        std::uint64_t  mElementSize;
    };

} // namespace FREYR_NAMESPACE
