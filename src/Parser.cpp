#include "Parser.h"
#include "Ast.h"
#include "Compiler.h"
#include "Exception.h"
#include "Expression.h"
#include "Symbols.h"
#include "Tokenizer.h"
#include <format>

SIXPACK_NAMESPACE_BEGIN

//============================================================================================================
// ParserBase
//============================================================================================================

ParserBase::ParserBase(StringView input)
    : mTokenizer(input) {
    mNextToken = mTokenizer.getNext();
}

bool ParserBase::accept(TokenType tokenType) {
    if (mNextToken.type == tokenType) {
        mLastToken = mNextToken;
        mNextToken = mTokenizer.getNext();
        return true;
    }
    return false;
}

void ParserBase::expect(TokenType tokenType, std::optional<String> errorMessage) {
    if (!accept(tokenType)) {
        if (errorMessage) {
            fail(std::move(*errorMessage));
        } else {
            fail(std::format("Unexpected '{}'", mNextToken.text));
        }
    }
}

void ParserBase::fail(String message, std::optional<StringPosition> position) const {
    throw ParseException(std::move(message), position.value_or(mNextToken.position));
}

//============================================================================================================
// ExpressionParser
//============================================================================================================

class ExpressionParser::Impl : ParserBase {
    const Lexicon& mLexicon;

public:
    Impl(StringView input, const Lexicon& lexicon)
        : ParserBase(input)
        , mLexicon(lexicon) {}

    std::unique_ptr<ast::Node> parse() {
        std::unique_ptr<ast::Node> expression = parseL4();
        expect(TokenType::END_OF_INPUT);
        return expression;
    }

private:
    using UnaryOperatorMapping  = std::pair<TokenType, ast::UnaryOperator::Type>;
    using BinaryOperatorMapping = std::pair<TokenType, ast::BinaryOperator::Type>;

    /// Creates a new AST node.
    ///
    /// \tparam    T          The type of the AST node.
    /// \param[in] startToken The token which started the parse sequence of the node.
    /// \param[in] innerToken The token which represent the node itself.
    /// \param[in] args...    The arguments for the node's constructor.
    template <typename T, typename... TArgs>
    std::unique_ptr<ast::Node> makeNode(const Token& startToken,
                                        const Token& innerToken,
                                        TArgs&&... args) const {
        auto node = std::make_unique<T>(std::forward<TArgs>(args)...);
        node->setInnerSourceView(innerToken.text);
        node->setOuterSourceView(
            StringView(startToken.text.data(), lastToken().text.data() + lastToken().text.size()));
        return node;
    }

    /// Parses a single unary operator.
    ///
    /// We do not sequence unary operators; therefore `--x` is not a valid construct.
    ///
    /// \param[in] mapping The mapping of supported tokens to the associated AST operator node types.
    template <auto TNextParseStage>
    std::unique_ptr<ast::Node> parseUnaryOperator(std::initializer_list<UnaryOperatorMapping> mapping) {
        const Token startToken = nextToken();
        for (const auto& [tokenType, operatorType] : mapping) {
            if (accept(tokenType)) {
                std::unique_ptr<ast::Node> postfix = (this->*TNextParseStage)();
                return makeNode<ast::UnaryOperator>(startToken, startToken, operatorType, std::move(postfix));
            }
        }
        return (this->*TNextParseStage)();
    }

    /// Parses a sequence of binary operators.
    ///
    /// If multiple operators are sequenced, they are handled with left-to-right associativity; therefore
    /// `x-y-z` is treated as `(x-y)-z`.
    ///
    /// \param[in] mapping The mapping of supported tokens to the associated AST operator node types.
    template <auto TNextParseStage>
    std::unique_ptr<ast::Node> parseBinaryOperator(std::initializer_list<BinaryOperatorMapping> mapping) {
        const Token                startToken = nextToken();
        std::unique_ptr<ast::Node> prefix     = (this->*TNextParseStage)();

        const auto parseInfix = [&]() -> std::unique_ptr<ast::Node> {
            const Token innerToken = nextToken();
            for (const auto& [tokenType, operatorType] : mapping) {
                if (accept(tokenType)) {
                    std::unique_ptr<ast::Node> postfix = (this->*TNextParseStage)();
                    return makeNode<ast::BinaryOperator>(
                        startToken, innerToken, operatorType, std::move(prefix), std::move(postfix));
                }
            }
            return nullptr;
        };

        while (std::unique_ptr<ast::Node> infix = parseInfix()) {
            prefix = std::move(infix);
        }
        return prefix;
    }

    // Parse Stages
    //========================================================================================================

    /// L0 stage (highest priority) -- identifiers, functions and parentheses.
    std::unique_ptr<ast::Node> parseL0() {
        const Token startToken = nextToken();
        if (accept(TokenType::IDENTIFIER)) {
            std::shared_ptr<Symbol> symbol = mLexicon.find(startToken.text);
            if (auto valueSymbol = std::dynamic_pointer_cast<ValueSymbol>(symbol)) {
                return makeNode<ast::Value>(startToken, startToken, std::move(valueSymbol));
            }
            if (auto functionSymbol = std::dynamic_pointer_cast<FunctionSymbol>(symbol)) {
                expect(TokenType::PARENTHESIS_LEFT, "Expected '('");
                std::unique_ptr<ast::Node> argument = parseL4();
                expect(TokenType::PARENTHESIS_RIGHT, "Expected ')'");
                return makeNode<ast::UnaryFunction>(startToken,
                                                    startToken,
                                                    std::move(functionSymbol),
                                                    std::move(argument));
            }
            fail(std::format("Unknown symbol '{}'", lastToken().text), lastToken().position);
        }
        if (accept(TokenType::NUMBER)) {
            return makeNode<ast::Literal>(startToken, startToken, lastToken().numericValue);
        }
        if (accept(TokenType::PARENTHESIS_LEFT)) {
            std::unique_ptr<ast::Node> infix = parseL4();
            expect(TokenType::PARENTHESIS_RIGHT, "Expected ')'");
            infix->setOuterSourceView(
                StringView(startToken.text.data(), lastToken().text.data() + lastToken().text.size()));
            return infix;
        }
        if (accept(TokenType::BRACKET_LEFT)) {
            std::unique_ptr<ast::Node> infix = parseL4();
            expect(TokenType::BRACKET_RIGHT, "Expected ']'");
            infix->setOuterSourceView(
                StringView(startToken.text.data(), lastToken().text.data() + lastToken().text.size()));
            return infix;
        }
        fail(nextToken().type != TokenType::END_OF_INPUT ? std::format("Unexpected '{}'", nextToken().text)
                                                         : "Unexpected end of input");
    }

