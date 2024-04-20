#pragma once

namespace FREYR_NAMESPACE
{

    namespace meta
    {
        template <typename... Ts>
        using List = std::tuple<Ts...>;

        namespace impl
        {

            // List indexOf
            template <class T, class Tuple>
            struct indexOf;

            template <typename T>
            struct indexOf<T, std::tuple<>>
            {
                static constexpr std::uint64_t value = -1;
            };

            template <class T, class... Ts>
            struct indexOf<T, std::tuple<T, Ts...>>
            {
                static constexpr std::uint64_t value = 0;
            };

            template <class T, class U, class... Ts>
            struct indexOf<T, std::tuple<U, Ts...>>
            {
                static constexpr std::uint64_t value = 1 + indexOf<T, std::tuple<Ts...>>::value;
            };

            // tuple sizeOf
            template <typename... Ts>
            struct sizeOf;

            template <typename T>
            struct sizeOf<std::tuple<T>>
            {
                static constexpr int value = 1;
            };

            template <typename T, typename... Ts>
            struct sizeOf<std::tuple<T, Ts...>> : sizeOf<std::tuple<Ts...>>
            {
                static constexpr int value = sizeOf<std::tuple<Ts...>>::value + 1;
            };

            // tuple contains
            template <typename T, typename Tuple>
            struct contains;

            template <typename T>
            struct contains<T, std::tuple<>> : std::false_type
            {
            };

            template <typename T, typename U, typename... Ts>
            struct contains<T, std::tuple<U, Ts...>> : contains<T, std::tuple<Ts...>>
            {
            };

            template <typename T, typename... Ts>
            struct contains<T, std::tuple<T, Ts...>> : std::true_type
            {
            };

            // make a List be a tuple
            template <class List>
            struct asTuple;

            template <template <class...> class List, class... Elements>
            struct asTuple<List<Elements...>>
            {
                using type = std::tuple<Elements...>;
            };

            // unwrap a list to another container
            template <template <class...> class Container, class List>
            struct rename;

            template <template <class...> class Container, template <class...> class List, class... Elements>
            struct rename<Container, List<Elements...>>
            {
                using type = Container<Elements...>;
            };

            // generate a tuple of N Ts
            template <typename T, std::uint64_t N, typename... Ts>
            struct generateTuple
            {
                using type = typename generateTuple<T, N - 1, T, Ts...>::type;
            };

            template <typename T, typename... Ts>
            struct generateTuple<T, 0, Ts...>
            {
                using type = std::tuple<Ts...>;
            };

            // tuple_from_func_args
            template <typename F, typename... Args>
            struct extract_func_args;

            template <typename F, typename... Args>
            struct extract_func_args<F(Args...)>
            {
                using tuple = std::tuple<Args...>;
            };
        } // namespace impl

        template <typename T, std::uint64_t N>
        using generateTuple = typename impl::generateTuple<T, N>::type;

        template <typename T>
        using extract_func_args = typename impl::extract_func_args<T>::tuple;

        template <template <class...> class Container, class List>
        using rename = typename impl::rename<Container, List>::type;

        template <typename T, typename Tuple>
        using contains = typename impl::contains<T, Tuple>::type;

        template <typename T, typename Tuple>
        constexpr std::uint64_t indexOf()
        {
            return impl::indexOf<T, Tuple>::value;
        }

        template <typename Tuple>
        constexpr std::uint64_t sizeOf()
        {
            return impl::sizeOf<Tuple>::value;
        }
    } // namespace meta

} // namespace FREYR_NAMESPACE
