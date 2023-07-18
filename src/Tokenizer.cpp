#include "Tokenizer.h"
#include <charconv>

SIXPACK_NAMESPACE_BEGIN

// Note: Deliberately using hard-coded version instead of "std::isspace" which is locale-dependent.
static bool isSpace(const char ch) {
    return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
}

// Note: Deliberately using hard-coded version instead of "std::isdigit" which is locale-dependent.
static bool isDigit(const char ch) {
    return ch >= '0' && ch <= '9';
}

// Note: Deliberately using hard-coded version instead of "std::isalpha" which is locale-dependent.
static bool isLetter(const char ch) {
    return ch >= 'a' && ch <= 'z' || ch >= 'A' && ch <= 'Z' || ch == '_';
}

Token Tokenizer::getNext() {
    while (mPosition < mInput.size() && isSpace(mInput[mPosition])) {
        ++mPosition;
    }
    if (mPosition >= mInput.size()) {
        return Token{};
    }
    const StringPosition startPosition = mPosition;
    const char           startChar     = mInput[mPosition];
    ++mPosition;

    const auto makeToken = [&](TokenType type, double numericValue = 0.0) {
        return Token{ .type         = type,
                      .text         = mInput.substr(startPosition, mPosition - startPosition),
                      .position     = startPosition,
                      .numericValue = numericValue };
    };

    switch (startChar) {
    case '=': return makeToken(TokenType::OPERATOR_EQUALS);
    case '+': return makeToken(TokenType::OPERATOR_PLUS);
    case '-': return makeToken(TokenType::OPERATOR_MINUS);
    case '*': return makeToken(TokenType::OPERATOR_ASTERISK);
    case '/': return makeToken(TokenType::OPERATOR_SLASH);
    case '^': return makeToken(TokenType::OPERATOR_CARET);
    case '(': return makeToken(TokenType::PARENTHESIS_LEFT);
    case ')': return makeToken(TokenType::PARENTHESIS_RIGHT);
    case '[': return makeToken(TokenType::BRACKET_LEFT);
    case ']': return makeToken(TokenType::BRACKET_RIGHT);
    default:
        if (isDigit(startChar)) {
            const char* const            startChar = mInput.data() + startPosition;
            const char* const            stopChar  = mInput.data() + mInput.size();
            double                       numericValue{};
            const std::from_chars_result result = std::from_chars(startChar, stopChar, numericValue);
            assert(result.ptr != startChar);
            mPosition = startPosition + (result.ptr - startChar);
            return makeToken(TokenType::NUMBER, numericValue);
        }
        if (isLetter(startChar)) {
            while (mPosition < mInput.size() && (isLetter(mInput[mPosition]) || isDigit(mInput[mPosition]))) {
                ++mPosition;
            }
            return makeToken(TokenType::IDENTIFIER);
        }
        return makeToken(TokenType::UNKNOWN);
    }
}

SIXPACK_NAMESPACE_END
