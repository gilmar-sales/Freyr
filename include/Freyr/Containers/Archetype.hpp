#pragma once

#include "Freyr/Containers/ComponentArray.hpp"
#include "Freyr/Containers/Signature.hpp"

namespace FREYR_NAMESPACE
{

    class Archetype
    {
      public:
        explicit Archetype(const Entity maxEntities) :
            internalName("Archetype: "), mMaxEntities(maxEntities)
        {
            mRegisteredEntities.resize(maxEntities);
            mRegisteredComponents.resize(512);
            mComponentArrays.resize(512);
        }

        Archetype(const Archetype& other) :
            mRegisteredComponents(other.mRegisteredComponents),
            mSignature(other.mSignature), mMaxEntities(other.mMaxEntities),
            internalName(other.internalName)
        {
            mRegisteredEntities.resize(other.mMaxEntities);
            mComponentArrays.resize(512);

            for (auto component : mRegisteredComponents)
            {
                mComponentArrays[mRegisteredComponents.getIndex(component)] =
                    other
                        .mComponentArrays[mRegisteredComponents.getIndex(
                            component)]
                        ->Clone();
            }
        }

        ~Archetype()
        {
            for (const auto& component : mRegisteredComponents)
            {
                delete (mComponentArrays[mRegisteredComponents.getIndex(
                    component)]);
            }
        }

        void StartTracing()
        {
            FREYR_PROFILING_BEGIN("FREYR",
                                  internalName.c_str(),
                                  perfetto::Track((uint64_t) this));
        }

        void EndTracing()
        {
            FREYR_PROFILING_END("FREYR", perfetto::Track((uint64_t) this));
        }

        void AddEntity(const Entity entity)
        {
            mRegisteredEntities.insert(entity);
            for (auto const& component : mRegisteredComponents)
            {
                mComponentArrays[mRegisteredComponents.getIndex(component)]
                    ->AddEntity(entity);
            }
        }

        void CopyEntity(Entity from, Entity to);

        template <typename T>
        void RegisterComponent()
        {
            assert(!mRegisteredComponents.contains(GetComponentId<T>()) &&
                   "Registering component type more than once.");

            mSignature.AddComponent<T>();

            mRegisteredComponents.insert(GetComponentId<T>());
            mComponentArrays[mRegisteredComponents.getIndex(
                GetComponentId<T>())] = new ComponentArray<T>(mMaxEntities);

            if (internalName.size() > 12)
            {
                internalName += ", ";
            }

            internalName += typeid(T).name();
        }

        template <typename T>
        void AddComponent(const Entity& entity, T component)
        {
            if (mRegisteredEntities.size() == mMaxEntities - 10)
            {
                Resize(static_cast<size_t>(mMaxEntities * 1.25));
            }

            mRegisteredEntities.insert(entity);

            GetComponentArray<T>()->InsertData(entity, component);
        }

        template <typename T>
        void RemoveComponent(const Entity& entity)
        {
            GetComponentArray<T>()->RemoveData(entity);
        }

        template <typename T>
        [[nodiscard]] bool HasComponent() const
        {
            return mRegisteredComponents.contains(GetComponentId<T>());
        }

        template <typename T>
        T& GetComponent(const Entity& entity)
        {
            return GetComponentArray<T>()->GetData(entity);
        }

        void RemoveEntity(const Entity& entity)
        {
            for (auto const& component : mRegisteredComponents)
            {
                mComponentArrays[mRegisteredComponents.getIndex(component)]
                    ->RemoveEntity(entity);
            }

            mRegisteredEntities.remove(entity);
        }

        void Resize(size_t size)
        {
            mMaxEntities = size;
            mRegisteredEntities.resize(size);
            for (auto const& component : mRegisteredComponents)
            {
                mComponentArrays[mRegisteredComponents.getIndex(component)]
                    ->Resize(size);
            }
        }

        const SparseSet<Entity>& GetRegisteredEntities()
        {
            return mRegisteredEntities;
        }

        [[nodiscard]] const Signature& GetSignature() const
        {
            return mSignature;
        }

        template <typename... Components>
        void ForEach(std::string_view label, auto&& f)
        {
            std::scoped_lock lock(mMutexes[GetComponentId<Components>()]...);

            FREYR_PROFILING_BEGIN("FREYR",
                                  label.data(),
                                  perfetto::Track((uint64_t) this),
                                  "entity_count",
                                  mRegisteredEntities.size());
            for (const auto& entity : mRegisteredEntities)
            {
                f(entity, GetComponentArray<Components>()->GetData(entity)...);
            }
            FREYR_PROFILING_END("FREYR", perfetto::Track((uint64_t) this));
        }

