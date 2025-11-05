#pragma once

#include <algorithm>
#include <atomic>
#include <concepts>
#include <mutex>
#include <vector>

namespace FREYR_NAMESPACE
{
    template <typename T>
    concept has_size_t_cast = requires(T value) {
        { value } -> std::convertible_to<std::size_t>;
    };

    template <typename T>
        requires(has_size_t_cast<std::remove_pointer_t<T>>)
    class SparseSet
    {
      public:
        SparseSet(unsigned capacity = 512u)
        {
            mDense.reserve(capacity);
            mSparse.resize(capacity);
        }

        SparseSet(const SparseSet& other)
        {
            mDense.reserve(other.mDense.capacity());
            mSparse.resize(other.mSparse.size());

            for (auto value : other.mDense)
            {
                insert(value);
            }
        }

        ~SparseSet() = default;

        void insert(const T& element)
        {

            if (contains(element))
                return;

            std::lock_guard lock { mLock };

            int n = getValue(element);

            grow(n);

            mSparse[n] = mDense.size();
            mDense.emplace_back(element);
        }

        void remove(const T& n)
        {
            if (!contains(n))
                return;

            std::lock_guard lock { mLock };

            mDense[mSparse[n]]                 = mDense[mDense.size() - 1];
            mSparse[mDense[mDense.size() - 1]] = mSparse[n];
            mSparse[n]                         = 0;
            mDense.pop_back();
        }

        void swap(const T& a, const T& b)
        {
            if (!contains(a))
                return;

            if (contains(b))
                return;

            mSparse[b]         = mSparse[a];
            mDense[mSparse[a]] = b;
            mSparse[a]         = 0;
        }

        template <typename TElement>
            requires(std::is_pointer_v<TElement>)
        bool contains(const TElement& element) const
        {
            size_t n = getValue(element);

            return contains(n);
        }

        bool contains(const size_t& n) const
        {
            return mSparse.size() > n && mSparse[n] < mDense.size() &&
                   getValue(mDense[mSparse[n]]) == n;
        }

        void clear()
        {
            std::lock_guard<std::mutex> lock { mLock };
            mDense.clear();
        }

        void resize(unsigned size)
        {
            mDense.reserve(size);
            mSparse.resize(size);
        }

        size_t capacity() { return mSparse.size(); }

        void sort()
        {
            std::lock_guard lock { mLock };
            denseSort();

            sparseReorder();
        }

        T& operator[](auto& element) const
        {
            const int n = getValue(element);

            return const_cast<T&>(mDense[mSparse[n]]);
        };

        std::uint64_t size() { return mDense.size(); }

        auto begin() const { return mDense.rbegin(); }

        auto end() const { return mDense.rend(); }

        SparseSet<T> intersect(const SparseSet<T>& other)
        {
            auto intersection = SparseSet<T>(mSparse.size());

            bool useOther = mDense.size() > other.mDense.size();

            const auto& base = useOther ? other : *this;

            const auto& compare = useOther ? *this : other;

            for (auto entity : base)
            {
                if (compare.contains(entity))
                    intersection.insert(entity);
            }

            return std::move(intersection);
        }

        size_t getIndex(const size_t value) const { return mSparse[value]; }

        const std::vector<T>& getDense() { return mDense; }

        bool isFull() { return mDense.size() == mDense.capacity(); }

      protected:
        void denseSort() { std::sort(mDense.begin(), mDense.end()); }

        void grow(size_t size)
        {
            if (mSparse.size() > size)
                return;

            size = static_cast<size_t>(
                std::max(mSparse.size(), static_cast<size_t>(size * 1.3)));

            mSparse.resize(size);
            mDense.reserve(size);
        }

        void sparseReorder()
        {
            for (T i = 0; i < mDense.size(); i++)
            {
                mSparse[mDense[i]] = i;
            }
        }

        inline size_t getValue(auto& element) const { return element; }
        inline size_t getValue(auto* element) const { return (*element); }

      private:
        std::mutex          mLock;
        std::vector<T>      mDense;
        std::vector<size_t> mSparse;
    };

} // namespace FREYR_NAMESPACE
