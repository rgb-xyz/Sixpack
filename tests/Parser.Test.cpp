#include "Parser.h"
#include "Expression.h"
#include "Symbols.h"
#include "Utilities.h"
#include <cmath>
#include <iostream>
using namespace sixpack;

int main() {
    Lexicon lexicon;
    lexicon.add(std::make_shared<FunctionSymbol>("sin", RealFunction(&std::sin)));
    lexicon.add(std::make_shared<FunctionSymbol>("cos", RealFunction(&std::cos)));
    lexicon.add(std::make_shared<VariableSymbol>("x"));
    lexicon.add(std::make_shared<VariableSymbol>("y"));
    lexicon.add(std::make_shared<VariableSymbol>("z"));

    ExpressionParser parser(lexicon);
    Expression       expr = parser.parseToExpression("((x+y)*cos(z))^2");

    std::cout << "Original: " << expr.input() << std::endl;
    std::cout << "Infix:    " << stringifyExpression(expr, Notation::INFIX) << std::endl;
    std::cout << "Prefix:   " << stringifyExpression(expr, Notation::PREFIX) << std::endl;
    std::cout << "Postfix:  " << stringifyExpression(expr, Notation::POSTFIX) << std::endl;
    std::cout << std::endl;
    dumpSyntaxTree(expr, std::cout, true);
}