#include "Freyr/Containers/Signature.hpp"

#include <functional>

namespace FREYR_NAMESPACE
{
    bool Signature::Match(const Signature& other) const
    {
        const auto bitSetCount = std::min(mBitSets.size(), other.mBitSets.size());

        for (size_t index = 0; index < bitSetCount; index++)
        {
            if ((mBitSets[index] & other.mBitSets[index]) != mBitSets[index])
                return false;
        }

        for (auto index = bitSetCount; index < mBitSets.size(); index++)
        {
            if (mBitSets[index].any())
                return false;
        }

        return true;
    }

    bool Signature::Intersects(const Signature& other) const
    {
        const auto bitSetCount = std::min(mBitSets.size(), other.mBitSets.size());

        for (size_t index = 0; index < bitSetCount; index++)
        {
            if ((mBitSets[index] & other.mBitSets[index]).any())
                return true;
        }

        return false;
    }

    bool Signature::operator==(const Signature& other) const
    {
        const auto bitSetCount = std::min(mBitSets.size(), other.mBitSets.size());

        for (size_t index = 0; index < bitSetCount; index++)
        {
            if (mBitSets[index] != other.mBitSets[index])
                return false;
        }

        const auto& bitSets = mBitSets.size() > other.mBitSets.size() ? mBitSets : other.mBitSets;

        for (auto index = bitSetCount; index < bitSets.size(); index++)
        {
            if (bitSets[index].any())
            {
                return false;
            }
        }

        return true;
    }

    size_t Signature::Hash() const
    {
        size_t used = mBitSets.size();
        while (used > 0 && !mBitSets[used - 1].any())
        {
            --used;
        }

        size_t hash = 0;
        for (size_t index = 0; index < used; ++index)
        {
            hash ^= std::hash<BitSet> {}(mBitSets[index]) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        }

        return hash;
    }
} // namespace FREYR_NAMESPACE
