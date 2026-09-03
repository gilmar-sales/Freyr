#pragma once

#include "Freyr/Containers/Archetype.hpp"
#include "Freyr/Core/ArchetypeMatchIndex.hpp"
#include "Freyr/Core/Filter.hpp"
#include "Freyr/Core/Profiling.hpp"

#include <limits>
#include <unordered_map>

namespace FREYR_NAMESPACE
{

    struct EntityIndex
    {
        Archetype*      archetype;
        ArchetypeChunk* archetypeChunk;
    };

    class ComponentManager
    {
      public:
        explicit ComponentManager(const skr::Arc<FreyrOptions>&         freyrOptions,
                                  const skr::Arc<skr::ServiceProvider>& serviceProvider) :
            mMaxEntities(freyrOptions->MaxEntities), mServiceProvider(serviceProvider),
            mRegisteredComponents(1024)
        {
            mArchetypes.reserve(1024);
            mArchetypesBySignature.reserve(1024);
            SetMaxEntities(mMaxEntities);
        }

        ~ComponentManager()
        {
            mArchetypesBySignature.clear();
            mArchetypes.clear();
        }

        void SetMaxEntities(const Entity maxEntities) { mEntityIndexes.resize(maxEntities); }

        template <typename T>
            requires IsComponent<T>
        void RegisterComponent()
        {
            const auto componentId = GetComponentId<T>();
            if (mRegisteredComponents.contains(componentId))
                return;

            mRegisteredComponents.insert(componentId);
        }

        template <typename T>
            requires IsComponent<T>
        [[nodiscard]] bool UnregisterComponent()
        {
            return UnregisterComponent(GetComponentId<T>());
        }

        [[nodiscard]] bool UnregisterComponent(const ComponentId componentId)
        {
            if (!mRegisteredComponents.contains(componentId))
                return true;

            Signature probe;
            probe.AddComponent(componentId);

            for (const auto& archetype : mArchetypes)
            {
                if (probe.Match(archetype->GetSignature()) && archetype->Count() > 0)
                    return false;
            }

            mRegisteredComponents.remove(componentId);
            return true;
        }

        template <typename T>
            requires IsComponent<T>
        [[nodiscard]] bool IsComponentRegistered() const
        {
            return IsComponentRegistered(GetComponentId<T>());
        }

        [[nodiscard]] bool IsComponentRegistered(const ComponentId componentId) const
        {
            return mRegisteredComponents.contains(componentId);
        }

        template <typename T>
            requires IsComponent<T>
        [[nodiscard]] ComponentId GetComponentIndex() const
        {
            FREYR_ASSERT(mRegisteredComponents.contains(GetComponentId<T>()) &&
                         "Component not registered before use.");

            return mRegisteredComponents.getIndex(GetComponentId<T>());
        }

        [[nodiscard]] std::size_t ArchetypeCount() const { return mArchetypes.size(); }

        void ExecutePendingMutations()
        {
            Task mutation;
            while (mPendingMutations.try_pop(mutation))
            {
                mutation();
            }
        }

        [[nodiscard]] const std::vector<Archetype*>& ArchetypesMatchingInclude(
            const Signature& includeSignature) const
        {
            return mMatchIndex.GetOrBuild(includeSignature, [this](const Signature& include) {
                return BootstrapIncludeIndex(include);
            });
        }

        [[nodiscard]] const std::vector<Archetype*>& ArchetypesMatchingFilter(
            const Filter& filter) const
        {
            return mMatchIndex.GetOrBuildFilter(filter, [this](const Filter& entry) {
                return BootstrapFilterIndex(entry);
            });
        }

        void ForEachArchetype(auto&& function) const
        {
            for (const auto& archetype : mArchetypes)
            {
                function(archetype.get());
            }
        }

        template <typename Fn>
        void ForEachMatchingArchetype(const Filter& filter, Fn&& function) const
        {
            for (Archetype* archetype : ArchetypesMatchingFilter(filter))
                function(archetype);
        }

        template <typename Fn>
        void ForEachArchetypeWithInclude(const Signature& includeSignature, Fn&& function) const
        {
            if (includeSignature.IsEmpty())
            {
                ForEachArchetype(std::forward<Fn>(function));
                return;
            }

            for (Archetype* archetype : ArchetypesMatchingInclude(includeSignature))
                function(archetype);
        }

        template <typename T>
            requires IsComponent<T>
        void AddComponent(const Entity entity, T component)
        {
            EnqueueMutation([this, entity, component = std::move(component)]() mutable {
                AddComponentNow(entity, std::move(component));
            });
        }

