#pragma once

#include "Freyr/Base/Entity.hpp"
#include "Freyr/Containers/MPMCQueue.hpp"
#include "Freyr/Core/Assertions.hpp"
#include "Freyr/Core/FreyrOptions.hpp"

#include <Skirnir/Common.hpp>

#include <atomic>
#include <memory>
#include <optional>

namespace FREYR_NAMESPACE
{

    class EntityManager
    {
      public:
        explicit EntityManager(const skr::Arc<FreyrOptions>& freyrOptions) :
            mAvailableEntities(freyrOptions->MaxEntities), mLivingEntityCount(0),
            mMaxEntities(freyrOptions->MaxEntities),
            mGenerations(std::make_unique<std::atomic<Generation>[]>(freyrOptions->MaxEntities)),
            mAlive(std::make_unique<std::atomic<std::uint8_t>[]>(freyrOptions->MaxEntities))
        {
            for (std::uint64_t i = 0; i < mMaxEntities; ++i)
            {
                mGenerations[i].store(0, std::memory_order_relaxed);
                mAlive[i].store(0, std::memory_order_relaxed);
            }
        }

        Entity CreateEntity()
        {
            Entity entity;
            if (mAvailableEntities.try_pop(entity))
            {
                FREYR_ASSERT(entity < mMaxEntities && "Recycled entity out of range.");
                mAlive[entity].store(1, std::memory_order_release);
                return entity;
            }

            FREYR_ASSERT(mLivingEntityCount < mMaxEntities && "Too many entities in existence.");

            entity = mLivingEntityCount++;
            mAlive[entity].store(1, std::memory_order_release);
            return entity;
        }

        void DestroyEntity(Entity entity)
        {
            FREYR_ASSERT(entity < mMaxEntities && "Entity out of range.");
            FREYR_ASSERT(mAlive[entity].load(std::memory_order_acquire) != 0 &&
                         "Destroying entity that is not alive.");

            mAlive[entity].store(0, std::memory_order_release);
            mGenerations[entity].fetch_add(1, std::memory_order_acq_rel);

            const bool pushed = mAvailableEntities.try_push(entity);
            FREYR_ASSERT(pushed && "Entity free-list is full.");
            (void) pushed;
        }

        [[nodiscard]] Entity LivingEntities() const { return mLivingEntityCount.load(); }

        [[nodiscard]] std::uint64_t MaxEntities() const { return mMaxEntities; }

        [[nodiscard]] bool IsAlive(Entity entity) const
        {
            if (entity == NullEntity || entity >= mMaxEntities)
                return false;
            return mAlive[entity].load(std::memory_order_acquire) != 0;
        }

        [[nodiscard]] bool IsAlive(EntityHandle handle) const
        {
            if (handle.entity == NullEntity || handle.entity >= mMaxEntities)
                return false;
            if (mAlive[handle.entity].load(std::memory_order_acquire) == 0)
                return false;
            return mGenerations[handle.entity].load(std::memory_order_acquire) == handle.generation;
        }

        [[nodiscard]] Generation GetGeneration(Entity entity) const
        {
            if (entity == NullEntity || entity >= mMaxEntities)
                return 0;
            return mGenerations[entity].load(std::memory_order_acquire);
        }

        [[nodiscard]] EntityHandle HandleOf(Entity entity) const
        {
            if (!IsAlive(entity))
                return NullHandle;
            return EntityHandle {.entity = entity, .generation = GetGeneration(entity)};
        }

        [[nodiscard]] std::optional<Entity> Resolve(EntityHandle handle) const
        {
            if (!IsAlive(handle))
                return std::nullopt;
            return handle.entity;
        }

      private:
        rigtorp::MPMCQueue<Entity>                       mAvailableEntities;
        std::atomic<Entity>                              mLivingEntityCount;
        std::uint64_t                                    mMaxEntities;
        std::unique_ptr<std::atomic<Generation>[]>       mGenerations;
        std::unique_ptr<std::atomic<std::uint8_t>[]>     mAlive;
    };

} // namespace FREYR_NAMESPACE
