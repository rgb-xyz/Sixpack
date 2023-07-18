#pragma once
#include "Common.h"
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

SIXPACK_NAMESPACE_BEGIN

namespace ast {
    class Node;
}

/// Abstract Semantic Graph (ASG)
namespace asg {

    class Visitor;

    /// The abstract base class of an ASG term.
    class Term : public std::enable_shared_from_this<Term> {
        mutable std::optional<int>    mDepth;
        mutable std::optional<String> mKey;
        const ast::Node*              mSourceNode = nullptr;

    public:
        virtual ~Term() = default;

        int        depth() const;
        StringView key() const;

        const ast::Node* sourceNode() const { return mSourceNode; }
        void             setSourceNode(const ast::Node* sourceNode) { mSourceNode = sourceNode; }

        virtual std::optional<Real> evaluateConstant() const = 0;

        virtual void accept(Visitor& visitor) const = 0;

    protected:
        virtual int    getDepth() const = 0;
        virtual String getKey() const   = 0;
        bool           canBeModified() const;
    };

    class Sequence final : public Term {
        std::vector<std::shared_ptr<const Term>> mTerms;

    public:
        const std::vector<std::shared_ptr<const Term>>& terms() const { return mTerms; }

        void addTerm(std::shared_ptr<const Term> term);

        virtual std::optional<Real> evaluateConstant() const override;

        virtual void accept(Visitor& visitor) const override;

    protected:
        virtual int    getDepth() const override;
        virtual String getKey() const override;
    };

    // Terminals
    //========================================================================================================

    class Constant final : public Term {
        const Real mValue;

    public:
        explicit Constant(Real value)
            : mValue(value == 0.0 ? 0.0 : value) {} // convert -0 to +0

        Real value() const { return mValue; }

        virtual std::optional<Real> evaluateConstant() const override { return mValue; }

        virtual void accept(Visitor& visitor) const override;

    private:
        virtual int    getDepth() const override;
        virtual String getKey() const override;
    };

    class Input final : public Term {
        const String mName;

    public:
        explicit Input(StringView name)
            : mName(name) {}

        StringView name() const { return mName; }

        virtual std::optional<Real> evaluateConstant() const override { return std::nullopt; }

        virtual void accept(Visitor& visitor) const override;

    private:
        virtual int    getDepth() const override;
        virtual String getKey() const override;
    };

    class Output final : public Term {
        const String                mName;
        std::shared_ptr<const Term> mTerm;

    public:
        Output(StringView name, std::shared_ptr<const Term> term)
            : mName(std::move(name))
            , mTerm(std::move(term)) {}

        StringView                         name() const { return mName; }
        const std::shared_ptr<const Term>& term() const { return mTerm; }

        virtual std::optional<Real> evaluateConstant() const override;

        virtual void accept(Visitor& visitor) const override;

    private:
        virtual int    getDepth() const override;
        virtual String getKey() const override;
    };

    class UnaryFunction final : public Term {
        const RealFunction                mFunction;
        const std::shared_ptr<const Term> mArgument;

    public:
        explicit UnaryFunction(RealFunction function, std::shared_ptr<const Term> argument)
            : mFunction(function)
            , mArgument(std::move(argument)) {
            assert(mFunction);
            assert(mArgument);
        }

        RealFunction                       function() const { return mFunction; }
        const std::shared_ptr<const Term>& argument() const { return mArgument; }

        virtual std::optional<Real> evaluateConstant() const override;

        virtual void accept(Visitor& visitor) const override;

    private:
        virtual int    getDepth() const override;
        virtual String getKey() const override;
    };

    // (Abelian) Group Operations
    //========================================================================================================

    class GroupOperation : public Term {
        const Real                mIdentity;
        const std::optional<Real> mNullElement;

        std::shared_ptr<const Constant>          mConstantTerm;
        std::vector<std::shared_ptr<const Term>> mPositiveTerms;
        std::vector<std::shared_ptr<const Term>> mNegativeTerms;

    protected:
        GroupOperation(Real                        identity,
                       std::optional<Real>         nullElement,
                       std::shared_ptr<const Term> constantTerm = {});

    public:
        const std::shared_ptr<const Constant>&          constantTerm() const { return mConstantTerm; }
        const std::vector<std::shared_ptr<const Term>>& positiveTerms() const { return mPositiveTerms; }
        const std::vector<std::shared_ptr<const Term>>& negativeTerms() const { return mNegativeTerms; }

        void addPositiveTerm(std::shared_ptr<const Term> term);
        void addNegativeTerm(std::shared_ptr<const Term> term);

