#pragma once

#include <algorithm>
#include <concepts>
#include <shared_mutex>
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
        explicit SparseSet(unsigned capacity = 512u)
        {
            mDense.reserve(capacity);
            mSparse.resize(capacity);
            mCount    = 0;
            mCapacity = capacity;
        }

        SparseSet(const SparseSet& other)
        {
            mDense.reserve(other.mDense.capacity());
            mSparse.resize(other.mCapacity);

            for (auto value : other.mDense)
            {
                insert(value);
            }
        }

        ~SparseSet() = default;

        void insert(T element)
        {
            if (contains(element))
                return;

            std::unique_lock lock(mMutex);

            const int n = getValue(element);

            grow(n);

            mSparse[n] = mCount++;
            mDense.emplace_back(element);
        }

        void remove(T n)
        {
            if (!contains(n))
                return;

            std::unique_lock lock(mMutex);

            mDense[mSparse[n]]           = mDense[lastIndex()];
            mSparse[mDense[lastIndex()]] = mSparse[n];
            mSparse[n]                   = 0;
            mDense.pop_back();
            mCount -= 1;
        }

        void swap(const T a, const T b)
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
        bool contains(const TElement element) const
        {
            const size_t n = getValue(element);

            return contains(n);
        }

        [[nodiscard]] bool contains(const size_t n) const
        {
            std::shared_lock readLock(mMutex);
            return mCapacity > n && mSparse[n] < mCount && getValue(mDense[mSparse[n]]) == n;
        }

        void clear()
        {
            std::unique_lock lock(mMutex);
            mDense.clear();
        }

        void resize(unsigned size)
        {
            std::unique_lock lock(mMutex);
            grow(size);
        }

        size_t capacity() { return mCapacity; }

        void sort()
        {
            std::unique_lock lock(mMutex);
            denseSort();
            sparseReorder();
        }

        T& operator[](auto& element) const
        {
            const int n = getValue(element);

            return const_cast<T&>(mDense[mSparse[n]]);
        };

        size_t size() { return mCount; }

        auto begin() const { return mDense.rbegin(); }

        auto end() const { return mDense.rend(); }

        SparseSet<T> intersect(const SparseSet<T>& other)
        {
            auto intersection = SparseSet<T>(mCapacity);

            bool useOther = mCount > other.mCount;

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

        size_t lastIndex() const { return mCount - 1; }

        const std::vector<T>& getDense() { return mDense; }

        bool isFull() { return mCount == mDense.capacity(); }

      protected:
        void denseSort() { std::sort(mDense.begin(), mDense.end()); }

        void grow(size_t size)
        {
            if (mCapacity > size)
                return;

            size = static_cast<size_t>(std::max(mCapacity, static_cast<size_t>(size * 1.3)));

            mSparse.resize(size);
            mDense.reserve(size);
            mCapacity = size;
        }

        void sparseReorder()
        {
            for (size_t i = 0; i < mCount; ++i)
            {
                mSparse[mDense[i]] = i;
            }
        }

        static inline size_t getValue(auto& element) { return element; }
        static inline size_t getValue(auto* element) { return (*element); }

      private:
        mutable std::shared_mutex mMutex;
        size_t                    mCount {};
        size_t                    mCapacity {};
        std::vector<T>            mDense;
        std::vector<size_t>       mSparse;
    };
} // namespace FREYR_NAMESPACE
