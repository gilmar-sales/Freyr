#pragma once

#include <algorithm>
#include <atomic>
#include <concepts>
#include <mutex>
#include <vector>

#include <boost/container/vector.hpp>

namespace FREYR_NAMESPACE
{
    template <typename T>
    concept has_size_t_cast = requires(T value) {
        { value } -> std::convertible_to<std::size_t>;
    };

    template <typename T>
        requires(has_size_t_cast<T>)
    class SparseSet
    {
      public:
        explicit SparseSet(unsigned capacity = 512u)
        {
            resize(capacity);
            sorted    = false;
            sparse[0] = -1;
            mIndex    = 0;
        }

        SparseSet(const SparseSet& other)
        {
            dense.reserve(other.dense.capacity());
            sparse.resize(other.sparse.size());
            sorted = false;

            for (auto value : other.dense)
            {
                insert(value);
            }
        }

        ~SparseSet() = default;

        void insert(const T& n)
        {
            if (contains(n))
                return;

            std::lock_guard lock { m_lock };

            grow(n);

            sparse[n] = static_cast<T>(dense.size());
            dense.emplace_back(n);
            sorted = false;
        }

        void try_insert(const T& n)
        {
            if (contains(n))
                return;

            grow(n);

            auto index   = mIndex.fetch_add(1, std::memory_order_relaxed);
            dense[index] = n;

            sparse[n] = static_cast<T>(index);
            sorted    = false;
        }

        void remove(const T& n)
        {
            if (!contains(n))
                return;

            std::lock_guard lock { m_lock };

            dense[sparse[n]]                = dense[dense.size() - 1];
            sparse[dense[dense.size() - 1]] = sparse[n];
            sparse[n]                       = n - 1;
            dense.pop_back();
            sorted = false;
        }

        void try_remove(const T& n)
        {
            if (!contains(n))
                return;

            const auto index =
                mIndex.fetch_sub(1, std::memory_order_relaxed) - 1;

            if (index >= 0)
            {
                std::lock_guard lock { m_lock };
                dense[sparse[n]]     = dense[index];
                sparse[dense[index]] = sparse[n];
            }

            sparse[n] = n - 1;

            sorted = false;
        }

        void swap(const T& a, const T& b)
        {
            if (!contains(a))
                return;

            if (contains(b))
                return;

            sparse[b]        = sparse[a];
            dense[sparse[a]] = b;
            sparse[a]        = 0;
        }

        bool contains(const T& n) const
        {
            return sparse.size() > n && sparse[n] < dense.size() &&
                   dense[sparse[n]] == n;
        }

        void clear() { dense.clear(); }

        void resize(unsigned size)
        {
            dense.resize(size);
            sparse.resize(size);
        }

        void sort()
        {
            if (sorted)
                return;

            denseSort();

            sparseReorder();
            sorted = true;
        }

        T& operator[](int index) { return dense[index]; };

        std::uint64_t size() { return dense.size(); }

        auto begin() const { return dense.rbegin(); }

        auto end() const { return dense.rend(); }

        SparseSet<T> intersect(const SparseSet<T>& other)
        {
            auto intersection = SparseSet<T>(sparse.size());

            bool useOther = dense.size() > other.dense.size();

            const auto& base = useOther ? other : *this;

            const auto& compare = useOther ? *this : other;

            for (auto entity : base)
            {
                if (compare.contains(entity))
                    intersection.insert(entity);
            }

            return std::move(intersection);
        }

        const T& getIndex(const T& value) { return sparse[value]; }

        const std::vector<T>& getDense() { return dense; }

      protected:
        void denseSort() { std::sort(dense.begin(), dense.end()); }

        void grow(size_t size)
        {
            if (sparse.size() > size)
                return;

            size = static_cast<size_t>(std::max(sparse.size(), size) * 1.3f);

            resize(size);
        }

        void sparseReorder()
        {
            for (T i = 0; i < dense.size(); ++i)
            {
                sparse[dense[i]] = i;
            }
        }

      private:
        std::atomic<int>        mIndex;
        std::mutex              m_lock;
        std::condition_variable m_cond;
        std::vector<T>          dense;
        std::vector<T>          sparse;
        bool                    sorted;
    };

} // namespace FREYR_NAMESPACE
