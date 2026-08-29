#include "Freyr/Base/TypeNameId.hpp"
#include "Freyr/Core/Assertions.hpp"

#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>

namespace FREYR_NAMESPACE
{
    namespace
    {
        struct TypeNameRegistry
        {
            std::mutex                                     mutex;
            std::unordered_map<std::string, std::uint64_t> nameToId;
            std::deque<std::string>                        idToName;
            std::uint64_t                                  nextId = 0;
        };

        TypeNameRegistry& RegistryFor(const TypeIdKind kind)
        {
            static TypeNameRegistry registries[3];
            return registries[static_cast<std::uint8_t>(kind)];
        }
    } // namespace

    std::uint64_t RegisterTypeName(const TypeIdKind kind, const std::string_view name)
    {
        FREYR_ASSERT(!name.empty() && "Type name used for id registration must not be empty.");

        auto&           registry = RegistryFor(kind);
        std::lock_guard lock(registry.mutex);

        const auto key = std::string(name);
        if (const auto it = registry.nameToId.find(key); it != registry.nameToId.end())
        {
            return it->second;
        }

        const auto id = registry.nextId++;
        registry.nameToId.emplace(key, id);
        registry.idToName.emplace_back(key);
        return id;
    }

    std::uint64_t TypeNameCount(const TypeIdKind kind)
    {
        auto&           registry = RegistryFor(kind);
        std::lock_guard lock(registry.mutex);
        return registry.nextId;
    }

    std::string_view TypeNameOf(const TypeIdKind kind, const std::uint64_t id)
    {
        auto&           registry = RegistryFor(kind);
        std::lock_guard lock(registry.mutex);
        if (id >= registry.idToName.size())
            return {};
        return registry.idToName[static_cast<size_t>(id)];
    }
} // namespace FREYR_NAMESPACE
