#pragma once

#include <Freyr/Base/Component.hpp>

struct ModelComponent : fr::Component
{
    unsigned mesh;
    unsigned material;
    unsigned texture;
};