        template <typename... Components>
        void ForEachAsync(std::string_view label, auto&& f)
        {
            std::scoped_lock lock(mMutexes[GetComponentId<Components>()]...);

            FREYR_PROFILING_BEGIN("FREYR",
                                  label.data(),
                                  perfetto::Track((uint64_t) this),
                                  "entity_count",
                                  mRegisteredEntities.size());
            std::for_each(
                std::execution::par,
                mRegisteredEntities.begin(),
                mRegisteredEntities.end(),
                [&](const auto& entity) {
                    f(entity,
                      GetComponentArray<Components>()->GetData(entity)...);
                });
            FREYR_PROFILING_END("FREYR", perfetto::Track((uint64_t) this));
        }

        template <typename... Components>
        void ForEachParallel(std::string_view label, auto&& f, Entity index)
        {
            std::scoped_lock lock(mMutexes[GetComponentId<Components>()]...);

            FREYR_PROFILING_BEGIN("FREYR",
                                  label.data(),
                                  perfetto::Track((uint64_t) this),
                                  "entity_count",
                                  mRegisteredEntities.size());
            std::for_each(
                std::execution::par,
                mRegisteredEntities.begin(),
                mRegisteredEntities.end(),
                [&](const auto& entity) {
                    f(entity,
                      index + mRegisteredEntities.getIndex(entity),
                      GetComponentArray<Components>()->GetData(entity)...);
                });
            FREYR_PROFILING_END("FREYR", perfetto::Track((uint64_t) this));
        }

        template <typename... Components>
        void Map(auto&&                                             f,
                 Entity                                             index,
                 std::vector<decltype(f(*(new Entity {}),
                                        *(new Components {})...))>& buffer)
        {
            std::scoped_lock lock(mMutexes[GetComponentId<Components>()]...);

            std::for_each(
                std::execution::par,
                mRegisteredEntities.begin(),
                mRegisteredEntities.end(),
                [&](const auto& entity) {
                    buffer[index + mRegisteredEntities.getIndex(entity)] =
                        f(entity,
                          GetComponentArray<Components>()->GetData(entity)...);
                });
        }

        template <typename... Components>
        void ForEachParallel(std::string_view   label,
                             SparseSet<Entity>& entities,
                             auto&&             f)
        {
            std::scoped_lock lock(mMutexes[GetComponentId<Components>()]...);

            FREYR_PROFILING_BEGIN("FREYR",
                                  label.data(),
                                  perfetto::Track((uint64_t) this),
                                  "entity_count",
                                  entities.size());
            std::for_each(
                std::execution::par,
                entities.begin(),
                entities.end(),
                [&](const auto& entity) {
                    if (!mRegisteredEntities.contains(entity))
                        return;
                    f(entity,
                      GetComponentArray<Components>()->GetData(entity)...);

                    entities.remove(entity);
                });
            FREYR_PROFILING_END("FREYR", perfetto::Track((uint64_t) this));
        }

        std::mutex& Mutex() { return mMutex; }

        std::size_t Count() { return mRegisteredEntities.size(); }

        void Swap(const Entity& a, const Entity& b)
        {
            mRegisteredEntities.swap(a, b);

            for (auto component : mRegisteredComponents)
            {
                mComponentArrays[mRegisteredComponents.getIndex(component)]
                    ->Swap(a, b);
            }
        }

      protected:
        friend class ComponentManager;
        friend class Scene;
        void MoveData(const Entity&                     entity,
                      const std::shared_ptr<Archetype>& other)
        {
            for (const auto component : mRegisteredComponents)
            {
                mComponentArrays[mRegisteredComponents.getIndex(component)]
                    ->MoveData(
                        entity,
                        other->mComponentArrays[other->mRegisteredComponents
                                                    .getIndex(component)]);
            }

            other->mRegisteredEntities.insert(entity);
            RemoveEntity(entity);
        };

        void MoveData(const std::shared_ptr<Archetype>& other)
        {
            for (const auto component : mRegisteredComponents)
            {
                mComponentArrays[mRegisteredComponents.getIndex(component)]
                    ->MoveData(
                        other->mComponentArrays[other->mRegisteredComponents
                                                    .getIndex(component)]);
            }

            for (auto entity : mRegisteredEntities)
            {
                other->mRegisteredEntities.insert(entity);
            }
        };

        template <typename T>
        ComponentArray<T>* GetComponentArray()
        {
            assert(mRegisteredComponents.contains(GetComponentId<T>()) &&
                   "Component not registered before use.");

            return static_cast<ComponentArray<T>*>(
                mComponentArrays[mRegisteredComponents.getIndex(
                    GetComponentId<T>())]);
        }

      private:
        std::string internalName;

        std::mutex mMutex;
        Signature  mSignature;
        std::mutex mMutexes[512];

        std::vector<IComponentArray*> mComponentArrays;
        SparseSet<ComponentId>        mRegisteredComponents;
        SparseSet<Entity>             mRegisteredEntities;
        Entity                        mMaxEntities;
    };
    inline void Archetype::CopyEntity(const Entity from, const Entity to)
    {
        for (auto const& component : mRegisteredComponents)
        {
            mComponentArrays[mRegisteredComponents.getIndex(component)]
                ->CopyEntity(from, to);
        }
    }

} // namespace FREYR_NAMESPACE
