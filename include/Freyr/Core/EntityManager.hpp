#pragma once

#include "Freyr/Base/Entity.hpp"
#include "Freyr/Containers/MPMCQueue.hpp"

namespace FREYR_NAMESPACE
{

    class EntityManager
    {
      public:
        explicit EntityManager(const Ref<FreyrOptions>& freyrOptions) :
            mAvailableEntities(1024 * 1024), mLivingEntityCount(0),
            mMaxEntities(freyrOptions->MaxEntities)
        {
        }

        Entity CreateEntity()
        {
            FREYR_ASSERT(mLivingEntityCount <= mMaxEntities &&
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
            FREYR_ASSERT(entity < mMaxEntities && "Entity out of range.");

            mAvailableEntities.try_push(entity);
        }

        [[nodiscard]] Entity LivingEntities() const
        {
            return mLivingEntityCount.load();
        }

      private:
        rigtorp::MPMCQueue<Entity> mAvailableEntities;
        std::atomic<Entity>        mLivingEntityCount;
        std::uint64_t              mMaxEntities;
    };

} // namespace FREYR_NAMESPACE
