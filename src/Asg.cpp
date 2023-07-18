#include "Asg.h"
#include <algorithm>
#include <format>

SIXPACK_NAMESPACE_BEGIN

//============================================================================================================
// Term
//============================================================================================================

int asg::Term::depth() const {
    if (!mDepth) {
        mDepth = getDepth();
    }
    return *mDepth;
}

StringView asg::Term::key() const {
    if (!mKey) {
        mKey = getKey();
    }
    return *mKey;
}

bool asg::Term::canBeModified() const {
    return !mDepth && !mKey;
}

//============================================================================================================
// Sequence
//============================================================================================================

void asg::Sequence::addTerm(std::shared_ptr<const Term> term) {
    assert(term);
    assert(canBeModified());
    mTerms.push_back(term);
}

std::optional<Real> asg::Sequence::evaluateConstant() const {
    return std::nullopt;
}

void asg::Sequence::accept(Visitor& visitor) const {
    visitor.visit(*this);
}

int asg::Sequence::getDepth() const {
    int depth = -1;
    for (const auto& term : mTerms) {
        depth = std::max(depth, term->depth());
    }
    return 1 + depth;
}

String asg::Sequence::getKey() const {
    std::vector<StringView> keys;
    for (const auto& term : mTerms) {
        keys.push_back(term->key());
    }
    std::sort(keys.begin(), keys.end());
    String result;
    for (const StringView& key : keys) {
        if (!result.empty()) {
            result += "|";
        }
        result += key;
    }
    return result;
}

//============================================================================================================
// Constant
//============================================================================================================

void asg::Constant::accept(Visitor& visitor) const {
    visitor.visit(*this);
}

int asg::Constant::getDepth() const {
    return 0;
}

String asg::Constant::getKey() const {
    return std::format("{}", mValue);
}

//============================================================================================================
// Input
//============================================================================================================

void asg::Input::accept(Visitor& visitor) const {
    visitor.visit(*this);
}

int asg::Input::getDepth() const {
    return 0;
}

String asg::Input::getKey() const {
    return String(mName);
}

//============================================================================================================
// Output
//============================================================================================================

std::optional<Real> asg::Output::evaluateConstant() const {
    return std::nullopt;
}

void asg::Output::accept(Visitor& visitor) const {
    visitor.visit(*this);
}

int asg::Output::getDepth() const {
    return 1 + mTerm->depth();
}

String asg::Output::getKey() const {
    return std::format("{}[{}]", mName, mTerm->key());
}

//============================================================================================================
// UnaryFunction
//============================================================================================================

std::optional<Real> asg::UnaryFunction::evaluateConstant() const {
    if (std::optional<Real> constant = mArgument->evaluateConstant()) {
        return mFunction(*constant);
    } else {
        return std::nullopt;
    }
}

void asg::UnaryFunction::accept(Visitor& visitor) const {
    visitor.visit(*this);
}

int asg::UnaryFunction::getDepth() const {
    return 1 + mArgument->depth();
}

String asg::UnaryFunction::getKey() const {
    return std::format("{:#x}({})", reinterpret_cast<uintptr_t>(mFunction), mArgument->key());
}

//============================================================================================================
// GroupOperation
//============================================================================================================

asg::GroupOperation::GroupOperation(Real                        identity,
                                    std::optional<Real>         nullElement,
                                    std::shared_ptr<const Term> constantTerm)
    : mIdentity(identity)
    , mNullElement(nullElement)
    , mConstantTerm(std::dynamic_pointer_cast<const Constant>(constantTerm)) {
    if (!mConstantTerm) {
        mConstantTerm = std::make_shared<Constant>(mIdentity);
    }
}

void asg::GroupOperation::addPositiveTerm(std::shared_ptr<const Term> term) {
    assert(term);
    assert(canBeModified());
    mPositiveTerms.push_back(std::move(term));
}

void asg::GroupOperation::addNegativeTerm(std::shared_ptr<const Term> term) {
    assert(term);
    assert(canBeModified());
    mNegativeTerms.push_back(std::move(term));
}

std::optional<Real> asg::GroupOperation::evaluateConstant() const {
    if (mPositiveTerms.empty() && mNegativeTerms.empty()) {
        return mConstantTerm->value();
    } else if (mConstantTerm->value() == mNullElement) {
        return mNullElement;
    } else {
        return std::nullopt;
    }
}

