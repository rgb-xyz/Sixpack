#pragma once
#include "Common.h"
#include <memory>
#include <vector>

SIXPACK_NAMESPACE_BEGIN

namespace asg {
    class Term;
}
class Expression;
class Program;

class Compiler {
    class Context;
    const std::unique_ptr<Context> mContext;

public:
    Compiler();
    ~Compiler();

    void addConstant(StringView name, Real value);
    void addFunction(StringView name, RealFunction function);

    void addParameter(StringView name, Real value);
    void addVariable(StringView name);

    enum class Visibility {
        PUBLIC,
        PRIVATE,
        SYMBOLIC,
    };

    Expression addExpression(StringView name, StringView expression, Visibility visibility);

    void addSourceScript(StringView input);

    std::vector<StringView>                        getInputs() const;
    std::vector<std::pair<StringView, Real>>       getParameters() const;
    std::vector<std::pair<StringView, Expression>> getOutputs() const;

    Program compile() const;

    // Internals

    std::shared_ptr<const asg::Term> makeGraph() const;
    Program                          compileGraph(const asg::Term& graph) const;
};

SIXPACK_NAMESPACE_END
