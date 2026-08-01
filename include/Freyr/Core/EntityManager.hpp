#pragma once

#include "Freyr/Base/Entity.hpp"
#include "Freyr/Containers/MPMCQueue.hpp"

namespace FREYR_NAMESPACE
{

    class EntityManager
    {
      public:
        explicit EntityManager(const skr::Arc<FreyrOptions>& freyrOptions) :
            mAvailableEntities(static_cast<size_t>(static_cast<double>(freyrOptions->MaxEntities) * 0.8)),
            mLivingEntityCount(0), mMaxEntities(freyrOptions->MaxEntities)
        {
        }

        Entity CreateEntity()
        {
            FREYR_ASSERT(mLivingEntityCount <= mMaxEntities && "Too many entities in existence.");

            if (Entity entity; mAvailableEntities.try_pop(entity))
            {
                return entity;
            }

            return mLivingEntityCount++;
        }

        void DestroyEntity(Entity entity)
        {
            FREYR_ASSERT(entity < mMaxEntities && "Entity out of range.");

            mAvailableEntities.try_push(entity);
        }

        [[nodiscard]] Entity LivingEntities() const { return mLivingEntityCount.load(); }

      private:
        rigtorp::MPMCQueue<Entity> mAvailableEntities;
        std::atomic<Entity>        mLivingEntityCount;
        std::uint64_t              mMaxEntities;
    };

} // namespace FREYR_NAMESPACE
