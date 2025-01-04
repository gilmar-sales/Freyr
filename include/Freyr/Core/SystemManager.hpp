#pragma once

#include <Skirnir.hpp>

#include "Freyr/Base/Entity.hpp"
#include "Freyr/Base/System.hpp"
#include "Freyr/Containers/SparseSet.hpp"

namespace FREYR_NAMESPACE
{

    class SystemManager
    {
      public:
        explicit SystemManager(const std::uint64_t maxSystems) :
            mMaxSystems(maxSystems)
        {
            mSignatures.resize(maxSystems);
            mSystemFactories.resize(maxSystems);
            mRegisteredSystems.resize(maxSystems);
        };

        ~SystemManager() = default;

        template <typename T>
            requires IsSystem<T>
        void RegisterSystem()
        {
            assert(!mRegisteredSystems.contains(GetSystemId<T>()) &&
                   "Registering system more than once.");

            mSystemFactories[GetSystemId<T>()] = [](ServiceProvider& provider) {
                return provider.GetService<T>();
            };

            mRegisteredSystems.insert(GetSystemId<T>());
        }

        template <typename T>
        //    requires IsSystem<T>
        void SetSignature(const Signature& signature)
        {
            assert(mRegisteredSystems.contains(GetSystemId<T>()) &&
                   "System used before registered.");

            mSignatures[GetSystemId<T>()] = signature;
        }

        void PreUpdate(const float                             dt,
                       const std::shared_ptr<ServiceProvider>& serviceProvider)
        {
            for (auto const& id : mRegisteredSystems.getDense())
            {
                GetSystem(id, serviceProvider)->PreUpdate(dt);
            }
        }

        void Update(const float                             dt,
                    const std::shared_ptr<ServiceProvider>& serviceProvider)
        {
            for (auto const& id : mRegisteredSystems.getDense())
            {
                GetSystem(id, serviceProvider)->Update(dt);
            }
        }

        void PostUpdate(const float                             dt,
                        const std::shared_ptr<ServiceProvider>& serviceProvider)
        {
            for (auto const& id : mRegisteredSystems.getDense())
            {
                GetSystem(id, serviceProvider)->PostUpdate(dt);
            }
        }

        void EntityDestroyed(Entity entity)
        {
            for (auto const& id : mRegisteredSystems)
            {
                // mSystems[id]->mEntities.remove(entity);
            }
        }

        void EntitySignatureChanged(Entity           entity,
                                    const Signature& entitySignature)
        {
            for (const auto& id : mRegisteredSystems)
            {
                // auto const& system          = mSystems[id];
                auto const& systemSignature = mSignatures[id];

                if (systemSignature.Match(entitySignature))
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
        [[nodiscard]] std::shared_ptr<System> GetSystem(
            const unsigned long                     systemId,
            const std::shared_ptr<ServiceProvider>& serviceProvider) const
        {
            return std::static_pointer_cast<System>(
                mSystemFactories[systemId](*serviceProvider));
        }

        std::vector<Signature>      mSignatures;
        std::vector<ServiceFactory> mSystemFactories;
        SparseSet<SystemId>         mRegisteredSystems;
        std::uint64_t               mMaxSystems;
    };

} // namespace FREYR_NAMESPACE
