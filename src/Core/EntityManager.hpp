#pragma once

#include "Types/Component.hpp"
#include "Types/Entity.hpp"

namespace FREYR_NAMESPACE
{

    class EntityManager
    {
      public:
        EntityManager(std::uint64_t maxEntities) :
            mMaxEntities(maxEntities), mLivingEntityCount(0)
        {
            mSignatures.resize(maxEntities);

            for (Entity entity = 0; entity < maxEntities; ++entity)
            {
                mAvailableEntities.push(entity);
            }
        }

        Entity CreateEntity()
        {
            assert(mLivingEntityCount < mMaxEntities && "Too many entities in existence.");

            Entity id = mAvailableEntities.front();
            mAvailableEntities.pop();
            ++mLivingEntityCount;

            return id;
        }

        void DestroyEntity(Entity entity)
        {
            assert(entity < mMaxEntities && "Entity out of range.");

            mSignatures[entity].reset();
            mAvailableEntities.push(entity);
            --mLivingEntityCount;
        }

        Signature& GetSignature(Entity entity)
        {
            assert(entity < mLivingEntityCount && "Entity out of range.");

            return mSignatures[entity];
        }

      private:
        std::queue<Entity>     mAvailableEntities;
        std::vector<Signature> mSignatures;
        uint32_t               mLivingEntityCount;
        std::uint64_t          mMaxEntities;
    };

} // namespace FREYR_NAMESPACE
