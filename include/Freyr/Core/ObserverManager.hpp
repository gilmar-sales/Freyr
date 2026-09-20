#pragma once

#include "Freyr/Base/Component.hpp"
#include "Freyr/Base/Entity.hpp"

#include <functional>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace FREYR_NAMESPACE
{
    class ObserverManager
    {
      public:
        template <typename T>
            requires IsComponent<T>
        void ObserveAdd(std::function<void(Entity)> callback)
        {
            std::lock_guard lock(mMutex);
            mAddObservers[GetComponentId<T>()].push_back(std::move(callback));
        }

        template <typename T>
            requires IsComponent<T>
        void ObserveRemove(std::function<void(EntityHandle)> callback)
        {
            std::lock_guard lock(mMutex);
            mRemoveObservers[GetComponentId<T>()].push_back(std::move(callback));
        }

        void QueueAdd(ComponentId componentId, Entity entity)
        {
            std::lock_guard lock(mMutex);
            mPendingAdds.push_back({componentId, entity});
        }

        void QueueRemove(ComponentId componentId, EntityHandle handle)
        {
            std::lock_guard lock(mMutex);
            mPendingRemoves.push_back({componentId, handle});
        }

        void Flush()
        {
            std::vector<std::pair<ComponentId, Entity>>       adds;
            std::vector<std::pair<ComponentId, EntityHandle>> removes;
            {
                std::lock_guard lock(mMutex);
                adds.swap(mPendingAdds);
                removes.swap(mPendingRemoves);
            }

            for (const auto& [componentId, entity] : adds)
            {
                const auto it = mAddObservers.find(componentId);
                if (it == mAddObservers.end())
                    continue;
                for (auto& cb : it->second)
                    cb(entity);
            }

            for (const auto& [componentId, handle] : removes)
            {
                const auto it = mRemoveObservers.find(componentId);
                if (it == mRemoveObservers.end())
                    continue;
                for (auto& cb : it->second)
                    cb(handle);
            }
        }

      private:
        std::mutex mMutex;
        std::unordered_map<ComponentId, std::vector<std::function<void(Entity)>>> mAddObservers;
        std::unordered_map<ComponentId, std::vector<std::function<void(EntityHandle)>>>
                                                              mRemoveObservers;
        std::vector<std::pair<ComponentId, Entity>>           mPendingAdds;
        std::vector<std::pair<ComponentId, EntityHandle>>     mPendingRemoves;
    };
} // namespace FREYR_NAMESPACE
