#pragma once

#include "Freyr/Containers/Signature.hpp"

namespace FREYR_NAMESPACE
{
    class TaskManager;
    class ArchetypeChunk;

    class IScheduler
    {
      public:
        virtual ~IScheduler() = default;

        virtual void DispatchChunk(ArchetypeChunk*  chunk,
                                   const Signature& archetypeSignature,
                                   TaskManager*     taskManager) = 0;

        virtual void Flush(TaskManager* taskManager) = 0;
    };
} // namespace FREYR_NAMESPACE