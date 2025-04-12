#pragma once

#include "Freyr/Base/Entity.hpp"
#include "Freyr/Containers/MPMCQueue.hpp"

namespace FREYR_NAMESPACE
{

    class EntityManager
    {
      public:
        explicit EntityManager(const Ref<FreyrOptions>& freyrOptions) :
            mAvailableEntities(freyrOptions->InitialCapacity),
            mLivingEntityCount(0), mMaxEntities(freyrOptions->InitialCapacity)
        {
        }

        Entity CreateEntity()
        {
            assert(mLivingEntityCount <= mMaxEntities &&
                   "Too many entities in existence.");

            {
                if (!mAvailableEntities.empty())
                {
                    Entity entity;

                    while (!mAvailableEntities.try_pop(entity))
                    {
                    }

                    return entity;
                }
            }

            return mLivingEntityCount++;
        }

        void DestroyEntity(Entity entity)
        {
            assert(entity < mMaxEntities && "Entity out of range.");

            mAvailableEntities.try_push(entity);
        }

      private:
        rigtorp::MPMCQueue<Entity> mAvailableEntities;
        std::atomic<Entity>        mLivingEntityCount;
        std::uint64_t              mMaxEntities;
    };

} // namespace FREYR_NAMESPACE
