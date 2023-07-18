#pragma once
#include "Tokenizer.h"
#include <memory>
#include <optional>

SIXPACK_NAMESPACE_BEGIN

namespace ast {
    class Node;
}
class Compiler;
class Expression;
class Lexicon;

class ParserBase {
    Tokenizer mTokenizer;
    Token     mNextToken;
    Token     mLastToken;

protected:
    explicit ParserBase(StringView input);

    StringView   input() const { return mTokenizer.input(); }
    const Token& nextToken() const { return mNextToken; }
    const Token& lastToken() const { return mLastToken; }

    /// Accepts the token of the given type.
    ///
    /// If the type of the next token matches \a tokenType, the input is advanced to the next token and the
    /// function returns `true`. Otherwise, the function returns `false` without any advancement.
    bool accept(TokenType tokenType);

    /// Expects the token of the given type.
    ///
    /// If the type of the next token matches \a tokenType, the input is advanced to the next token.
    /// Otherwise, an "unexpected token" parse exception is thrown.
    ///
    /// \param[in] errorMessage An optional error message to be reported upon failure. If not specified, a
    ///                         generic message is used instead.
    void expect(TokenType tokenType, std::optional<String> errorMessage = std::nullopt);

    /// Reports the parsing failure by throwing ParseException.
    ///
    /// \param[in] message  The message describing the parsing failure.
    /// \param[in] position An optional parameter specifying the parsing position of the failure. If not
    ///                     specified, the position of the next token is used.
    ///
    /// The thrown exception will contain the position of the next token as a place where the error occurred.
    [[noreturn]] void fail(String message, std::optional<StringPosition> position = std::nullopt) const;
};

class ExpressionParser {
    class Impl;
    const Lexicon& mLexicon;

public:
    explicit ExpressionParser(const Lexicon& lexicon)
        : mLexicon(lexicon) {}

    std::unique_ptr<ast::Node> parseToTree(StringView input) const;
    Expression                 parseToExpression(StringView input) const;
};

class ScriptParser {
    class Impl;
    Compiler& mCompiler;

public:
    explicit ScriptParser(Compiler& compiler)
        : mCompiler(compiler) {}

    void parseScript(StringView input) const;
    void parseScriptLine(StringView input) const;
};

SIXPACK_NAMESPACE_END
