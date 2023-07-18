#pragma once
#include "Common.h"

SIXPACK_NAMESPACE_BEGIN

namespace ast {
    class Node;
}
namespace asg {
    class Term;
}
class Expression;
class Program;

enum class Notation {
    INFIX,  ///< The infix (algebraic) notation.
    PREFIX, ///< The prefix (Polish) notation.
    POSTFIX ///< The postfix (reverse Polish) notation.
};

String stringifyExpression(const ast::Node& root, Notation notation);
String stringifyExpression(const Expression& expression, Notation notation);

void dumpSyntaxTree(const ast::Node& root, std::ostream& output, StringView sourceView = {});
void dumpSyntaxTree(const Expression& expression, std::ostream& output, bool includeSourceView = false);

void dumpSemanticGraph(const asg::Term& root, std::ostream& output);

void dumpProgram(const Program& program, std::ostream& output);

SIXPACK_NAMESPACE_END
