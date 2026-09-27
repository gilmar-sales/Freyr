#pragma once

#include <Freyr/Base/System.hpp>

#include <vector>

inline std::vector<int>* gScheduleOrder = nullptr;

class ScheduleOrderA : public fr::System
{
  public:
    explicit ScheduleOrderA(const skr::Arc<fr::Registry> registry) : System(registry) {}

    void Update(float) override
    {
        if (gScheduleOrder)
            gScheduleOrder->push_back(1);
        ++UpdateCount;
    }

    int UpdateCount = 0;
};

class ScheduleOrderB : public fr::System
{
  public:
    explicit ScheduleOrderB(const skr::Arc<fr::Registry> registry) : System(registry) {}

    void Update(float) override
    {
        if (gScheduleOrder)
            gScheduleOrder->push_back(2);
        ++UpdateCount;
    }

    int UpdateCount = 0;
};

class GatedSystem : public fr::System
{
  public:
    explicit GatedSystem(const skr::Arc<fr::Registry> registry) : System(registry) {}

    void Update(float) override { ++UpdateCount; }

    int UpdateCount = 0;
};
