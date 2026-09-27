#pragma once

#include "Freyr/Base/System.hpp"
#include "Freyr/Core/ComponentManager.hpp"
#include "Freyr/Core/FreyrOptions.hpp"
#include "Freyr/Core/Registry.hpp"
#include "Freyr/Core/ThreadPool.hpp"
#include "Freyr/Hierarchy/HierarchyManager.hpp"
#include "Freyr/Hierarchy/HierarchyPropagation.hpp"
#include "Freyr/Hierarchy/HierarchyPropagationPolicy.hpp"

#include <vector>

namespace FREYR_NAMESPACE
{
    template <HierarchyPropagationPolicy Policy>
    class HierarchyPropagationSystem : public System
    {
      public:
        HierarchyPropagationSystem(const skr::Arc<Registry>&         registry,
                                   const skr::Arc<HierarchyManager>& hierarchy,
                                   const skr::Arc<ComponentManager>& components,
                                   const skr::Arc<ThreadPool>&       threadPool,
                                   const skr::Arc<FreyrOptions>&     options) :
            System(registry), mHierarchy(hierarchy), mComponents(components),
            mThreadPool(threadPool), mOptions(options)
        {
        }

        void PostUpdate(float) override
        {
            using Local = typename Policy::Local;
            using World = typename Policy::World;

            if (mHierarchy->HasAnyDirty())
            {
                PropagateDirty(*mHierarchy, *mComponents, *mThreadPool, mOptions->ThreadCount,
                               mPolicy, mHeads, mParents, mHierarchy->GetPropagationMode());
                return;
            }

            mRegistry->CreateMutation()->Each([this](Entity entity, Local&, World&) {
                if (mHierarchy->GetParent(entity) == NullEntity)
                    mPolicy.OnRoot(*mComponents, entity);
            });

            PropagateForest(*mHierarchy, *mComponents, *mThreadPool, mOptions->ThreadCount, mPolicy,
                            mHierarchy->RootsWithChildren(), mHierarchy->GetPropagationMode());
        }

      private:
        skr::Arc<HierarchyManager> mHierarchy;
        skr::Arc<ComponentManager> mComponents;
        skr::Arc<ThreadPool>       mThreadPool;
        skr::Arc<FreyrOptions>     mOptions;
        Policy                     mPolicy {};
        std::vector<Entity>        mHeads;
        std::vector<Entity>        mParents;
    };
} // namespace FREYR_NAMESPACE