    /// L1 stage -- the binary `^` operator.
    std::unique_ptr<ast::Node> parseL1() {
        return parseBinaryOperator<&Impl::parseL0>(
            { BinaryOperatorMapping{ TokenType::OPERATOR_CARET, ast::BinaryOperator::Type::CARET } });
    }

    /// L2 stage -- the unary `+` and `-` operators.
    std::unique_ptr<ast::Node> parseL2() {
        return parseUnaryOperator<&Impl::parseL1>(
            { UnaryOperatorMapping{ TokenType::OPERATOR_PLUS, ast::UnaryOperator::Type::PLUS },
              UnaryOperatorMapping{ TokenType::OPERATOR_MINUS, ast::UnaryOperator::Type::MINUS } });
    }

    /// L3 stage -- the binary `*` and `/` operators.
    std::unique_ptr<ast::Node> parseL3() {
        return parseBinaryOperator<&Impl::parseL2>(
            { BinaryOperatorMapping{ TokenType::OPERATOR_ASTERISK, ast::BinaryOperator::Type::ASTERISK },
              BinaryOperatorMapping{ TokenType::OPERATOR_SLASH, ast::BinaryOperator::Type::SLASH } });
    }

    /// L4 stage (lowest priority) -- the binary `+` and `-` operators.
    std::unique_ptr<ast::Node> parseL4() {
        return parseBinaryOperator<&Impl::parseL3>(
            { BinaryOperatorMapping{ TokenType::OPERATOR_PLUS, ast::BinaryOperator::Type::PLUS },
              BinaryOperatorMapping{ TokenType::OPERATOR_MINUS, ast::BinaryOperator::Type::MINUS } });
    }
};

std::unique_ptr<ast::Node> ExpressionParser::parseToTree(StringView input) const {
    Impl impl(input, mLexicon);
    return impl.parse();
}

Expression ExpressionParser::parseToExpression(StringView input) const {
    Expression expression;
    expression.mData        = std::make_shared<Expression::Data>();
    expression.mData->input = String(input);
    try {
        expression.mData->astRoot = parseToTree(expression.mData->input);
    } catch (const ParseException& exception) {
        expression.mData->error = std::make_unique<ParseException>(exception);
    }
    return expression;
}

//============================================================================================================
// ScriptParser
//============================================================================================================

class ScriptParser::Impl : ParserBase {
    Compiler& mCompiler;

public:
    Impl(StringView input, Compiler& compiler)
        : ParserBase(input)
        , mCompiler(compiler) {}

    void parse() {
        while (accept(TokenType::IDENTIFIER)) {
            if (lastToken().text == "const") {
                expect(TokenType::IDENTIFIER);
                const StringView name = lastToken().text;
                expect(TokenType::OPERATOR_EQUALS);
                expect(TokenType::NUMBER);
                mCompiler.addConstant(name, lastToken().numericValue);
                break;
            }
            if (lastToken().text == "param") {
                expect(TokenType::IDENTIFIER);
                const StringView name  = lastToken().text;
                Real             value = 0.0;
                if (accept(TokenType::OPERATOR_EQUALS)) {
                    expect(TokenType::NUMBER);
                    value = lastToken().numericValue;
                }
                mCompiler.addParameter(name, value);
                break;
            }
            if (lastToken().text == "input") {
                expect(TokenType::IDENTIFIER);
                mCompiler.addVariable(lastToken().text);
                break;
            }
            Compiler::Visibility visibility{};
            if (lastToken().text == "output") {
                expect(TokenType::IDENTIFIER);
                visibility = Compiler::Visibility::PUBLIC;
            } else {
                visibility = Compiler::Visibility::SYMBOLIC;
            }
            const StringView name = lastToken().text;
            expect(TokenType::OPERATOR_EQUALS);
            const StringView exprString = input().substr(lastToken().position + lastToken().text.size());
            const Expression expression = mCompiler.addExpression(name, exprString, visibility);
            if (!expression) {
                fail(String(expression.error()), expression.errorPosition() + nextToken().position);
            }
            return;
        }
        expect(TokenType::END_OF_INPUT);
    }
};

void ScriptParser::parseScript(StringView input) const {
    StringPosition start = 0;
    do {
        const StringPosition next = input.find('\n', start);
        const StringView     line = input.substr(start, next - start);
        try {
            parseScriptLine(line);
        } catch (const ParseException& exception) {
            throw ParseException(exception.message(), exception.where() + start);
        }
        start = next + 1; // npos + 1 -> 0
    } while (start > 0);
}

void ScriptParser::parseScriptLine(StringView input) const {
    input = input.substr(0, input.find('#')); // truncate line comments
    Impl(input, mCompiler).parse();
}

SIXPACK_NAMESPACE_END
