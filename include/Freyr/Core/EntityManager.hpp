#pragma once

#include "Freyr/Base/Entity.hpp"

#include <shared_mutex>

namespace FREYR_NAMESPACE
{

    class EntityManager
    {
      public:
        explicit EntityManager(const std::uint64_t maxEntities) :
            mLivingEntityCount(0), mMaxEntities(maxEntities)
        {
        }

        Entity CreateEntity()
        {
            assert(mLivingEntityCount <= mMaxEntities &&
                   "Too many entities in existence.");

            {
                std::shared_lock readLock(mSharedMutex);

                if (!mAvailableEntities.empty())
                {
                    std::unique_lock writeLock(mSharedMutex);

                    const Entity entity = mAvailableEntities.front();
                    mAvailableEntities.pop();

                    return entity;
                }
            }

            return mLivingEntityCount++;
        }

        void DestroyEntity(Entity entity)
        {
            assert(entity < mMaxEntities && "Entity out of range.");

            std::unique_lock writeLock(mSharedMutex);
            mAvailableEntities.push(entity);
        }

      private:
        std::shared_mutex   mSharedMutex;
        std::queue<Entity>  mAvailableEntities;
        std::atomic<Entity> mLivingEntityCount;
        std::uint64_t       mMaxEntities;
    };

} // namespace FREYR_NAMESPACE
