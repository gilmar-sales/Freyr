#pragma once

#include <algorithm>
#include <concepts>
#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

#include "Freyr/Core/Assertions.hpp"
#include "Freyr/Core/RwLock.hpp"

namespace FREYR_NAMESPACE
{
    template <typename T>
    concept has_size_t_cast = requires(T value) {
        { value } -> std::convertible_to<std::size_t>;
    };

    struct LockedSync
    {
        RwLock lock;

        auto read() { return lock.read(); }
        auto write() { return lock.write(); }
    };

    struct UnlockedSync
    {
        struct NoopGuard
        {
        };

        NoopGuard read() const { return {}; }
        NoopGuard write() const { return {}; }
    };

    template <typename T, typename Sync = LockedSync>
        requires(has_size_t_cast<std::remove_pointer_t<T>>)
    class SparseSet
    {
      public:
        static constexpr size_t BUCKET_SIZE  = 4096;
        static constexpr size_t BUCKET_SHIFT = 12;
        static constexpr size_t BUCKET_MASK  = BUCKET_SIZE - 1;

        using Index                 = std::uint32_t;
        static constexpr Index npos = std::numeric_limits<Index>::max();

        explicit SparseSet(unsigned capacity = 512u) { mDense.reserve(capacity); }

        SparseSet(const SparseSet& other)
        {
            mDense.reserve(other.mDense.capacity());

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

            const size_t n = getValue(element);

            ensureBucket(n);

            const size_t bucketIdx = n >> BUCKET_SHIFT;
            const size_t localIdx  = n & BUCKET_MASK;

            auto write                          = mSync.write();
            mSparseBuckets[bucketIdx][localIdx] = static_cast<Index>(mDense.size());
            mDense.emplace_back(element);
        }

        template <typename TElement>
            requires(std::is_pointer_v<TElement>)
        void remove(const TElement element)
        {
            const size_t n = getValue(element);
            remove(n);
        }

        void remove(const size_t n)
        {
            const Index indexToRemove = find(n);
            if (indexToRemove == npos)
                return;

            auto write = mSync.write();

            const size_t bucketIdx = n >> BUCKET_SHIFT;
            const size_t localIdx  = n & BUCKET_MASK;
            const size_t lastIdx   = mDense.size() - 1;

            mDense[indexToRemove] = mDense[lastIdx];

            const size_t movedValue     = getValue(mDense[lastIdx]);
            const size_t movedBucketIdx = movedValue >> BUCKET_SHIFT;
            const size_t movedLocalIdx  = movedValue & BUCKET_MASK;

            mSparseBuckets[movedBucketIdx][movedLocalIdx] = indexToRemove;
            mSparseBuckets[bucketIdx][localIdx]           = npos;

            mDense.pop_back();
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

            auto write = mSync.write();

            const Index denseIdx                  = mSparseBuckets[bucketIdxA][localIdxA];
            mSparseBuckets[bucketIdxB][localIdxB] = denseIdx;
            mDense[denseIdx]                      = b;
            mSparseBuckets[bucketIdxA][localIdxA] = npos;
        }

        template <typename TElement>
            requires(std::is_pointer_v<TElement>)
        [[nodiscard]] bool contains(TElement element) const
        {
            const size_t n = getValue(element);
            return contains(n);
        }

        [[nodiscard]] bool contains(const size_t n) const { return find(n) != npos; }

        [[nodiscard]] Index find(const size_t n) const
        {
            const size_t bucketIdx = n >> BUCKET_SHIFT;
            const size_t localIdx  = n & BUCKET_MASK;

            auto read = mSync.read();

            if (bucketIdx >= mSparseBuckets.size())
                return npos;

            const auto& bucket = mSparseBuckets[bucketIdx];
            if (!bucket)
                return npos;

            return bucket[localIdx];
        }

        [[nodiscard]] Index index(const size_t n) const
        {
            FREYR_ASSERT(find(n) != npos && "SparseSet::index called for missing key.");

            auto read = mSync.read();
            return mSparseBuckets[n >> BUCKET_SHIFT][n & BUCKET_MASK];
        }

        void clear()
        {
            auto write = mSync.write();

            mDense.clear();
            mSparseBuckets.clear();
        }

        void resize(const size_t size) { growDense(size); }

        size_t capacity() const { return mDense.capacity(); }

        void sort()
        {
            auto write = mSync.write();
            denseSort();
            sparseReorder();
        }

        T at(size_t index) const { return mDense.data()[index]; }

        T& operator[](auto& element) const
        {
            const size_t n         = getValue(element);
            const size_t bucketIdx = n >> BUCKET_SHIFT;
            const size_t localIdx  = n & BUCKET_MASK;

            auto read = mSync.read();
            return const_cast<T&>(mDense.data()[mSparseBuckets[bucketIdx][localIdx]]);
        }

        size_t size() const { return mDense.size(); }

        auto begin() const { return mDense.begin(); }

        auto end() const { return mDense.end(); }

        SparseSet intersect(const SparseSet& other)
        {
            auto intersection = SparseSet(mDense.capacity());

            bool useOther = mDense.size() > other.mDense.size();

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
            const Index denseIdx = find(value);
            if (denseIdx == npos)
            {
                FREYR_ASSERT(false && "SparseSet::getIndex called for missing key.");
                return 0;
            }

            return denseIdx;
        }

        size_t lastIndex() const { return mDense.size() - 1; }

        const std::vector<T>& getDense() const { return mDense; }

        bool isFull() { return mDense.size() == mDense.capacity(); }

      protected:
        void denseSort() { std::sort(mDense.begin(), mDense.end()); }

        void ensureBucket(size_t index)
        {
            const size_t bucketIdx = index >> BUCKET_SHIFT;

            auto write = mSync.write();
            if (bucketIdx >= mSparseBuckets.size())
            {
                mSparseBuckets.resize(bucketIdx + 1);
            }

            if (mSparseBuckets[bucketIdx] == nullptr)
            {
                mSparseBuckets[bucketIdx] = std::make_unique_for_overwrite<Index[]>(BUCKET_SIZE);
                std::fill_n(mSparseBuckets[bucketIdx].get(), BUCKET_SIZE, npos);
            }
        }

        void growDense(size_t size)
        {
            if (mDense.capacity() > size)
                return;

            size =
                static_cast<size_t>(std::max(mDense.capacity(), static_cast<size_t>(size * 1.3)));

            auto write = mSync.write();
            mDense.reserve(size);
        }

        void sparseReorder()
        {
            for (size_t i = 0; i < mDense.size(); ++i)
            {
                const size_t value     = getValue(mDense[i]);
                const size_t bucketIdx = value >> BUCKET_SHIFT;
                const size_t localIdx  = value & BUCKET_MASK;

                mSparseBuckets[bucketIdx][localIdx] = static_cast<Index>(i);
            }
        }

        static inline size_t getValue(const auto& element) { return element; }
        static inline size_t getValue(auto* element) { return *element; }

      private:
        mutable Sync                           mSync;
        std::vector<T>                         mDense;
        std::vector<std::unique_ptr<Index[]>>  mSparseBuckets;
    };

    template <typename T>
    using LocalSparseSet = SparseSet<T, UnlockedSync>;
} // namespace FREYR_NAMESPACE
