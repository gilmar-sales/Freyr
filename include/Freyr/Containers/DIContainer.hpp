#pragma once

#include "Freyr/Base/Dependency.hpp"
#include "Freyr/Meta/Reflection.hpp"

namespace FREYR_NAMESPACE
{
    class DIContainer
    {
      public:
        DIContainer()  = default;
        ~DIContainer() = default;

        template <typename Concrete>
            requires(std::tuple_size_v<refl::as_tuple<Concrete>> > 0) 
        void AddSingleton()
        {
            using ConstructorArgs = refl::as_tuple<Concrete>;

            AddSingletonImpl<Concrete>(ConstructorArgs {});
        }

        template <typename Concrete>
        void AddSingleton()
        {
            using ConstructorArgs = refl::as_tuple<Concrete>;

            AddSingleton<Concrete>(std::make_shared<Concrete>());
        }

        template <typename Concrete>
        void AddSingleton(std::shared_ptr<Concrete> element)
        {
            assert(!Contains<Concrete>() && "Can't register twice");

            mSingletonContainer.insert({ GetDependencyId<Concrete>(), element });
        }

        template <typename Concrete, typename Arg, typename... Rest>
            requires(std::is_constructible_v<Concrete, Arg, Rest...>)
        void AddSingleton(Arg arg, Rest... rest)
        {
            AddSingleton<Concrete>(std::make_shared<Concrete>(arg, rest...));
        }

        template <typename Interface, typename Concrete, typename Arg, typename... Rest>
            requires(not std::is_same_v<Interface, Concrete> and std::is_base_of_v<Interface, Concrete> and std::is_constructible_v<Concrete, Arg, Rest...>)
        void AddSingleton(Arg arg, Rest... rest)
        {
            AddSingleton<Interface, Concrete>(std::make_shared<Concrete>(arg, rest...));
        }

        template <typename Interface, typename Concrete>
            requires(not std::is_same_v<Interface, Concrete> and std::is_base_of_v<Interface, Concrete>)
        void AddSingleton(std::shared_ptr<Concrete> element)
        {
            assert(!mSingletonContainer.contains(GetDependencyId<Interface>()) && "Can't register twice");

            mSingletonContainer.insert({ GetDependencyId<Interface>(), element });
        }

        template <typename Concrete>
        void AddTransient(std::function<std::shared_ptr<void>(DIContainer&)> f)
        {
            assert(!mTransientContainer.contains(GetDependencyId<Concrete>()) && "Can't register twice");

            mTransientContainer.insert({ GetDependencyId<Concrete>(), f });
        }

        template <typename Interface, typename Concrete>
            requires(std::is_base_of_v<Interface, Concrete>)
        void AddTransient(std::function<std::shared_ptr<void>(DIContainer&)> f)
        {
            assert(!mTransientContainer.contains(GetDependencyId<Interface>()) && "Can't register twice");

            mTransientContainer.insert({ GetDependencyId<Interface>(), f });
        }

        template <typename Concrete>
            requires(std::tuple_size_v<refl::as_tuple<Concrete>> > 0) 
        void AddTransient()
        {
            using ConstructorArgs = refl::as_tuple<Concrete>;

            AddTransientImpl<Concrete>(ConstructorArgs {});
        }

        template <typename Concrete>
        void AddTransient()
        {

            AddTransient<Concrete>(std::make_shared<Concrete>());
        }

        template <typename Interface, typename Concrete, typename Arg, typename... Rest>
            requires(not std::is_same_v<Interface, Concrete> and std::is_base_of_v<Interface, Concrete> and std::is_constructible_v<Concrete, Arg, Rest...>)
        void AddTransient(Arg arg, Rest... rest)
        {
            AddTransient<Interface, Concrete>([=](DIContainer& dependencyContainer) {
                return std::make_shared<Concrete>(arg, rest...);
            });
        }

        template <typename Concrete>
        std::shared_ptr<Concrete> GetService()
        {
            assert(Contains<Concrete>() && "Unable to get unregistered type");

            if (mTransientContainer.contains(GetDependencyId<Concrete>()))
                return std::static_pointer_cast<Concrete>(mTransientContainer.at(GetDependencyId<Concrete>())(*this));

            return std::static_pointer_cast<Concrete>(mSingletonContainer.at(GetDependencyId<Concrete>()));
        }

        template <typename Concrete>
        bool Contains()
        {
            return mSingletonContainer.contains(GetDependencyId<Concrete>()) || mTransientContainer.contains(GetDependencyId<Concrete>());
        }

      protected:
        template <typename Concrete, typename... Args>
            requires(std::is_constructible_v<Concrete, Args...>)
        void AddSingletonImpl(std::tuple<Args...> tuple)
        {
            AddSingleton<Concrete>(GetService<std::remove_pointer_t<decltype(Args(nullptr).get())>>()...);
        }

        template <typename Concrete, typename... Args>
            requires(std::is_constructible_v<Concrete, Args...>)
        void AddTransientImpl(std::tuple<Args...> tuple)
        {
            AddTransient<Concrete>([](DIContainer& dependencyContainer) {
                return std::make_shared<Concrete>(dependencyContainer.GetService<std::remove_pointer_t<decltype(Args(nullptr).get())>>()...);
            });
        }

      private:
        std::unordered_map<DependencyId, std::function<std::shared_ptr<void>(DIContainer&)>> mTransientContainer;
        std::unordered_map<DependencyId, std::shared_ptr<void>>                              mSingletonContainer;
    };
} // FREYR_NAMESPACE
