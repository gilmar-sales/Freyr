#pragma once

#include <algorithm>
#include <mutex>
#include <vector>

namespace FREYR_NAMESPACE
{

    template<typename T>
    class SparseSet
    {
      public:
        SparseSet(unsigned capacity = 512u)
        {
            dense.reserve(capacity);
            sparse.resize(capacity);
            sorted = false;
        }

        SparseSet(const SparseSet &other)
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

        void insert(const T &n)
        {
            if (contains(n)) return;
            std::lock_guard lock{m_lock};

            sparse[n] = static_cast<T>(dense.size());
            dense.push_back(n);
            sorted = false;
        }

        void remove(const T &n)
        {
            if (!contains(n)) return;

            std::lock_guard<std::mutex> lock{m_lock};

            dense[sparse[n]]                = dense[dense.size() - 1];
            sparse[dense[dense.size() - 1]] = sparse[n];
            sparse[n]                       = 0;
            dense.pop_back();
            sorted = false;
        }

        bool contains(const T &n) const { return sparse[n] < dense.size() && dense[sparse[n]] == n; }

        void clear() { dense.clear(); }

        void resize(unsigned size)
        {
            dense.reserve(size);
            sparse.resize(size);
        }

        void sort()
        {
            if (sorted) return;
            std::lock_guard lock{m_lock};
            denseSort();

            sparseReorder();
            sorted = true;
        }

        T &operator[](int index) { return dense[index]; };

        std::uint64_t size() { return dense.size(); }

        auto begin() const { return dense.rbegin(); }

        auto end() const { return dense.rend(); }

        SparseSet<T> intersect(const SparseSet<T> &other)
        {
            auto intersection = SparseSet<T>(sparse.size());

            auto base = dense.size() > other.dense.size() ? other : *this;

            return intersection;
        }

        const T &getIndex(const T &value) { return sparse[value]; }

      protected:
        void denseSort() { std::sort(dense.begin(), dense.end()); }

        void sparseReorder()
        {
            for (T i = 0; i < dense.size(); i++)
            {
                sparse[dense[i]] = i;
            }
        }

      private:
        std::mutex m_lock;
        std::vector<T> dense;
        std::vector<T> sparse;
        bool sorted;
    };

} // namespace FREYR_NAMESPACE