        template <typename... Ts>
            requires(IsComponent<Ts> and ...)
        void AddComponents(const Entity entity, const Ts&... components)
        {
            EnqueueMutation([this, entity, components...] {
                AddComponentsNow(entity, components...);
            });
        }

        template <typename... Ts, typename TFunc>
            requires(IsComponent<Ts> and ...) and (std::is_invocable_v<TFunc, Entity, Ts&...> or
                                                   std::is_invocable_v<TFunc, Ts&...>)
        void AddComponents(const Entity entity, const Ts&... components, TFunc&& callback)
        {
            EnqueueMutation(
                [this, entity, components..., callback = std::forward<TFunc>(callback)]() mutable {
                    CreateOrUpdateEntityIndexWith<Ts...>(
                        entity,
                        [entity, components..., callback = std::move(callback)](
                            EntityIndex& entityIndex) mutable {
                            auto& [actualArchetype, actualChunk] = entityIndex;
                            actualChunk->ApplyComponents<Ts...>(entity,
                                                                components...,
                                                                std::move(callback));
                        });
                });
        }

        template <typename T>
            requires IsComponent<T>
        void RemoveComponent(const Entity entity)
        {
            EnqueueMutation([this, entity] { RemoveComponentNow<T>(entity); });
        }

        template <typename... Ts>
            requires(IsComponent<Ts> and ...)
        void RemoveComponents(const Entity entity)
        {
            EnqueueMutation([this, entity] { RemoveComponentsNow<Ts...>(entity); });
        }

        template <typename T>
            requires IsComponent<T>
        T& GetComponent(const Entity& entity)
        {
            auto& [archetype, chunk] = GetEntityIndex(entity);

            FREYR_ASSERT(archetype != nullptr && chunk != nullptr);

            return chunk->GetComponent<T>(entity);
        }

        template <typename... Ts>
            requires(IsComponent<Ts> and ...)
        bool TryGetComponents(const Entity& entity, auto&& f)
        {
            auto& [archetype, chunk] = GetEntityIndex(entity);

            if (archetype == nullptr)
                return false;

            if (!archetype->HasComponents<Ts...>())
                return false;

            f(chunk->GetComponent<Ts>(entity)...);

            return true;
        }

        template <typename T>
            requires IsComponent<T>
        [[nodiscard]] bool HasComponent(const Entity& entity)
        {
            const auto& [archetype, _] = GetEntityIndex(entity);

            return archetype != nullptr && archetype->HasComponent<T>();
        }

        template <typename... Ts>
            requires(IsComponent<Ts> and ...)
        [[nodiscard]] bool HasComponents(const Entity& entity)
        {
            const auto& [archetype, _] = GetEntityIndex(entity);

            return archetype != nullptr && archetype->HasComponents<Ts...>();
        }

        void EntityDestroyed(const Entity& entity)
        {
            auto& entityIndex        = GetEntityIndex(entity);
            auto& [archetype, chunk] = entityIndex;

            if (chunk)
            {
                auto* chunkPtr = chunk;
                chunkPtr->EnqueueTask([chunkPtr, entity, &entityIndex] {
                    chunkPtr->RemoveEntity(entity);
                    entityIndex.archetype      = nullptr;
                    entityIndex.archetypeChunk = nullptr;
                });
                return;
            }

            archetype = nullptr;
            chunk     = nullptr;
        }

        inline EntityIndex& GetEntityIndex(const Entity& entity) { return mEntityIndexes[entity]; }

        skr::Arc<Archetype> AddArchetype(skr::Arc<Archetype> archetype)
        {
            FREYR_TRACE("FREYR", "ComponentManager::AddArchetype");

            const auto signature = archetype->GetSignature();

            if (const auto existingIt = mArchetypesBySignature.find(signature);
                existingIt != mArchetypesBySignature.end())
            {
                const auto& existingArchetype = existingIt->second;

                archetype->ForEachChunk([&](ArchetypeChunk* chunk) {
                    chunk->ForEach("ForEachEntity", [&](auto entity) {
                        GetEntityIndex(entity).archetype      = existingArchetype.get();
                        GetEntityIndex(entity).archetypeChunk = chunk;
                    });
                });

                archetype->MoveData(existingArchetype);

                return existingArchetype;
            }

            mArchetypesBySignature.emplace(signature, archetype);
            mArchetypes.push_back(archetype);
            IndexArchetype(archetype.get());

            archetype->ForEachChunk([&](ArchetypeChunk* chunk) {
                chunk->ForEach("ForEachEntity", [&](auto entity) {
                    GetEntityIndex(entity).archetype      = archetype.get();
                    GetEntityIndex(entity).archetypeChunk = chunk;
                });
            });

            return archetype;
        }

