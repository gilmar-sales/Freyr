#pragma once

#include "Freyr/Base/TypeNameId.hpp"
#include "Freyr/Core/Assertions.hpp"

#include <Skirnir/Common/Reflection.hpp>

#include <any>
#include <optional>
#include <string>
#include <unordered_map>

namespace FREYR_NAMESPACE
{
    using ResourceId = std::uint64_t;

    template <typename T>
    [[nodiscard]] inline ResourceId GetResourceId()
    {
        static const auto id = RegisterTypeName(TypeIdKind::Resource, refl::type_name<T>());
        return id;
    }

    class ResourceManager
    {
      public:
        template <typename T>
        void Insert(T value)
        {
            const auto id = GetResourceId<T>();
            mResources.insert_or_assign(id, std::make_any<T>(std::move(value)));
        }

        template <typename T>
        [[nodiscard]] bool Has() const
        {
            return mResources.contains(GetResourceId<T>());
        }

        template <typename T>
        [[nodiscard]] T& Get()
        {
            const auto id = GetResourceId<T>();
            const auto it = mResources.find(id);
            FREYR_ASSERT(it != mResources.end() && "Resource not inserted");
            return std::any_cast<T&>(it->second);
        }

        template <typename T>
        [[nodiscard]] const T& Get() const
        {
            const auto id = GetResourceId<T>();
            const auto it = mResources.find(id);
            FREYR_ASSERT(it != mResources.end() && "Resource not inserted");
            return std::any_cast<const T&>(it->second);
        }

        template <typename T>
        [[nodiscard]] std::optional<std::reference_wrapper<T>> TryGet()
        {
            const auto id = GetResourceId<T>();
            const auto it = mResources.find(id);
            if (it == mResources.end())
                return std::nullopt;
            return std::any_cast<T&>(it->second);
        }

        template <typename T>
        bool Remove()
        {
            return mResources.erase(GetResourceId<T>()) > 0;
        }

      private:
        std::unordered_map<ResourceId, std::any> mResources;
    };
} // namespace FREYR_NAMESPACE
