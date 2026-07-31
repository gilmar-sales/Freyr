#pragma once

#include "MPMCQueue.hpp"

#include <cmath>

#include "Freyr/Containers/ArchetypeChunk.hpp"
#include "Freyr/Containers/Signature.hpp"
#include "Freyr/Core/FreyrOptions.hpp"
#include "Freyr/Core/Profiling.hpp"
#include "Freyr/Core/ThreadPool.hpp"

namespace FREYR_NAMESPACE
{
    template <typename T>
    struct Remove
    {
    };

    template <typename T>
    struct is_remove : std::false_type
    {
    };
    template <typename T>
    struct is_remove<Remove<T>> : std::true_type
    {
    };

    template <typename T>
    struct unwrap_remove
    {
        using type = T;
    };
    template <typename T>
    struct unwrap_remove<Remove<T>>
    {
        using type = T;
    };
    template <typename T>
    using unwrap_remove_t = typename unwrap_remove<T>::type;

    class Archetype
    {

      public:
        explicit Archetype(const Ref<FreyrOptions>& freyrOptions,
                           const Ref<ThreadPool>&   taskManager,
                           const Ref<TaskCounter>&  taskCounter) :
            mInternalName("Archetype: "), mRegisteredComponents(512), mFreyrOptions(freyrOptions),
            mThreadPool(taskManager), mTaskCounter(taskCounter)
        {
        }

        ~Archetype()
        {
            for (const auto chunk : mArchetypeChunks)
            {
                delete chunk;
            }
        }

        ArchetypeChunk* AddEntity(const Entity entity)
        {
            {
                auto write = mLock.write();
                if (!mArchetypeChunks.empty())
                {

                    if (const auto chunk = mArchetypeChunks.front(); chunk->IsFull())
                    {
                        mArchetypeChunks.pop_front();
                        mArchetypeChunks.push_back(chunk);
                    }
                }

                for (const auto chunk : mArchetypeChunks)
                {
                    if (chunk->IsFull())
                        continue;

                    if (chunk->TryAddEntity(entity))
                        return chunk;
                }
            }

            const auto chunk = CreateChunk();
            chunk->TryAddEntity(entity);

            return chunk;
        }

        void CopyEntity(Entity from, Entity to);

        template <typename T>
        void RegisterComponent()
        {
            FREYR_ASSERT(!mRegisteredComponents.contains(GetComponentId<T>()) &&
                         "Registering component type more than once.");

            mSignature.AddComponent<T>();

            ComponentArrayFactory componentFactory =
                [](Archetype* archetype, ArchetypeChunk* chunk) { chunk->AddComponentArray<T>(); };

            InternalRegisterComponent<T>(componentFactory);

            for (const auto& chunk : mArchetypeChunks)
            {
                componentFactory(this, chunk);
            }
        }

        template <typename T>
        void RemoveComponent(const Entity& entity)
        {
            GetComponentArray<T>()->RemoveData(entity);
        }

        template <typename T>
        [[nodiscard]] bool HasComponent() const
        {
            thread_local auto signature = Signature::Make<T>();
            return signature.Match(mSignature);
        }

        template <typename... Ts>
        [[nodiscard]] bool HasComponents() const
        {
            thread_local auto signature = Signature::Make<Ts...>();
            return signature.Match(mSignature);
        }

        void StartTasks()
        {
            for (const auto chunk : mArchetypeChunks)
            {
                chunk->StartTasks();
            }
        }

        std::optional<Entity> First() const
        {
            for (const auto& chunk : mArchetypeChunks)
            {
                if (chunk->Count() > 0)
                    return chunk->mRegisteredEntities.at(0);
            }

            return std::nullopt;
        }

        void GetRegisteredEntities(std::vector<Entity>& buffer) const
        {
            for (const auto& chunk : mArchetypeChunks)
            {
                chunk->GetRegisteredEntities(buffer);
            }
        }

        [[nodiscard]] const Signature& GetSignature() const { return mSignature; }

        void ForEachChunk(auto&& function)
        {
            for (auto chunk : mArchetypeChunks)
            {
                function(chunk);
            }
        }

        template <typename... Components>
        void ForEach(const char* label, auto&& function)
        {
            auto read = mLock.read();
            for (auto chunk : mArchetypeChunks)
            {
                chunk->ForEach<Components...>(label, function);
            }
        }

        template <typename... Components>
        void ForEachAsync(const char* label, auto&& function)
        {
            auto read = mLock.read();
            for (auto chunk : mArchetypeChunks)
            {
                chunk->ForEachAsync<Components...>(label, function);
            }
        }

