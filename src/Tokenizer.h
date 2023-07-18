#pragma once
#include "Common.h"

SIXPACK_NAMESPACE_BEGIN

enum class TokenType {
    NUMBER,            // 0, 1.23, 3.46e+4, ...
    IDENTIFIER,        // A, ab_c, _abc3, ...
    OPERATOR_EQUALS,   // =
    OPERATOR_PLUS,     // +
    OPERATOR_MINUS,    // -
    OPERATOR_ASTERISK, // *
    OPERATOR_SLASH,    // /
    OPERATOR_CARET,    // ^
    PARENTHESIS_LEFT,  // (
    PARENTHESIS_RIGHT, // )
    BRACKET_LEFT,      // [
    BRACKET_RIGHT,     // ]
    UNKNOWN,           // (anything else)
    END_OF_INPUT       // (end of input)
};

struct Token {
    TokenType      type = TokenType::END_OF_INPUT;
    StringView     text;
    StringPosition position = String::npos;
    double         numericValue; // type == TokenType::NUMBER

    explicit operator bool() const { return type != TokenType::END_OF_INPUT; }
};

class Tokenizer {
    const StringView mInput;
    StringPosition   mPosition = 0;

public:
    explicit Tokenizer(StringView input)
        : mInput(input) {}

    StringView     input() const { return mInput; }
    StringPosition position() const { return mPosition; }

    Token getNext();
};

SIXPACK_NAMESPACE_END
