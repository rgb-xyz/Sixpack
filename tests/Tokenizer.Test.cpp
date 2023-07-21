#include "Tokenizer.h"
#include <iostream>
using namespace sixpack;

std::ostream& operator<<(std::ostream& stream, const Token& token) {
    // clang-format off
    switch (token.type) {
    case TokenType::NUMBER:            stream << "NUMBER            "; break;
    case TokenType::IDENTIFIER:        stream << "IDENTIFIER        "; break;
    case TokenType::OPERATOR_PLUS:     stream << "OPERATOR_PLUS     "; break;
    case TokenType::OPERATOR_MINUS:    stream << "OPERATOR_MINUS    "; break;
    case TokenType::OPERATOR_ASTERISK: stream << "OPERATOR_ASTERISK "; break;
    case TokenType::OPERATOR_SLASH:    stream << "OPERATOR_SLASH    "; break;
    case TokenType::OPERATOR_CARET:    stream << "OPERATOR_CARET    "; break;
    case TokenType::PARENTHESIS_LEFT:  stream << "PARENTHESIS_LEFT  "; break;
    case TokenType::PARENTHESIS_RIGHT: stream << "PARENTHESIS_RIGHT "; break;
    case TokenType::BRACKET_LEFT:      stream << "BRACKET_LEFT      "; break;
    case TokenType::BRACKET_RIGHT:     stream << "BRACKET_RIGHT     "; break;
    case TokenType::UNKNOWN:           stream << "UNKNOWN           "; break;
    case TokenType::END_OF_INPUT:      stream << "END_OF_INPUT      "; break;
    default:                           stream << "???               "; break;
    }
    // clang-format on
    stream << "'" << token.text << "'";
    if (token.type == TokenType::NUMBER) {
        stream << " (" << token.numericValue << ")";
    }
    return stream;
}

void printTokens(StringView input) {
    std::cout << "Input: '" << input << "'" << std::endl;
    Tokenizer tokenizer(input);
    Token     token;
    do {
        token = tokenizer.getNext();
        std::cout << "- " << token << std::endl;
    } while (token);
    std::cout << std::endl;
}

int main() {
    printTokens("");
    printTokens("         \t   \r\n");
    printTokens("   1");
    printTokens("1   ");
    printTokens("1\t2");
    printTokens("1.0");
    printTokens("+1.0");
    printTokens("-1.0");
    printTokens("1.0.0");
    printTokens("1.0E1");
    printTokens("1.0E+1");
    printTokens("1.0E-1");
    printTokens("1.0e-1");
    printTokens("1.0f-1");
    printTokens("1.0e-1.0");
    printTokens("1.0f-1.0");
    printTokens("1.0e(1+3)");
    printTokens("]8/+def)[-1.3^*43");
    printTokens("abc123");
    printTokens("123abc");
    printTokens("123_abc");
    printTokens("_123abc");
    printTokens("sin(theta)^2*(a^2+r^2+(2*a^2*M*r*sin(theta)^2)/(r^2+a^2*cos(theta)^2))");
}
