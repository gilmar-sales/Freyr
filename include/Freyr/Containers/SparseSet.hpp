#pragma once

#include <algorithm>
#include <concepts>
#include <shared_mutex>
#include <vector>
#include <memory>

namespace
FREYR_NAMESPACE
{
    template <typename T>
    concept has_size_t_cast = requires(T value)
    {
        { value } -> std::convertible_to<std::size_t>;
    };

    template <typename T>
        requires(has_size_t_cast<std::remove_pointer_t<T>>)
    class SparseSet
    {
    public:
        static constexpr size_t BUCKET_SIZE  = 4096; // 4KB buckets
        static constexpr size_t BUCKET_SHIFT = 12;   // log2(4096)
        static constexpr size_t BUCKET_MASK  = BUCKET_SIZE - 1;

        explicit SparseSet(unsigned capacity = 512u)
        {
            mDense.reserve(capacity);
            mCount         = 0;
            mDenseCapacity = capacity;
        }

        SparseSet(const SparseSet& other)
        {
            mDense.reserve(other.mDense.capacity());
            mDenseCapacity = other.mDenseCapacity;

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

            const size_t n = getValue(element);
            ensureBucket(n);

            const size_t bucketIdx = n >> BUCKET_SHIFT;
            const size_t localIdx  = n & BUCKET_MASK;

            mSparseBuckets[bucketIdx][localIdx] = mCount++;
            mDense.emplace_back(element);
        }

        void remove(T n)
        {
            if (!contains(n))
                return;

            std::unique_lock lock(mMutex);

            const size_t value     = getValue(n);
            const size_t bucketIdx = value >> BUCKET_SHIFT;
            const size_t localIdx  = value & BUCKET_MASK;

            const size_t indexToRemove = mSparseBuckets[bucketIdx][localIdx];
            const size_t lastIdx       = lastIndex();

            // Swap with last element
            mDense[indexToRemove] = mDense[lastIdx];

            // Update the moved element's sparse index
            const size_t movedValue                       = getValue(mDense[lastIdx]);
            const size_t movedBucketIdx                   = movedValue >> BUCKET_SHIFT;
            const size_t movedLocalIdx                    = movedValue & BUCKET_MASK;
            mSparseBuckets[movedBucketIdx][movedLocalIdx] = indexToRemove;

            // Clear removed element's sparse index
            mSparseBuckets[bucketIdx][localIdx] = 0;

            mDense.pop_back();
            mCount -= 1;
        }

        void swap(const T a, const T b)
        {
            if (!contains(a))
                return;

            if (contains(b))
                return;

            const size_t valueA = getValue(a);
            const size_t valueB = getValue(b);

            const size_t bucketIdxA = valueA >> BUCKET_SHIFT;
            const size_t localIdxA  = valueA & BUCKET_MASK;
            const size_t bucketIdxB = valueB >> BUCKET_SHIFT;
            const size_t localIdxB  = valueB & BUCKET_MASK;

            ensureBucket(valueB);

            const size_t denseIdx                 = mSparseBuckets[bucketIdxA][localIdxA];
            mSparseBuckets[bucketIdxB][localIdxB] = denseIdx;
            mDense[denseIdx]                      = b;
            mSparseBuckets[bucketIdxA][localIdxA] = 0;
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

            const size_t bucketIdx = n >> BUCKET_SHIFT;
            const size_t localIdx  = n & BUCKET_MASK;

            if (bucketIdx >= mSparseBuckets.size())
                return false;

            if (!mSparseBuckets[bucketIdx])
                return false;

            const size_t denseIdx = mSparseBuckets[bucketIdx][localIdx];

            return denseIdx < mCount && getValue(mDense[denseIdx]) == n;
        }

        void clear()
        {
            std::unique_lock lock(mMutex);
            mDense.clear();
            mCount = 0;
            // Optionally clear buckets to free memory
            // mSparseBuckets.clear();
        }

        void resize(const size_t size)
        {
            std::unique_lock lock(mMutex);
            growDense(size);
        }

        size_t capacity() const { return mDenseCapacity; }

        void sort()
        {
            std::unique_lock lock(mMutex);
            denseSort();
            sparseReorder();
        }

        T at(size_t index) const
        {
            return mDense[index];
        }

        T& operator[](auto& element) const
        {
            const size_t n         = getValue(element);
            const size_t bucketIdx = n >> BUCKET_SHIFT;
            const size_t localIdx  = n & BUCKET_MASK;

            return const_cast<T&>(mDense[mSparseBuckets[bucketIdx][localIdx]]);
        }

        size_t size() const { return mCount; }

        auto begin() const { return mDense.rbegin(); }

        auto end() const { return mDense.rend(); }

        SparseSet<T> intersect(const SparseSet<T>& other)
        {
            auto intersection = SparseSet<T>(mDenseCapacity);

            bool useOther = mCount > other.mCount;

            const auto& base    = useOther ? other : *this;
            const auto& compare = useOther ? *this : other;

            for (auto entity : base)
            {
                if (compare.contains(entity))
                    intersection.insert(entity);
            }

            return std::move(intersection);
        }

        size_t getIndex(const size_t value) const
        {
            const size_t bucketIdx = value >> BUCKET_SHIFT;
            const size_t localIdx  = value & BUCKET_MASK;

            if (bucketIdx >= mSparseBuckets.size() || !mSparseBuckets[bucketIdx])
                return 0;

            return mSparseBuckets[bucketIdx][localIdx];
        }

        size_t lastIndex() const { return mCount - 1; }

        const std::vector<T>& getDense() { return mDense; }

        bool isFull() { return mCount == mDense.capacity(); }

    protected:
        void denseSort() { std::sort(mDense.begin(), mDense.end()); }

        void ensureBucket(size_t index)
        {
            const size_t bucketIdx = index >> BUCKET_SHIFT;

            if (bucketIdx >= mSparseBuckets.size())
            {
                mSparseBuckets.resize(bucketIdx + 1);
            }

            if (!mSparseBuckets[bucketIdx])
            {
                mSparseBuckets[bucketIdx] = std::make_unique<size_t[]>(BUCKET_SIZE);
                // Initialize bucket to zeros
                std::fill_n(mSparseBuckets[bucketIdx].get(), BUCKET_SIZE, 0);
            }
        }

        void growDense(size_t size)
        {
            if (mDenseCapacity > size)
                return;

            size = static_cast<size_t>(std::max(mDenseCapacity, static_cast<size_t>(size * 1.3)));

            mDense.reserve(size);
            mDenseCapacity = size;
        }

        void sparseReorder()
        {
            for (size_t i = 0; i < mCount; ++i)
            {
                const size_t value     = getValue(mDense[i]);
                const size_t bucketIdx = value >> BUCKET_SHIFT;
                const size_t localIdx  = value & BUCKET_MASK;

                mSparseBuckets[bucketIdx][localIdx] = i;
            }
        }

        static inline size_t getValue(auto element) { return element; }
        static inline size_t getValue(auto* element) { return *element; }

    private:
        mutable std::shared_mutex              mMutex;
        size_t                                 mCount{};
        size_t                                 mDenseCapacity{};
        std::vector<T>                         mDense;
        std::vector<std::unique_ptr<size_t[]>> mSparseBuckets;
    };
} // namespace FREYR_NAMESPACE