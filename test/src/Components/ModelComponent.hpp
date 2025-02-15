#pragma once

#include <Freyr/Freyr.hpp>

struct ModelComponent : fr::Component
{
    unsigned mesh;
    unsigned material;
    unsigned texture;
};