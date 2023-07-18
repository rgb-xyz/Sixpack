#pragma once
#include "Common.h"
#include <array>
#include <memory>
#include <span>

SIXPACK_NAMESPACE_BEGIN

class FunctionSymbol;
class ValueSymbol;

/// Abstract Syntax Tree (AST)
namespace ast {

    class Visitor;

    /// The abstract base class of an AST node.
    class Node {
        StringView mInnerSourceView;
        StringView mOuterSourceView;

    public:
        virtual ~Node() = default;

        StringView innerSourceView() const { return mInnerSourceView; }
        StringView outerSourceView() const { return mOuterSourceView; }

        void setInnerSourceView(StringView innerSourceView) { mInnerSourceView = innerSourceView; }
        void setOuterSourceView(StringView outerSourceView) { mOuterSourceView = outerSourceView; }

        virtual std::span<const std::unique_ptr<Node>> children() const = 0;

        virtual void accept(Visitor& visitor) const = 0;
    };

    // Node Valence Categories
    //========================================================================================================

    /// The base class of an invariadic AST node, i.e. a node with a fixed valency (number of children).
    template <int TValency>
    class InvariadicNode : public Node {
        const std::array<std::unique_ptr<Node>, TValency> mChildren;

    public:
        template <typename... TChildren>
        InvariadicNode(TChildren&&... children)
            : mChildren{ std::forward<TChildren>(children)... } {
            static_assert(sizeof...(TChildren) == TValency);
            assert(std::find(mChildren.begin(), mChildren.end(), std::unique_ptr<Node>{}) == mChildren.end());
        }

        virtual std::span<const std::unique_ptr<Node>> children() const override final { return mChildren; }
    };

    /// A 0-adic AST node, i.e. a leaf.
    class NiladicNode : public InvariadicNode<0> {
    public:
        virtual void accept(Visitor& visitor) const override;
    };

    /// A 1-adic AST node, e.g. an unary operator or a 1-argument function.
    class MonadicNode : public InvariadicNode<1> {
    public:
        explicit MonadicNode(std::unique_ptr<Node> child)
            : InvariadicNode<1>(std::move(child)) {}

        virtual void accept(Visitor& visitor) const override;
    };

    /// A 2-adic AST node, e.g. a binary operator.
    class DyadicNode : public InvariadicNode<2> {
    public:
        DyadicNode(std::unique_ptr<Node> child1, std::unique_ptr<Node> child2)
            : InvariadicNode<2>(std::move(child1), std::move(child2)) {}

        virtual void accept(Visitor& visitor) const override;
    };

    // Niladic Nodes
    //========================================================================================================

    /// The AST node representing a literal (direct value).
    class Literal final : public NiladicNode {
        const Real mValue;

    public:
        explicit Literal(Real value)
            : mValue(value) {}

        Real value() const { return mValue; }

        virtual void accept(Visitor& visitor) const override;
    };

    /// The AST node representing a named value (constant, parameter or variable).
    class Value final : public NiladicNode {
        const std::shared_ptr<ValueSymbol> mValueSymbol;

    public:
        explicit Value(std::shared_ptr<ValueSymbol> valueSymbol)
            : mValueSymbol(std::move(valueSymbol)) {
            assert(mValueSymbol);
        }

        const ValueSymbol& valueSymbol() const { return *mValueSymbol; }

        virtual void accept(Visitor& visitor) const override;
    };

    // Monadic Nodes
    //========================================================================================================

    /// The AST node representing a call to a unary (named) function.
    class UnaryFunction final : public MonadicNode {
        const std::shared_ptr<FunctionSymbol> mFunctionSymbol;

    public:
        UnaryFunction(std::shared_ptr<FunctionSymbol> functionSymbol, std::unique_ptr<Node> argument)
            : MonadicNode(std::move(argument))
            , mFunctionSymbol(std::move(functionSymbol)) {
            assert(mFunctionSymbol);
        }

        const FunctionSymbol& functionSymbol() const { return *mFunctionSymbol; }
        const Node&           argument() const { return *children()[0]; }

        virtual void accept(Visitor& visitor) const override;
    };

    /// The AST node representing a unary operator.
    class UnaryOperator final : public MonadicNode {
    public:
        enum class Type {
            PLUS, ///< The plus operator `+X` (i.e. the identity).
            MINUS ///< The minus operator `-Y` (i.e. the negation).
        };

    private:
        const Type mType;

    public:
        UnaryOperator(Type type, std::unique_ptr<Node> operand)
            : MonadicNode(std::move(operand))
            , mType(type) {}

        Type        type() const { return mType; }
        const Node& operand() const { return *children()[0]; }

        virtual void accept(Visitor& visitor) const override;
    };

    // Dyadic Nodes
    //========================================================================================================

    /// The AST node representing a binary operator.
    class BinaryOperator : public DyadicNode {
    public:
        enum class Type {
            PLUS,     ///< The plus operator `X+Y` (i.e. the addition).
            MINUS,    ///< The minus operator `X-Y` (i.e. the subtraction).
            ASTERISK, ///< The asterisk operator `X*Y` (i.e. the multiplication).
            SLASH,    ///< The slash operator `X/Y` (i.e. the division).
            CARET     ///< The caret operator `X^Y` (i.e. the exponentiation).
        };

    private:
        const Type mType;

    public:
        BinaryOperator(Type type, std::unique_ptr<Node> leftOperand, std::unique_ptr<Node> rightOperand)
            : DyadicNode(std::move(leftOperand), std::move(rightOperand))
            , mType(type) {}

        Type        type() const { return mType; }
        const Node& leftOperand() const { return *children()[0]; }
        const Node& rightOperand() const { return *children()[1]; }

        virtual void accept(Visitor& visitor) const override;
    };

    // Visitor
    //========================================================================================================

    class Visitor {
    public:
        virtual ~Visitor() = default;

        virtual void visit(const Node& node) = 0;

        // Niladic Nodes
        virtual void visit(const NiladicNode& node) { visit(static_cast<const Node&>(node)); }
        virtual void visit(const Literal& node) { visit(static_cast<const NiladicNode&>(node)); }
        virtual void visit(const Value& node) { visit(static_cast<const NiladicNode&>(node)); }

        // Monadic Nodes
        virtual void visit(const MonadicNode& node) { visit(static_cast<const Node&>(node)); }
        virtual void visit(const UnaryFunction& node) { visit(static_cast<const MonadicNode&>(node)); }
        virtual void visit(const UnaryOperator& node) { visit(static_cast<const MonadicNode&>(node)); }

        // Dyadic Nodes
        virtual void visit(const DyadicNode& node) { visit(static_cast<const Node&>(node)); }
        virtual void visit(const BinaryOperator& node) { visit(static_cast<const DyadicNode&>(node)); }
    };

} // namespace ast

SIXPACK_NAMESPACE_END
