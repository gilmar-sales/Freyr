#include "gtest/gtest.h"

#include <Freyr/Freyr.hpp>

#include "../Components/ModelComponent.hpp"
#include "../Components/PositionComponent.hpp"

struct CompA : fr::Component
{
};
struct CompB : fr::Component
{
};
struct CompC : fr::Component
{
};
struct CompD : fr::Component
{
};

class SignatureSpec : public ::testing::Test
{
};

TEST_F(SignatureSpec, SignatureShouldBeEmptyByDefault)
{
    auto signature = fr::Signature {};

    auto other = fr::Signature {};
    ASSERT_TRUE(signature == other);
}

TEST_F(SignatureSpec, SignatureShouldAddSingleComponent)
{
    auto signature = fr::Signature {};

    signature.AddComponent<PositionComponent>();

    auto other = fr::Signature {};
    other.AddComponent<PositionComponent>();
    ASSERT_TRUE(signature == other);
}

TEST_F(SignatureSpec, SignatureShouldAddMultipleComponents)
{
    auto signature = fr::Signature {};

    signature.AddComponents<PositionComponent, ModelComponent>();

    auto other = fr::Signature {};
    other.AddComponents<PositionComponent, ModelComponent>();
    ASSERT_TRUE(signature == other);
}

TEST_F(SignatureSpec, SignatureShouldRemoveComponent)
{
    auto signature = fr::Signature {};
    signature.AddComponent<PositionComponent>();

    signature.RemoveComponent<PositionComponent>();

    auto other = fr::Signature {};
    ASSERT_TRUE(signature == other);
}

TEST_F(SignatureSpec, SignatureShouldMatchWhenAllBitsArePresent)
{
    auto signature = fr::Signature {};
    signature.AddComponent<PositionComponent>();

    auto other = fr::Signature {};
    other.AddComponents<PositionComponent, ModelComponent>();

    ASSERT_TRUE(signature.Match(other));
}

TEST_F(SignatureSpec, SignatureShouldNotMatchWhenMissingBits)
{
    auto signature = fr::Signature {};
    signature.AddComponents<PositionComponent, ModelComponent>();

    auto other = fr::Signature {};
    other.AddComponent<PositionComponent>();

    ASSERT_FALSE(signature.Match(other));
}

TEST_F(SignatureSpec, SignatureShouldMatchEmptySignature)
{
    auto signature = fr::Signature {};
    auto other     = fr::Signature {};

    ASSERT_TRUE(signature.Match(other));
}

TEST_F(SignatureSpec, SignatureShouldMatchIdenticalSignatures)
{
    auto signature = fr::Signature {};
    signature.AddComponents<PositionComponent, ModelComponent>();

    auto other = fr::Signature {};
    other.AddComponents<PositionComponent, ModelComponent>();

    ASSERT_TRUE(signature.Match(other));
    ASSERT_TRUE(signature == other);
}

TEST_F(SignatureSpec, SignatureShouldNotMatchDifferentSignatures)
{
    auto signature = fr::Signature {};
    signature.AddComponent<PositionComponent>();

    auto other = fr::Signature {};
    other.AddComponent<ModelComponent>();

    ASSERT_FALSE(signature == other);
}

TEST_F(SignatureSpec, Signature_Make_ShouldCreateSignatureWithComponents)
{
    auto signature = fr::Signature::Make<PositionComponent, ModelComponent>();

    auto other = fr::Signature {};
    other.AddComponents<PositionComponent, ModelComponent>();

    ASSERT_TRUE(signature == other);
}

TEST_F(SignatureSpec, Signature_Make_ShouldMatchPartialSignature)
{
    auto partial = fr::Signature::Make<PositionComponent>();
    auto full    = fr::Signature::Make<PositionComponent, ModelComponent>();

    ASSERT_TRUE(partial.Match(full));
}

TEST_F(SignatureSpec, SignatureShouldHandleLargeComponentIds)
{
    auto signature = fr::Signature {};

    signature.AddComponent<CompA>();
    signature.AddComponent<CompB>();
    signature.AddComponent<CompC>();
    signature.AddComponent<CompD>();
    signature.AddComponent<PositionComponent>();
    signature.AddComponent<ModelComponent>();

    auto other = fr::Signature {};
    other.AddComponents<CompA, CompB, CompC, CompD, PositionComponent, ModelComponent>();

    ASSERT_TRUE(signature == other);
}

TEST_F(SignatureSpec, SignatureShouldGrowVectorWhenAddingManyComponents)
{
    auto signature = fr::Signature {};
    signature.AddComponent<CompA>();
    signature.AddComponent<CompB>();

    signature.RemoveComponent<CompA>();

    auto other = fr::Signature {};
    other.AddComponent<CompB>();

    ASSERT_TRUE(signature == other);
}

TEST_F(SignatureSpec, MatchShouldReturnTrueWhenThisIsSubsetOfOther)
{
    auto sig1 = fr::Signature::Make<CompA, CompB>();
    auto sig2 = fr::Signature::Make<CompA, CompB, CompC>();
    auto sig3 = fr::Signature::Make<CompA>();

    ASSERT_TRUE(sig1.Match(sig2));
    ASSERT_TRUE(sig3.Match(sig1));
    ASSERT_FALSE(sig2.Match(sig1));
    ASSERT_FALSE(sig1.Match(sig3));
}

TEST_F(SignatureSpec, EqualityShouldReturnFalseForDifferentSignatures)
{
    auto sig1 = fr::Signature::Make<CompA, CompB>();
    auto sig2 = fr::Signature::Make<CompA, CompC>();
    auto sig3 = fr::Signature::Make<CompA, CompB>();
    auto sig4 = fr::Signature::Make<CompA>();

    ASSERT_FALSE(sig1 == sig2);
    ASSERT_TRUE(sig1 == sig3);
    ASSERT_FALSE(sig1 == sig4);
}

TEST_F(SignatureSpec, EqualityShouldReturnFalseWhenExtraBitsetsHaveBits)
{
    // Arrange
    auto sig1 = fr::Signature {};
    sig1.AddComponent<PositionComponent>();
    sig1.AddComponent(static_cast<fr::ComponentId>(132));

    auto sig2 = fr::Signature {};
    sig2.AddComponent<PositionComponent>();

    // Act
    const auto equals        = sig1 == sig2;
    const auto equalsReverse = sig2 == sig1;

    ASSERT_FALSE(equals);
    ASSERT_FALSE(equalsReverse);
}