int asg::GroupOperation::getDepth() const {
    int depth = mConstantTerm->depth();
    for (const auto& term : mPositiveTerms) {
        depth = std::max(depth, term->depth());
    }
    for (const auto& term : mNegativeTerms) {
        depth = std::max(depth, term->depth());
    }
    return 1 + depth;
}

String asg::GroupOperation::getKey() const {
    const auto getKeys = [](const auto& terms) {
        std::vector<StringView> keys;
        for (const auto& term : terms) {
            keys.push_back(term->key());
        }
        std::sort(keys.begin(), keys.end());
        return keys;
    };

    String result{ mConstantTerm->key() };
    const auto [positiveSign, negativeSign] = operatorSigns();
    for (const StringView& key : getKeys(mPositiveTerms)) {
        result += std::format("{}({})", positiveSign, key);
    }
    for (const StringView& key : getKeys(mNegativeTerms)) {
        result += std::format("{}({})", negativeSign, key);
    }
    return result;
}

//============================================================================================================
// Addition
//============================================================================================================

void asg::Addition::accept(Visitor& visitor) const {
    visitor.visit(*this);
}

//============================================================================================================
// Multiplication
//============================================================================================================

void asg::Multiplication::accept(Visitor& visitor) const {
    visitor.visit(*this);
}

//============================================================================================================
// Exponentiation
//============================================================================================================

std::optional<Real> asg::Exponentiation::evaluateConstant() const {
    if (std::optional<Real> constantBase = mBase->evaluateConstant()) {
        if (constantBase == 0.0) {
            return 1.0;
        }
        if (std::optional<Real> constantExponent = mExponent->evaluateConstant()) {
            return std::pow(*constantBase, *constantExponent);
        }
    }
    return std::nullopt;
}

void asg::Exponentiation::accept(Visitor& visitor) const {
    visitor.visit(*this);
}

int asg::Exponentiation::getDepth() const {
    return 1 + std::max(mBase->depth(), mExponent->depth());
}

String asg::Exponentiation::getKey() const {
    return std::format("({})^({})", mBase->key(), mExponent->key());
}

//============================================================================================================
// Squaring
//============================================================================================================

std::optional<Real> asg::Squaring::evaluateConstant() const {
    if (std::optional<Real> constantBase = mBase->evaluateConstant()) {
        return *constantBase * *constantBase;
    }
    return std::nullopt;
}

void asg::Squaring::accept(Visitor& visitor) const {
    visitor.visit(*this);
}

int asg::Squaring::getDepth() const {
    return 1 + mBase->depth();
}

String asg::Squaring::getKey() const {
    return std::format("({})^2", mBase->key());
}

//============================================================================================================
// Transform
//============================================================================================================

std::shared_ptr<const asg::Term> asg::Transform::transform(const std::shared_ptr<const Term>& term) {

    class TransformVisitor final : Visitor {
        Transform&                  mSelf;
        std::shared_ptr<const Term> mResult;

        virtual void visit(const Sequence& term) override { mResult = mSelf.transformImpl(term); }
        virtual void visit(const Constant& term) override { mResult = mSelf.transformImpl(term); }
        virtual void visit(const Input& term) override { mResult = mSelf.transformImpl(term); }
        virtual void visit(const Output& term) override { mResult = mSelf.transformImpl(term); }
        virtual void visit(const UnaryFunction& term) override { mResult = mSelf.transformImpl(term); }
        virtual void visit(const Addition& term) override { mResult = mSelf.transformImpl(term); }
        virtual void visit(const Multiplication& term) override { mResult = mSelf.transformImpl(term); }
        virtual void visit(const Exponentiation& term) override { mResult = mSelf.transformImpl(term); }
        virtual void visit(const Squaring& term) override { mResult = mSelf.transformImpl(term); }

    public:
        explicit TransformVisitor(Transform& self)
            : mSelf(self) {}

        std::shared_ptr<const Term>&& visit(const Term& term) && {
            term.accept(*this);
            assert(mResult);
            if (term.sourceNode() && !mResult->sourceNode()) {
                const_cast<Term&>(*mResult).setSourceNode(term.sourceNode());
            }
            return std::move(mResult);
        }
    };

    assert(term);
    const auto termIt = mTransformedTerms.find(term);
    if (termIt != mTransformedTerms.end()) {
        return termIt->second;
    }
    std::shared_ptr<const Term> transformedTerm = coalesceImpl(TransformVisitor(*this).visit(*term));
    mTransformedTerms.insert({ term, transformedTerm });
    return transformedTerm;
}

SIXPACK_NAMESPACE_END