        template <typename... Components>
            requires(IsComponent<Components> and ...)
        void ForEach(const char* label, auto&& f)
        {
            const auto includeSignature = Signature::Make<Components...>();

            for (Archetype* archetype : ArchetypesMatchingInclude(includeSignature))
                archetype->ForEach<Components...>(label, f);
        }

      private:
        [[nodiscard]] std::vector<Archetype*> BootstrapFilterIndex(const Filter& filter) const
        {
            std::vector<Archetype*> matched;
            matched.reserve(mArchetypes.size());

            const auto& includeSignature = filter.IncludeSignature();

            if (includeSignature.IsEmpty())
            {
                ForEachArchetype([&](Archetype* archetype) {
                    if (filter.MatchArchetype(archetype))
                        matched.push_back(archetype);
                });
                return matched;
            }

            for (Archetype* archetype : ArchetypesMatchingInclude(includeSignature))
            {
                if (filter.MatchArchetype(archetype))
                    matched.push_back(archetype);
            }

            return matched;
        }

        [[nodiscard]] std::vector<Archetype*> BootstrapIncludeIndex(
            const Signature& includeSignature) const
        {
            std::vector<Archetype*> matched;
            matched.reserve(mArchetypes.size());

            const std::vector<Archetype*>* candidates     = nullptr;
            std::size_t                    candidateCount = std::numeric_limits<std::size_t>::max();

            includeSignature.ForEachComponent([&](const ComponentId componentId) {
                const auto iterator = mArchetypesByComponent.find(componentId);
                if (iterator == mArchetypesByComponent.end())
                    return;

                if (iterator->second.size() < candidateCount)
                {
                    candidateCount = iterator->second.size();
                    candidates     = &iterator->second;
                }
            });

            if (candidates != nullptr)
            {
                for (Archetype* archetype : *candidates)
                {
                    if (includeSignature.Match(archetype->GetSignature()))
                        matched.push_back(archetype);
                }

                return matched;
            }

            for (const auto& archetype : mArchetypes)
            {
                if (includeSignature.Match(archetype->GetSignature()))
                    matched.push_back(archetype.get());
            }

            return matched;
        }

        template <typename... Ts>
        static void ApplySignatureDelta(Signature& signature)
        {
            (([&] {
                 using TComponent = std::remove_reference_t<Ts>;

                 if constexpr (is_remove<TComponent>::value)
                     signature.RemoveComponent<unwrap_remove_t<TComponent>>();
                 else
                     signature.AddComponent<TComponent>();
             }()),
             ...);
        }

        template <typename... Ts>
        static Signature MakeSignatureFromComponents()
        {
            Signature signature;
            (([&] {
                 if constexpr (!is_remove<Ts>::value)
                     signature.AddComponent<Ts>();
             }()),
             ...);
            return signature;
        }

        template <typename... Ts>
        void RegisterComponentsOnArchetype(Archetype* archetype) const
        {
            (([&] {
                 using TComponent = std::remove_reference_t<Ts>;

                 if constexpr (!is_remove<TComponent>::value)
                     archetype->RegisterComponent<TComponent>();
             }()),
             ...);
        }

        template <typename... Ts>
        skr::Arc<Archetype> FindOrCreateArchetype(const Signature& signature,
                                                  Archetype*       sourceArchetype)
        {
            if (const auto existingIt = mArchetypesBySignature.find(signature);
                existingIt != mArchetypesBySignature.end())
            {
                return existingIt->second;
            }

            const auto newArchetype = mServiceProvider.lock()->GetService<Archetype>();

            if (sourceArchetype != nullptr)
                sourceArchetype->RegisterComponentsTo<Ts...>(newArchetype);

            RegisterComponentsOnArchetype<Ts...>(newArchetype.get());
            mArchetypesBySignature.emplace(newArchetype->GetSignature(), newArchetype);
            mArchetypes.push_back(newArchetype);
            IndexArchetype(newArchetype.get());
            return newArchetype;
        }

        void IndexArchetype(Archetype* archetype)
        {
            archetype->GetSignature().ForEachComponent([&](const ComponentId componentId) {
                mArchetypesByComponent[componentId].push_back(archetype);
            });

            mMatchIndex.OnArchetypeAdded(archetype);
        }

