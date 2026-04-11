#pragma once

#include <memory>
#include <utility>
#include <vector>

#include "Freyr/Containers/ArchetypeChunk.hpp"
#include "Freyr/Containers/Signature.hpp"
#include "Freyr/Core/IScheduler.hpp"
#include "Freyr/Core/TaskManager.hpp"

namespace FREYR_NAMESPACE
{
    class Scheduler final : public IScheduler
    {
        using Action     = std::function<void(ArchetypeChunk*)>;
        using ActionList = std::vector<std::shared_ptr<Action>>;

      public:
        template <typename... Ts>
            requires(IsComponent<Ts> && ...)
        void Run(const char* label, auto&& action)
        {
            auto signature = MakeSignature<Ts...>();
            auto task      = std::make_shared<Action>([label, action = std::forward<decltype(action)>(action)](
                                                     ArchetypeChunk* chunk) { chunk->ForEach<Ts...>(label, action); });
            mActionsMap[signature].push_back(task);
        }

        void DispatchChunk(ArchetypeChunk*  chunk,
                           const Signature& archetypeSignature,
                           TaskManager*     taskManager) override
        {
            std::vector<std::shared_ptr<Action>> actionPtrs;
            for (const auto& [signature, actions] : mActionsMap)
            {
                if (signature.Match(archetypeSignature))
                {
                    for (const auto& action : actions)
                    {
                        actionPtrs.push_back(action);
                    }
                }
            }

            fr::function<void()> task = [actionPtrs = std::move(actionPtrs), chunk]() {
                for (const auto action : actionPtrs)
                {
                    (*action)(chunk);
                }
            };
            taskManager->AddTask(std::move(task));
        }

        void Flush(TaskManager* taskManager) override { mActionsMap.clear(); }

      private:
        std::unordered_map<Signature, ActionList, SignatureHash> mActionsMap;
    };
} // namespace FREYR_NAMESPACE
