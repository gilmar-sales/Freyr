#pragma once

#include "Containers/SparseSet.hpp"
#include "System.hpp"
#include "Types/Component.hpp"
#include "Types/Entity.hpp"

namespace FREYR_NAMESPACE
{

    class SystemManager
    {
      public:
        explicit SystemManager(std::uint64_t maxSystems) : mMaxSystems(maxSystems)
        {
            mSignatures.resize(maxSystems);
            mSystems.resize(maxSystems);
            mRegisteredSystems.resize(maxSystems);
        };

        ~SystemManager() = default;

        template<typename T>
            requires IsSystem<T>
        std::shared_ptr<T> RegisterSystem(ECSManager* manager)
        {
            assert(!mRegisteredSystems.contains(GetSystemId<T>()) && "Registering system more than once.");

            auto system                = std::make_shared<T>();
            system->mManager = manager;
            mSystems[GetSystemId<T>()] = system;
            mRegisteredSystems.insert(GetSystemId<T>());

            system->Start();
            return system;
        }

        template<typename T>
        //    requires IsSystem<T>
        void SetSignature(const Signature &signature)
        {
            assert(mRegisteredSystems.contains(GetSystemId<T>()) && "System used before registered.");

            mSignatures[GetSystemId<T>()] = signature;
        }

        void PreUpdate(float dt)
        {
            for (auto const &id : mRegisteredSystems)
            {
                mSystems[id]->PreUpdate(dt);
            }
        }

        void Update(float dt)
        {
            for (auto const &id : mRegisteredSystems)
            {
                mSystems[id]->Update(dt);
            }
        }

        void PostUpdate(float dt)
        {
            for (auto const &id : mRegisteredSystems)
            {
                mSystems[id]->PostUpdate(dt);
            }
        }

        void EntityDestroyed(Entity entity)
        {
            for (auto const &id : mRegisteredSystems)
            {
                // mSystems[id]->mEntities.remove(entity);
            }
        }

        void EntitySignatureChanged(Entity entity, const Signature &entitySignature)
        {
            for (const auto &id : mRegisteredSystems)
            {
                auto const &system          = mSystems[id];
                auto const &systemSignature = mSignatures[id];

                if ((entitySignature & systemSignature) == systemSignature)
                {
                    // system->mEntities.insert(entity);
                }
                else
                {
                    // system->mEntities.remove(entity);
                }
            }
        }

      private:
        std::vector<Signature> mSignatures;
        std::vector<std::shared_ptr<System>> mSystems;
        SparseSet<SystemId> mRegisteredSystems;
        std::uint64_t mMaxSystems;
    };

} // namespace FREYR_NAMESPACE
