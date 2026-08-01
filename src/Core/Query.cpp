#include "Freyr/Core/Query.hpp"

namespace FREYR_NAMESPACE
{
    Query::Query(const skr::Arc<ComponentManager>& componentManager) : mComponentManager(componentManager)
    {
    }

    Query::~Query() = default;

} // namespace FREYR_NAMESPACE