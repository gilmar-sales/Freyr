#pragma once

#include "Freyr/Base/Entity.hpp"

namespace FREYR_NAMESPACE
{

    class EntityManager
    {
      public:
        EntityManager(std::uint64_t maxEntities) :
            mMaxEntities(maxEntities), mLivingEntityCount(0)
        {
            for (Entity entity = 0; entity < maxEntities; ++entity)
            {
                mAvailableEntities.push(entity);
            }
        }

        Entity CreateEntity()
        {
            assert(mLivingEntityCount < mMaxEntities &&
                   "Too many entities in existence.");

            Entity id = mAvailableEntities.front();
            mAvailableEntities.pop();
            ++mLivingEntityCount;

            return id;
        }

        void DestroyEntity(Entity entity)
        {
            assert(entity < mMaxEntities && "Entity out of range.");

            mAvailableEntities.push(entity);
            --mLivingEntityCount;
        }

      private:
        std::queue<Entity> mAvailableEntities;
        uint32_t           mLivingEntityCount;
        std::uint64_t      mMaxEntities;
    };

} // namespace FREYR_NAMESPACE
