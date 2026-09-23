#pragma once

#include "Freyr/Containers/Archetype.hpp"
#include "Freyr/Core/ArchetypeMatchIndex.hpp"
#include "Freyr/Core/ComponentTicks.hpp"
#include "Freyr/Core/EntityManager.hpp"
#include "Freyr/Core/Filter.hpp"
#include "Freyr/Core/ObserverManager.hpp"
#include "Freyr/Core/Profiling.hpp"
#include "Freyr/Serialization/EntityRemapper.hpp"

#include <algorithm>
#include <cstring>
#include <limits>
#include <ostream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace FREYR_NAMESPACE
{

    class ComponentManager;

    struct EntityIndex
    {
        Archetype*      archetype;
        ArchetypeChunk* archetypeChunk;
    };

    struct SnapshotCodec
    {
        std::string_view name;
        std::uint32_t    size  = 0;
        std::uint32_t    align = 0;
        void (*addFromBytes)(ComponentManager&, Entity, const void*) = nullptr;
        void (*writeColumn)(ArchetypeChunk*, std::size_t, std::ostream&) = nullptr;
        void (*remapColumn)(ArchetypeChunk*, std::size_t,
                            EntityHandle (*)(EntityHandle, void*), void*) = nullptr;
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

        void AdvanceTick()
        {
            ++mCurrentTick;
            mRemovedQueryable.swap(mRemovedPending);
            mRemovedPending.clear();
        }

        [[nodiscard]] Tick CurrentTick() const { return mCurrentTick; }

        void RecordRemoved(ComponentId componentId, EntityHandle handle)
        {
            mRemovedPending.push_back({componentId, handle});
        }

        [[nodiscard]] std::size_t CountRemoved(ComponentId componentId) const
        {
            std::size_t count = 0;
            for (const auto& entry : mRemovedQueryable)
            {
                if (entry.first == componentId)
                    ++count;
            }
            return count;
        }

        template <typename TFunc>
        void ForEachRemoved(ComponentId componentId, TFunc&& func) const
        {
            for (const auto& entry : mRemovedQueryable)
            {
                if (entry.first == componentId)
                    func(entry.second);
            }
        }

        void BindObserverManager(ObserverManager* observers) { mObserverManager = observers; }

        void BindEntityManager(const skr::Arc<EntityManager>& entityManager)
        {
            mEntityManager = entityManager;
        }

        template <typename T>
            requires IsComponent<T>
        void RegisterComponent()
        {
            const auto componentId = GetComponentId<T>();
            if (mRegisteredComponents.contains(componentId))
                return;

            mRegisteredComponents.insert(componentId);
            RegisterSnapshotCodec<T>();
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

            if (const auto it = mSnapshotCodecs.find(componentId); it != mSnapshotCodecs.end())
            {
                mSnapshotCodecsByName.erase(std::string(it->second.name));
                mSnapshotCodecs.erase(it);
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

        [[nodiscard]] const SnapshotCodec* FindSnapshotCodec(ComponentId componentId) const
        {
            const auto it = mSnapshotCodecs.find(componentId);
            return it == mSnapshotCodecs.end() ? nullptr : &it->second;
        }

        [[nodiscard]] const SnapshotCodec* FindSnapshotCodecByName(std::string_view name) const
        {
            const auto it = mSnapshotCodecsByName.find(std::string(name));
            if (it == mSnapshotCodecsByName.end())
                return nullptr;
            return FindSnapshotCodec(it->second);
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
                    const auto tick = mCurrentTick;
                    CreateOrUpdateEntityIndexWith<Ts...>(
                        entity,
                        [entity, components..., callback = std::move(callback), tick, this](
                            EntityIndex& entityIndex) mutable {
                            auto& [actualArchetype, actualChunk] = entityIndex;
                            actualChunk->ApplyComponents<Ts...>(entity,
                                                                components...,
                                                                std::move(callback));
                            actualChunk->MarkComponentsAdded<Ts...>(entity, tick);
                            if (mObserverManager)
                                (mObserverManager->QueueAdd(GetComponentId<Ts>(), entity), ...);
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

            if (archetype != nullptr)
            {
                const EntityHandle handle =
                    mEntityManager ? mEntityManager->HandleOf(entity)
                                   : EntityHandle {.entity = entity, .generation = 0};
                archetype->ForEachComponent(
                    [&](ComponentId componentId, std::string_view)
                    {
                        RecordRemoved(componentId, handle);
                        if (mObserverManager)
                            mObserverManager->QueueRemove(componentId, handle);
                    });
            }

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

        Entity CloneEntity(const Entity source)
        {
            FREYR_ASSERT(mEntityManager && "EntityManager must be bound for CloneEntity");
            auto& sourceIndex = GetEntityIndex(source);
            if (sourceIndex.archetype == nullptr || sourceIndex.archetypeChunk == nullptr)
                return NullEntity;

            const Entity clone     = mEntityManager->CreateEntity();
            auto*        archetype = sourceIndex.archetype;
            auto*        destChunk = archetype->AddEntity(clone);
            auto&        destIndex = GetEntityIndex(clone);
            destIndex.archetype      = archetype;
            destIndex.archetypeChunk = destChunk;

            sourceIndex.archetypeChunk->CopyEntity(source, clone, destChunk);
            destChunk->MarkAllAdded(clone, mCurrentTick);

            if (mObserverManager)
            {
                archetype->ForEachComponent([&](ComponentId componentId, std::string_view) {
                    mObserverManager->QueueAdd(componentId, clone);
                });
            }

            return clone;
        }

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

        friend class HierarchyManager;

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
            const auto tick = mCurrentTick;
            CreateOrUpdateEntityIndexWith<T>(
                entity,
                [entity, component = std::move(component), tick, this](EntityIndex& entityIndex) mutable {
                    auto& [actualArchetype, actualChunk] = entityIndex;
                    actualChunk->ApplyComponents<T>(entity, component, [](auto, auto&) {});
                    actualChunk->MarkComponentAdded<T>(entity, tick);
                    if (mObserverManager)
                        mObserverManager->QueueAdd(GetComponentId<T>(), entity);
                });
        }

        template <typename T>
            requires IsComponent<T>
        void SetComponentNow(const Entity entity, T component)
        {
            auto& [archetype, chunk] = GetEntityIndex(entity);
            if (archetype == nullptr || !archetype->HasComponent<T>())
            {
                AddComponentNow(entity, std::move(component));
                return;
            }
            chunk->GetComponent<T>(entity) = std::move(component);
            chunk->MarkComponentChanged<T>(entity, mCurrentTick);
        }

        template <typename... Ts>
        void AddComponentsNow(const Entity entity, const Ts&... components)
        {
            const auto tick = mCurrentTick;
            CreateOrUpdateEntityIndexWith<Ts...>(
                entity,
                [entity, components..., tick, this](EntityIndex& entityIndex) {
                    auto& [actualArchetype, actualChunk] = entityIndex;
                    actualChunk->ApplyComponents<Ts...>(entity, components..., [](Entity, Ts&...) {
                    });
                    actualChunk->MarkComponentsAdded<Ts...>(entity, tick);
                    if (mObserverManager)
                        (mObserverManager->QueueAdd(GetComponentId<Ts>(), entity), ...);
                });
        }

        template <typename T>
        void RemoveComponentNow(const Entity entity)
        {
            if (HasComponent<T>(entity))
            {
                const EntityHandle handle =
                    mEntityManager ? mEntityManager->HandleOf(entity)
                                   : EntityHandle {.entity = entity, .generation = 0};
                RecordRemoved(GetComponentId<T>(), handle);
                if (mObserverManager)
                    mObserverManager->QueueRemove(GetComponentId<T>(), handle);
            }
            CreateOrUpdateEntityIndexWith<Remove<T>>(entity, [&](EntityIndex&) {});
        }

        template <typename... Ts>
        void RemoveComponentsNow(const Entity entity)
        {
            if (HasComponents<Ts...>(entity))
            {
                const EntityHandle handle =
                    mEntityManager ? mEntityManager->HandleOf(entity)
                                   : EntityHandle {.entity = entity, .generation = 0};
                (RecordRemoved(GetComponentId<Ts>(), handle), ...);
                if (mObserverManager)
                    (mObserverManager->QueueRemove(GetComponentId<Ts>(), handle), ...);
            }
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

            if constexpr ((!is_remove<std::remove_reference_t<Ts>>::value && ...))
            {
                if (actualArchetype != nullptr && actualArchetype->HasComponents<Ts...>())
                {
                    callback(entityIndex);
                    return;
                }
            }

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

        template <typename T>
            requires IsComponent<T>
        void RegisterSnapshotCodec()
        {
            if constexpr (!std::is_trivially_copyable_v<T>)
                return;

            const auto componentId = GetComponentId<T>();
            if (mSnapshotCodecs.contains(componentId))
                return;

            SnapshotCodec codec {
                .name  = TypeNameOf(TypeIdKind::Component, componentId),
                .size  = static_cast<std::uint32_t>(sizeof(T)),
                .align = static_cast<std::uint32_t>(alignof(T)),
                .addFromBytes =
                    [](ComponentManager& cm, Entity entity, const void* bytes)
                {
                    T value {};
                    std::memcpy(&value, bytes, sizeof(T));
                    cm.AddComponentNow(entity, value);
                },
                .writeColumn =
                    [](ArchetypeChunk* chunk, std::size_t count, std::ostream& out)
                {
                    const auto span = chunk->GetComponentSpan<T>();
                    FREYR_ASSERT(span.size() >= count);
                    out.write(reinterpret_cast<const char*>(span.data()),
                              static_cast<std::streamsize>(count * sizeof(T)));
                },
                .remapColumn =
                    [](ArchetypeChunk* chunk, std::size_t count,
                       EntityHandle (*map)(EntityHandle, void*), void* ctx)
                {
                    if constexpr (!EntityRemapper<T>::kEnabled)
                        return;
                    auto span = chunk->GetComponentSpan<T>();
                    const auto n = std::min(count, span.size());
                    for (std::size_t i = 0; i < n; ++i)
                        EntityRemapper<T>::Remap(span[i], [&](EntityHandle h) { return map(h, ctx); });
                },
            };

            mSnapshotCodecsByName.emplace(std::string(codec.name), componentId);
            mSnapshotCodecs.emplace(componentId, codec);
        }

        friend class Registry;
        friend class Query;
        friend class SnapshotWriter;
        friend class SnapshotReader;

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
        std::unordered_map<ComponentId, SnapshotCodec>                    mSnapshotCodecs;
        std::unordered_map<std::string, ComponentId>                      mSnapshotCodecsByName;
        Tick                                                              mCurrentTick = 1;
        ObserverManager*                                                  mObserverManager = nullptr;
        skr::Arc<EntityManager>                                           mEntityManager;
        std::vector<std::pair<ComponentId, EntityHandle>>                 mRemovedPending;
        std::vector<std::pair<ComponentId, EntityHandle>>                 mRemovedQueryable;
    };
} // namespace FREYR_NAMESPACE