        Real                       identity() const { return mIdentity; }
        const std::optional<Real>& nullElement() const { return mNullElement; }

        virtual std::optional<Real> evaluateConstant() const override;

        virtual Real                              apply(Real left, Real right) const        = 0;
        virtual Real                              applyInverse(Real left, Real right) const = 0;
        virtual std::pair<StringView, StringView> operatorSigns() const                     = 0;

    private:
        virtual int    getDepth() const override;
        virtual String getKey() const override;
    };

    class Addition final : public GroupOperation {
    public:
        explicit Addition(std::shared_ptr<const Term> constantTerm = {})
            : GroupOperation(0.0, std::nullopt, std::move(constantTerm)) {}

        virtual void accept(Visitor& visitor) const override;

        virtual Real apply(Real left, Real right) const override { return left + right; }
        virtual Real applyInverse(Real left, Real right) const override { return left - right; }
        virtual std::pair<StringView, StringView> operatorSigns() const override { return { "+", "-" }; }
    };

    class Multiplication final : public GroupOperation {
    public:
        explicit Multiplication(std::shared_ptr<const Term> constantTerm = {})
            : GroupOperation(1.0, 0.0, std::move(constantTerm)) {}

        virtual void accept(Visitor& visitor) const override;

        virtual Real apply(Real left, Real right) const override { return left * right; }
        virtual Real applyInverse(Real left, Real right) const override { return left / right; }
        virtual std::pair<StringView, StringView> operatorSigns() const override { return { "*", "/" }; }
    };

    // Power Operations
    //========================================================================================================

    class Exponentiation final : public Term {
        std::shared_ptr<const Term> mBase;
        std::shared_ptr<const Term> mExponent;

    public:
        Exponentiation(std::shared_ptr<const Term> base, std::shared_ptr<const Term> exponent)
            : mBase(std::move(base))
            , mExponent(std::move(exponent)) {}

        const std::shared_ptr<const Term>& base() const { return mBase; }
        const std::shared_ptr<const Term>& exponent() const { return mExponent; }

        virtual std::optional<Real> evaluateConstant() const override;

        virtual void accept(Visitor& visitor) const override;

    private:
        virtual int    getDepth() const override;
        virtual String getKey() const override;
    };

    class Squaring final : public Term {
        std::shared_ptr<const Term> mBase;

    public:
        explicit Squaring(std::shared_ptr<const Term> base)
            : mBase(std::move(base)) {}

        const std::shared_ptr<const Term>& base() const { return mBase; }

        virtual std::optional<Real> evaluateConstant() const override;

        virtual void accept(Visitor& visitor) const override;

    private:
        virtual int    getDepth() const override;
        virtual String getKey() const override;
    };

    // Visitors & Transforms
    //========================================================================================================

    class Visitor {
    public:
        virtual ~Visitor() = default;

        virtual void visit(const Sequence& term)       = 0;
        virtual void visit(const Constant& term)       = 0;
        virtual void visit(const Input& term)          = 0;
        virtual void visit(const Output& term)         = 0;
        virtual void visit(const UnaryFunction& term)  = 0;
        virtual void visit(const Addition& term)       = 0;
        virtual void visit(const Multiplication& term) = 0;
        virtual void visit(const Exponentiation& term) = 0;
        virtual void visit(const Squaring& term)       = 0;
    };

    class Transform {
        std::unordered_map<std::shared_ptr<const Term>, std::shared_ptr<const Term>> mTransformedTerms;

    public:
        std::shared_ptr<const Term> transform(const std::shared_ptr<const Term>& term);

    private:
        virtual std::shared_ptr<const Term> transformImpl(const Sequence& term)       = 0;
        virtual std::shared_ptr<const Term> transformImpl(const Constant& term)       = 0;
        virtual std::shared_ptr<const Term> transformImpl(const Input& term)          = 0;
        virtual std::shared_ptr<const Term> transformImpl(const Output& term)         = 0;
        virtual std::shared_ptr<const Term> transformImpl(const UnaryFunction& term)  = 0;
        virtual std::shared_ptr<const Term> transformImpl(const Addition& term)       = 0;
        virtual std::shared_ptr<const Term> transformImpl(const Multiplication& term) = 0;
        virtual std::shared_ptr<const Term> transformImpl(const Exponentiation& term) = 0;
        virtual std::shared_ptr<const Term> transformImpl(const Squaring& term)       = 0;

        virtual std::shared_ptr<const Term> coalesceImpl(std::shared_ptr<const Term> term) = 0;
    };

} // namespace asg

SIXPACK_NAMESPACE_END
