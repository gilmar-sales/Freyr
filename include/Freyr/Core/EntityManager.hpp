#pragma once

#include "Freyr/Base/Entity.hpp"
#include "Freyr/Containers/MPMCQueue.hpp"

namespace FREYR_NAMESPACE
{

    class EntityManager
    {
      public:
        explicit EntityManager(const skr::Arc<FreyrOptions>& freyrOptions) :
            mAvailableEntities(freyrOptions->MaxEntities), mLivingEntityCount(0),
            mMaxEntities(freyrOptions->MaxEntities)
        {
        }

        Entity CreateEntity()
        {
            if (Entity entity; mAvailableEntities.try_pop(entity))
            {
                return entity;
            }

            FREYR_ASSERT(mLivingEntityCount < mMaxEntities && "Too many entities in existence.");

            return mLivingEntityCount++;
        }

        void DestroyEntity(Entity entity)
        {
            FREYR_ASSERT(entity < mMaxEntities && "Entity out of range.");

            const bool pushed = mAvailableEntities.try_push(entity);
            FREYR_ASSERT(pushed && "Entity free-list is full.");
            (void)pushed;
        }

        [[nodiscard]] Entity LivingEntities() const { return mLivingEntityCount.load(); }

      private:
        rigtorp::MPMCQueue<Entity> mAvailableEntities;
        std::atomic<Entity>        mLivingEntityCount;
        std::uint64_t              mMaxEntities;
    };

} // namespace FREYR_NAMESPACE