        template <typename... Components>
        void Map(
            auto&& mapFunction,
            Entity index,
            std::vector<decltype(mapFunction(*(new Entity {}), *(new Components {})...))>& buffer)
        {
            auto read = mLock.read();
            for (auto chunk : mArchetypeChunks)
            {
                chunk->Map<Components...>(mapFunction, index, buffer);
            }
        }

        template <typename... Components>
        void ForEach(const char* label, SparseSet<Entity>& entities, auto&& function)
        {
            auto read = mLock.read();
            for (auto chunk : mArchetypeChunks)
            {
                chunk->ForEach<Components...>(label, entities, function);
            }
        }

        void EnsureCapacity(const size_t capacity)
        {
            const size_t chunkCount =
                std::ceil(static_cast<float>(capacity) /
                          static_cast<float>(mFreyrOptions->ArchetypeChunkCapacity));

            if (chunkCount == 0)
                return;

            for (auto i = mArchetypeChunks.size(); i < chunkCount; ++i)
            {
                CreateChunk();
            }
        }

        [[nodiscard]] std::size_t Count() const
        {
            size_t result = 0;

            for (auto&& chunk : mArchetypeChunks)
                result += chunk->Count();

            return result;
        }

      protected:
        friend class ComponentManager;
        friend class Registry;

        template <typename T>
        void InternalRegisterComponent(const ComponentArrayFactory& componentArrayFactory)
        {
            if (mRegisteredComponents.contains(GetComponentId<T>()))
                return;

            if (mInternalName.size() > 12)
            {
                mInternalName += ", ";
            }

            mInternalName += skr::type_name<T>();

            mRegisteredComponents.insert(ComponentEntry { .componentId   = GetComponentId<T>(),
                                                          .componentName = skr::type_name<T>().data(),
                                                          .factory       = componentArrayFactory });
        }

        template <typename... Ts>
        void RegisterComponentsTo(const Ref<Archetype>& destination)
        {
            auto components = mRegisteredComponents;

            (([&] {
                 using TComponent = std::remove_reference_t<Ts>;

                 if constexpr (is_remove<TComponent>::value)
                     components.remove(GetComponentId<unwrap_remove_t<TComponent>>());
             }()),
             ...);

            for (const auto& componentEntry : components)
            {
                destination->mRegisteredComponents.insert(componentEntry);

                if (destination->mInternalName.size() > 12)
                {
                    destination->mInternalName += ", ";
                }

                destination->mInternalName += componentEntry.componentName;
                destination->mSignature.AddComponent(componentEntry.componentId);

                destination->ForEachChunk([&](ArchetypeChunk* destinationChunk) {
                    componentEntry.factory(this, destinationChunk);
                });
            }
        }

        void MoveData(const Ref<Archetype>& destination)
        {
            for (auto chunk : mArchetypeChunks)
            {
                destination->mArchetypeChunks.push_back(chunk);
            }

            destination->EnsureCapacity(Count() + destination->Count());

            mArchetypeChunks.clear();
        };

        template <typename T>
        ComponentArray<T>* GetComponentArray()
        {
            FREYR_ASSERT(mRegisteredComponents.contains(GetComponentId<T>()) &&
                         "Component not registered before use.");

            return static_cast<ComponentArray<T>*>(
                mArchetypeChunks[mRegisteredComponents.getIndex(GetComponentId<T>())]);
        }

      private:
        ArchetypeChunk* CreateChunk()
        {
            const auto chunk =
                new ArchetypeChunk(mInternalName, mFreyrOptions, mThreadPool, mTaskCounter);

            if (mThreadPool->IsRunning())
                chunk->StartTasks();

            for (const auto& componentEntry : mRegisteredComponents)
            {
                componentEntry.factory(this, chunk);
            }

            {
                auto write = mLock.write();
                mArchetypeChunks.push_back(chunk);
            }

            return chunk;
        }

        friend class ArchetypeChunk;

        std::string mInternalName;
        Signature   mSignature;

        RwLock                     mLock;
        std::list<ArchetypeChunk*> mArchetypeChunks;
        SparseSet<ComponentEntry>  mRegisteredComponents;
        Ref<FreyrOptions>          mFreyrOptions;
        Ref<ThreadPool>            mThreadPool;
        Ref<TaskCounter>           mTaskCounter;
    };

} // namespace FREYR_NAMESPACE