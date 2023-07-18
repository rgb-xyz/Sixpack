#pragma once
#include "Expression.h"
#include <unordered_map>

SIXPACK_NAMESPACE_BEGIN

// Named Symbols
//============================================================================================================

class Symbol {
    String mName;

public:
    explicit Symbol(StringView name)
        : mName(name) {}
    virtual ~Symbol() = default;

    const String& name() const { return mName; }
};

class ValueSymbol : public Symbol {
public:
    using Symbol::Symbol;
};

class ConstantSymbol final : public ValueSymbol {
    Real mValue;

public:
    ConstantSymbol(StringView name, Real value)
        : ValueSymbol(name)
        , mValue(value) {}

    Real value() const { return mValue; }
};

class ParameterSymbol final : public ValueSymbol {
    Real mValue;

public:
    ParameterSymbol(StringView name, Real value)
        : ValueSymbol(name)
        , mValue(value) {}

    Real value() const { return mValue; }
    void setValue(Real value) { mValue = value; }
};

class VariableSymbol final : public ValueSymbol {
public:
    using ValueSymbol::ValueSymbol;
};

class ExpressionSymbol final : public ValueSymbol {
    Expression mExpression;

public:
    ExpressionSymbol(StringView name, Expression expression)
        : ValueSymbol(name)
        , mExpression(expression) {}

    Expression expression() const { return mExpression; }
};

class FunctionSymbol final : public Symbol {
    RealFunction mFunction;

public:
    FunctionSymbol(StringView name, RealFunction function)
        : Symbol(name)
        , mFunction(function) {
        assert(mFunction);
    }

    RealFunction function() const { return mFunction; }
};

// Lexicon (Symbol Table)
//============================================================================================================

class Lexicon {
    std::unordered_map<StringView, std::shared_ptr<Symbol>> mSymbols;

public:
    const std::unordered_map<StringView, std::shared_ptr<Symbol>>& symbols() const { return mSymbols; }

    /// Adds the given symbol into the lexicon.
    ///
    /// \param[in] symbol The symbol to add. Must not be nullptr.
    ///
    /// \throws Exception if another symbol with the same name is already present.
    void add(std::shared_ptr<Symbol> symbol);

    /// Finds a symbol in lexicon matching the given name.
    ///
    /// \returns The matching symbol, or nullptr.
    std::shared_ptr<Symbol> find(StringView name) const;
};

SIXPACK_NAMESPACE_END