        void EnqueueMutation(Task&& mutation)
        {
            while (!mPendingMutations.try_push(std::forward<Task>(mutation)))
            {
            }
        }

        template <typename T>
        void AddComponentNow(const Entity entity, T component)
        {
            CreateOrUpdateEntityIndexWith<T>(
                entity,
                [entity, component = std::move(component)](EntityIndex& entityIndex) mutable {
                    auto& [actualArchetype, actualChunk] = entityIndex;
                    actualChunk->ApplyComponents<T>(entity, component, [](auto, auto&) {});
                });
        }

        template <typename... Ts>
        void AddComponentsNow(const Entity entity, const Ts&... components)
        {
            CreateOrUpdateEntityIndexWith<Ts...>(
                entity,
                [entity, components...](EntityIndex& entityIndex) {
                    auto& [actualArchetype, actualChunk] = entityIndex;
                    actualChunk->ApplyComponents<Ts...>(entity, components..., [](Entity, Ts&...) {
                    });
                });
        }

        template <typename T>
        void RemoveComponentNow(const Entity entity)
        {
            CreateOrUpdateEntityIndexWith<Remove<T>>(entity, [&](EntityIndex&) {});
        }

        template <typename... Ts>
        void RemoveComponentsNow(const Entity entity)
        {
            CreateOrUpdateEntityIndexWith<Remove<Ts>...>(entity, [&](EntityIndex&) {});
        }

        static void ClearEmptyEntity(const Entity          entity,
                                     EntityIndex&          entityIndex,
                                     ArchetypeChunk* const chunk)
        {
            chunk->EnqueueTask([chunk, entity, &entityIndex] {
                chunk->RemoveEntity(entity);
                entityIndex.archetype      = nullptr;
                entityIndex.archetypeChunk = nullptr;
            });
        }

        template <typename TCallback>
        void MigrateEntity(const Entity          entity,
                           EntityIndex&          entityIndex,
                           ArchetypeChunk* const oldChunk,
                           skr::Arc<Archetype>
                                       newArchetype,
                           TCallback&& callback)
        {
            const auto newChunk = newArchetype->AddEntity(entity);

            entityIndex.archetypeChunk = newChunk;
            entityIndex.archetype      = newArchetype.get();

            oldChunk->MoveData(entity, newChunk);
            callback(entityIndex);
        }

        template <typename... Ts>
        void CreateOrUpdateEntityIndexWith(const Entity entity, auto&& callback)
        {
            auto& entityIndex = GetEntityIndex(entity);
            auto  write       = mEntityIndexesLock.write();

            auto& [actualArchetype, actualChunk] = entityIndex;

            if (actualArchetype != nullptr)
            {
                auto signature = actualArchetype->GetSignature();
                ApplySignatureDelta<Ts...>(signature);

                if (signature.IsEmpty())
                {
                    ClearEmptyEntity(entity, entityIndex, actualChunk);
                    return;
                }

                if (signature != actualArchetype->GetSignature())
                {
                    const auto newArchetype =
                        FindOrCreateArchetype<Ts...>(signature, actualArchetype);
                    MigrateEntity(entity,
                                  entityIndex,
                                  actualChunk,
                                  newArchetype,
                                  std::forward<decltype(callback)>(callback));
                    return;
                }
            }
            else
            {
                const auto signature = MakeSignatureFromComponents<Ts...>();

                if (signature.IsEmpty())
                    return;

                if (const auto existingIt = mArchetypesBySignature.find(signature);
                    existingIt != mArchetypesBySignature.end())
                {
                    actualArchetype = existingIt->second.get();
                }
                else
                {
                    actualArchetype = FindOrCreateArchetype<Ts...>(signature, nullptr).get();
                }

                actualChunk = actualArchetype->AddEntity(entity);
            }

            callback(entityIndex);
        }

        friend class Registry;
        friend class Query;

        Entity mMaxEntities;

        skr::WeakArc<skr::ServiceProvider>                                mServiceProvider;
        SparseSet<ComponentId>                                            mRegisteredComponents;
        std::vector<skr::Arc<Archetype>>                                  mArchetypes;
        std::unordered_map<Signature, skr::Arc<Archetype>, SignatureHash> mArchetypesBySignature;
        std::unordered_map<ComponentId, std::vector<Archetype*>>          mArchetypesByComponent;
        mutable ArchetypeMatchIndex                                       mMatchIndex;
        std::vector<EntityIndex>                                          mEntityIndexes;
        RwLock                                                            mEntityIndexesLock;
        TaskQueue                                                         mPendingMutations;
    };
} // namespace FREYR_NAMESPACE
