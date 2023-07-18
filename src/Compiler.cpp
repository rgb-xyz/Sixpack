#include "Compiler.h"
#include "AsgTransforms.h"
#include "Ast.h"
#include "Exception.h"
#include "Parser.h"
#include "Program.h"
#include "Symbols.h"
#include <algorithm>
#include <format>
#include <unordered_set>

SIXPACK_NAMESPACE_BEGIN

namespace {

    //========================================================================================================
    // GraphBuilder
    //========================================================================================================

    class GraphBuilder final : ast::Visitor {
        std::vector<std::shared_ptr<asg::Term>>   mTerms;
        std::vector<std::shared_ptr<asg::Output>> mOutputs;

    public:
        void addOutput(StringView name, const Expression& expression) {
            assert(mTerms.empty());
            expression.visitAst(*this);
            assert(mTerms.size() == 1);
            mOutputs.push_back(std::make_shared<asg::Output>(name, popTerm()));
        }

        std::shared_ptr<asg::Term> makeGraph() const {
            auto root = std::make_shared<asg::Sequence>();
            for (const auto& output : mOutputs) {
                root->addTerm(output);
            }
            return root;
        }

    private:
        void pushTerm(std::shared_ptr<asg::Term> term) {
            assert(term);
            mTerms.push_back(std::move(term));
        }

        const std::shared_ptr<asg::Term>& lastTerm() const {
            assert(!mTerms.empty());
            return mTerms.back();
        }

        std::shared_ptr<asg::Term> popTerm() {
            std::shared_ptr<asg::Term> term = lastTerm();
            mTerms.pop_back();
            return term;
        }

        // Visitor Interface

        virtual void visit(const ast::Node& node) override { throw Exception("Unhandled node category."); }

        virtual void visit(const ast::Literal& node) override {
            pushTerm(std::make_shared<asg::Constant>(node.value()));
        }

        virtual void visit(const ast::Value& node) override {
            if (const auto* constant = dynamic_cast<const ConstantSymbol*>(&node.valueSymbol())) {
                pushTerm(std::make_shared<asg::Constant>(constant->value()));
            } else if (const auto* parameter = dynamic_cast<const ParameterSymbol*>(&node.valueSymbol())) {
                pushTerm(std::make_shared<asg::Constant>(parameter->value()));
            } else if (const auto* variable = dynamic_cast<const VariableSymbol*>(&node.valueSymbol())) {
                pushTerm(std::make_shared<asg::Input>(variable->name()));
            } else if (const auto* expression = dynamic_cast<const ExpressionSymbol*>(&node.valueSymbol())) {
                expression->expression().visitAst(*this);
            } else {
                throw Exception("Unhandled value symbol type.");
            }
            lastTerm()->setSourceNode(&node);
        }

        virtual void visit(const ast::UnaryFunction& node) override {
            node.argument().accept(*this);
            std::shared_ptr<asg::Term> term = popTerm();
            pushTerm(std::make_shared<asg::UnaryFunction>(node.functionSymbol().function(), std::move(term)));
            lastTerm()->setSourceNode(&node);
        }

        virtual void visit(const ast::UnaryOperator& node) override {
            node.operand().accept(*this);
            std::shared_ptr<asg::Term> term = popTerm();
            switch (node.type()) {
            case ast::UnaryOperator::Type::PLUS: pushTerm(term); break;
            case ast::UnaryOperator::Type::MINUS: {
                // Note: Let's represent the negation as "-1*x" rather than as "0-x".
                auto negation = std::make_shared<asg::Multiplication>(std::make_shared<asg::Constant>(-1.0));
                negation->addPositiveTerm(std::move(term));
                pushTerm(std::move(negation));
            } break;
            default: throw Exception("Unhandled unary operator type.");
            }
            lastTerm()->setSourceNode(&node);
        }

        virtual void visit(const ast::BinaryOperator& node) override {
            node.leftOperand().accept(*this);
            node.rightOperand().accept(*this);
            std::shared_ptr<asg::Term> rightTerm = popTerm();
            std::shared_ptr<asg::Term> leftTerm  = popTerm();
            switch (node.type()) {
            case ast::BinaryOperator::Type::PLUS: {
                auto operation = std::make_shared<asg::Addition>();
                operation->addPositiveTerm(std::move(leftTerm));
                operation->addPositiveTerm(std::move(rightTerm));
                pushTerm(std::move(operation));
            } break;
            case ast::BinaryOperator::Type::MINUS: {
                auto operation = std::make_shared<asg::Addition>();
                operation->addPositiveTerm(std::move(leftTerm));
                operation->addNegativeTerm(std::move(rightTerm));
                pushTerm(std::move(operation));
            } break;
            case ast::BinaryOperator::Type::ASTERISK: {
                auto operation = std::make_shared<asg::Multiplication>();
                operation->addPositiveTerm(std::move(leftTerm));
                operation->addPositiveTerm(std::move(rightTerm));
                pushTerm(std::move(operation));
            } break;
            case ast::BinaryOperator::Type::SLASH: {
                auto operation = std::make_shared<asg::Multiplication>();
                operation->addPositiveTerm(std::move(leftTerm));
                operation->addNegativeTerm(std::move(rightTerm));
                pushTerm(std::move(operation));
            } break;
            case ast::BinaryOperator::Type::CARET:
                pushTerm(std::make_shared<asg::Exponentiation>(std::move(leftTerm), std::move(rightTerm)));
                break;
            default: throw Exception("Unhandled binary operator type.");
            }
            lastTerm()->setSourceNode(&node);
        }
    };

    //========================================================================================================
    // CodeGenerator
    //========================================================================================================

    class CodeGenerator final : public asg::Visitor {
        std::unordered_set<const asg::Term*>                   mUniqueTerms;
        std::vector<std::vector<const asg::Term*>>             mTermLevels;
        Program::Variables                                     mInputs;
        Program::Variables                                     mOutputs;
        Program::Constants                                     mConstants;
        Program::Instructions                                  mInstructions;
        Program::Comments                                      mComments;
        std::unordered_map<const asg::Term*, Program::Address> mMemoryMapping;

    public:
        explicit CodeGenerator(const asg::Term& graphRoot) { graphRoot.accept(*this); }

        Program generate(const Lexicon& publicSymbols) {
            mInputs        = {};
            mOutputs       = {};
            mConstants     = {};
            mInstructions  = {};
            mComments      = {};
            mMemoryMapping = {};
            addComment(Program::SCRATCHPAD_ADDRESS, "scratch-pad");
            for (int level = 0; level < mTermLevels.size(); ++level) {
                std::stable_sort(mTermLevels[level].begin(),
                                 mTermLevels[level].end(),
                                 [](const auto& t1, const auto& t2) {
                                     return typeid(*t1).before(typeid(*t2));
                                 });
                if (level == 0) {
                    generateDataSection(mTermLevels[level]);
                } else {
                    generateCodeSection(mTermLevels[level]);
                }
            }
            generateIntrinsics();
            // Map unused variables to scratch-pad.
            for (const auto& [name, symbol] : publicSymbols.symbols()) {
                if (const auto* variable = dynamic_cast<const VariableSymbol*>(symbol.get())) {
                    // Note: Will insert only if not already present.
                    if (mInputs.insert({ variable->name(), Program::SCRATCHPAD_ADDRESS }).second) {
                        addComment(Program::SCRATCHPAD_ADDRESS, std::format("'{}'", variable->name()));
                    }
                }
            }
            return Program(std::move(mInputs),
                           std::move(mOutputs),
                           std::move(mConstants),
                           std::move(mInstructions),
                           std::move(mComments));
        }

    private:
        void mapToMemory(const asg::Term* term, Program::Address address) {
            if (!mMemoryMapping.insert({ term, address }).second) {
                throw CompileException("Code generation failed -- ambiguous memory mapping");
            }
            if (const auto* output = dynamic_cast<const asg::Output*>(term)) {
                addComment(address, std::format("'{}'", output->name()));
            } else if (term->sourceNode()) {
                addComment(address, std::format("'{}'", term->sourceNode()->outerSourceView()));
            }
        }

        Program::Address getAddress(const asg::Term* term) const {
            const auto addressIt = mMemoryMapping.find(term);
            if (addressIt == mMemoryMapping.end()) {
                throw CompileException("Code generation failed -- missing memory mapping");
            }
            return addressIt->second;
        }

        void addComment(Program::Address address, StringView comment) {
            String& addressComment = mComments[address];
            if (!addressComment.empty()) {
                addressComment += ", ";
            }
            addressComment += comment;
        }

        Program::Address emitInstruction(Program::Opcode             opcode,
                                         const Program::Instruction& operands,
                                         const asg::Term*            emitter = nullptr) {
            Program::Instruction instruction = operands;
            instruction.opcode               = opcode;
            const auto instructionIt =
                std::find(mInstructions.instructions.begin(), mInstructions.instructions.end(), instruction);
            const auto address = mInstructions.memoryOffset +
                                 Program::Address(instructionIt - mInstructions.instructions.begin());
            if (instructionIt == mInstructions.instructions.end()) {
                mInstructions.instructions.push_back(instruction);
            }
            if (emitter) {
                mapToMemory(emitter, address);
            }
            return address;
        }

        void emitGroupOperationSequence(const asg::GroupOperation& operation,
                                        Program::Opcode            initialPositiveOp,
                                        Program::Opcode            sequentialPositiveOp,
                                        Program::Opcode            initialNegativeOp,
                                        Program::Opcode            sequentialNegativeOp) {
            std::optional<Program::Address> lastAddress;
            std::optional<Program::Opcode>  pendingOperation;
            const double                    constant      = operation.constantTerm()->value();
            const bool                      needsConstant = constant != operation.identity();
            for (const auto& term : operation.positiveTerms()) {
                const Program::Address address = getAddress(term.get());
                if (lastAddress) {
                    lastAddress      = emitInstruction(sequentialPositiveOp,
                                                       { .operand1 = *lastAddress, .operand2 = address });
                    pendingOperation = std::nullopt;
                } else if (needsConstant) {
                    lastAddress =
                        emitInstruction(initialPositiveOp, { .immediate = constant, .operand2 = address });
                } else {
                    lastAddress      = address;
                    pendingOperation = initialPositiveOp;
                }
            }
            for (const auto& term : operation.negativeTerms()) {
                const Program::Address address = getAddress(term.get());
                if (lastAddress) {
                    lastAddress      = emitInstruction(sequentialNegativeOp,
                                                       { .operand1 = *lastAddress, .operand2 = address });
                    pendingOperation = std::nullopt;
                } else if (needsConstant) {
                    lastAddress =
                        emitInstruction(initialNegativeOp, { .immediate = constant, .operand2 = address });
                } else {
                    lastAddress      = address;
                    pendingOperation = initialNegativeOp;
                }
            }
            assert(lastAddress);
            if (pendingOperation) {
                lastAddress =
                    emitInstruction(*pendingOperation, { .immediate = constant, .operand2 = *lastAddress });
            }
            mapToMemory(&operation, *lastAddress);
        }

        void generateDataSection(const std::vector<const asg::Term*>& terms) {
            Program::Address constantCount = 0;
            Program::Address variableCount = 0;
            for (const auto& term : terms) {
                if (dynamic_cast<const asg::Constant*>(term)) {
                    ++constantCount;
                } else if (dynamic_cast<const asg::Input*>(term)) {
                    ++variableCount;
                } else {
                    throw CompileException("Code generation failed -- code present in the data section");
                }
            }
            const Program::Address variableSection = 1;
            const Program::Address constantSection = variableSection + variableCount;
            const Program::Address codeSection     = constantSection + constantCount;
            for (const auto& term : terms) {
                if (const auto* constant = dynamic_cast<const asg::Constant*>(term)) {
                    auto address = Program::Address(constantSection + mConstants.values.size());
                    mConstants.values.push_back(constant->value());
                    if (!mComments.contains(address)) {
                        addComment(address, "constant");
                    }
                    mapToMemory(constant, address);
                } else if (const auto* input = dynamic_cast<const asg::Input*>(term)) {
                    auto address = Program::Address(variableSection + mInputs.size());
                    address      = mInputs.insert({ String(input->name()), address }).first->second;
                    if (!mComments.contains(address)) {
                        addComment(address, "input");
                    }
                    mapToMemory(input, address);
                }
            }
            mConstants.memoryOffset    = constantSection;
            mInstructions.memoryOffset = codeSection;
        }

        void generateCodeSection(const std::vector<const asg::Term*>& terms) {
            for (const auto& term : terms) {
                if (const auto* output = dynamic_cast<const asg::Output*>(term)) {
                    const Program::Address address = getAddress(output->term().get());
                    mOutputs.insert({ String(output->name()), address });
                    mapToMemory(output, address);
                } else if (const auto* operation = dynamic_cast<const asg::UnaryFunction*>(term)) {
                    emitInstruction(Program::Opcode::CALL,
                                    { .function = operation->function(),
                                      .operand2 = getAddress(operation->argument().get()) },
                                    operation);
                } else if (const auto* operation = dynamic_cast<const asg::Addition*>(term)) {
                    emitGroupOperationSequence(*operation,
                                               Program::Opcode::ADD_IMM,
                                               Program::Opcode::ADD,
                                               Program::Opcode::SUBTRACT_IMM,
                                               Program::Opcode::SUBTRACT);
                } else if (const auto* operation = dynamic_cast<const asg::Multiplication*>(term)) {
                    emitGroupOperationSequence(*operation,
                                               Program::Opcode::MULTIPLY_IMM,
                                               Program::Opcode::MULTIPLY,
                                               Program::Opcode::DIVIDE_IMM,
                                               Program::Opcode::DIVIDE);
                } else if (const auto* operation = dynamic_cast<const asg::Exponentiation*>(term)) {
                    emitInstruction(Program::Opcode::POWER,
                                    { .operand1 = getAddress(operation->base().get()),
                                      .operand2 = getAddress(operation->exponent().get()) },
                                    operation);
                } else if (const auto* operation = dynamic_cast<const asg::Squaring*>(term)) {
                    emitInstruction(Program::Opcode::MULTIPLY,
                                    { .operand1 = getAddress(operation->base().get()),
                                      .operand2 = getAddress(operation->base().get()) },
                                    operation);
                } else {
                    throw CompileException("Code generation failed -- data present in the code section");
                }
            }
        }

        // Replace some function calls with intrinsics.
        //
        // Most notably, if both "sin" and "cos" are called for the same value, they are replaced with SINCOS
        // and NOP instructions, respectively.
        void generateIntrinsics() {
            struct IntrinsicCandidates {
                Program::Instruction* sin = nullptr;
                Program::Instruction* cos = nullptr;
            };
            std::unordered_map<Program::Address, IntrinsicCandidates> candidates;
            for (Program::Address index = 0; index < mInstructions.instructions.size(); ++index) {
                Program::Instruction& instruction = mInstructions.instructions[index];
                if (instruction.opcode == Program::Opcode::CALL) {
                    if (instruction.function == RealFunction(&std::sin)) {
                        candidates[instruction.operand2].sin = &instruction;
                    }
                    if (instruction.function == RealFunction(&std::cos)) {
                        candidates[instruction.operand2].cos = &instruction;
                    }
                }
            }
            for (const auto& [_, candidate] : candidates) {
                if (candidate.sin && candidate.cos) {
                    candidate.sin->opcode       = Program::Opcode::SINCOS;
                    candidate.sin->displacement = candidate.cos - candidate.sin;
                    candidate.cos->opcode       = Program::Opcode::NOP;
                }
            }
        }

        void gather(const asg::Term& term) {
            if (mUniqueTerms.insert(&term).second) {
                const size_t level = size_t(term.depth());
                if (mTermLevels.size() < level + 1) {
                    mTermLevels.resize(level + 1);
                }
                mTermLevels[level].push_back(&term);
            }
        }

        // Visitor Interface

        virtual void visit(const asg::Sequence& term) override {
            for (const auto& t : term.terms()) {
                t->accept(*this);
            }
        }

        virtual void visit(const asg::Constant& term) override { gather(term); }

        virtual void visit(const asg::Input& term) override { gather(term); }

        virtual void visit(const asg::Output& term) override {
            gather(term);
            term.term()->accept(*this);
        }

        virtual void visit(const asg::UnaryFunction& term) override {
            gather(term);
            term.argument()->accept(*this);
        }

        void visit(const asg::GroupOperation& term) {
            gather(term);
            // Note: The constant term is excluded on purpose.
            for (const auto& t : term.positiveTerms()) {
                t->accept(*this);
            }
            for (const auto& t : term.negativeTerms()) {
                t->accept(*this);
            }
        }

        virtual void visit(const asg::Addition& term) override {
            visit(static_cast<const asg::GroupOperation&>(term));
        }

        virtual void visit(const asg::Multiplication& term) override {
            visit(static_cast<const asg::GroupOperation&>(term));
        }

        virtual void visit(const asg::Exponentiation& term) override {
            gather(term);
            term.base()->accept(*this);
            term.exponent()->accept(*this);
        }

        virtual void visit(const asg::Squaring& term) override {
            gather(term);
            term.base()->accept(*this);
        }
    };

} // anonymous namespace

//============================================================================================================
// Compiler
//============================================================================================================

class Compiler::Context {
    Lexicon                                        mPublicSymbols;
    std::vector<std::shared_ptr<ExpressionSymbol>> mOutputSymbols;

public:
    const Lexicon&                                        publicSymbols() const { return mPublicSymbols; }
    const std::vector<std::shared_ptr<ExpressionSymbol>>& outputSymbols() const { return mOutputSymbols; }

    void addPublicSymbol(std::shared_ptr<Symbol> symbol) { mPublicSymbols.add(std::move(symbol)); }

    void addOutputSymbol(std::shared_ptr<ExpressionSymbol> symbol) {
        for (const auto& output : mOutputSymbols) {
            if (output->name() == symbol->name()) {
                throw Exception(std::format("Duplicate output symbol '{}'", symbol->name()));
            }
        }
        mOutputSymbols.push_back(std::move(symbol));
    }
};

Compiler::Compiler()
    : mContext(std::make_unique<Context>()) {}

Compiler::~Compiler() = default;

void Compiler::addConstant(StringView name, Real value) {
    mContext->addPublicSymbol(std::make_shared<ConstantSymbol>(name, value));
}

void Compiler::addFunction(StringView name, RealFunction function) {
    mContext->addPublicSymbol(std::make_shared<FunctionSymbol>(name, function));
}

void Compiler::addParameter(StringView name, Real value) {
    mContext->addPublicSymbol(std::make_shared<ParameterSymbol>(name, value));
}

void Compiler::addVariable(StringView name) {
    mContext->addPublicSymbol(std::make_shared<VariableSymbol>(name));
}

Expression Compiler::addExpression(StringView name, StringView expression, Visibility visibility) {
    Expression parsedExpression = ExpressionParser(mContext->publicSymbols()).parseToExpression(expression);
    auto       symbol           = std::make_shared<ExpressionSymbol>(name, parsedExpression);
    if (visibility != Visibility::PRIVATE) {
        mContext->addPublicSymbol(symbol);
    }
    if (visibility != Visibility::SYMBOLIC) {
        mContext->addOutputSymbol(symbol);
    }
    return parsedExpression;
}

void Compiler::addSourceScript(StringView input) {
    ScriptParser(*this).parseScript(input);
}

std::vector<StringView> Compiler::getInputs() const {
    std::vector<StringView> inputs;
    for (const auto& [name, symbol] : mContext->publicSymbols().symbols()) {
        if (dynamic_cast<const VariableSymbol*>(symbol.get())) {
            inputs.emplace_back(name);
        }
    }
    return inputs;
}

std::vector<std::pair<StringView, Real>> Compiler::getParameters() const {
    std::vector<std::pair<StringView, Real>> parameters;
    for (const auto& [name, symbol] : mContext->publicSymbols().symbols()) {
        if (const auto* parameter = dynamic_cast<const ParameterSymbol*>(symbol.get())) {
            parameters.emplace_back(name, parameter->value());
        }
    }
    return parameters;
}

std::vector<std::pair<StringView, Expression>> Compiler::getOutputs() const {
    std::vector<std::pair<StringView, Expression>> outputs;
    for (const auto& output : mContext->outputSymbols()) {
        outputs.emplace_back(output->name(), output->expression());
    }
    return outputs;
}

Program Compiler::compile() const {
    using Transform = asg::Reduced<asg::Grouped<asg::ConstEvaluated<asg::Merge>>>;

    std::shared_ptr<const asg::Term> graph            = makeGraph();
    std::shared_ptr<const asg::Term> transformedGraph = Transform{}.transform(graph);
    return compileGraph(*transformedGraph);
}

std::shared_ptr<const asg::Term> Compiler::makeGraph() const {
    GraphBuilder graphBuilder;
    for (const auto& output : mContext->outputSymbols()) {
        try {
            graphBuilder.addOutput(output->name(), output->expression());
        } catch (const Exception& exception) {
            throw CompileException(std::format("Output '{}': {}", output->name(), exception.message()));
        }
    }
    return graphBuilder.makeGraph();
}

Program Compiler::compileGraph(const asg::Term& graph) const {
    return CodeGenerator(graph).generate(mContext->publicSymbols());
}

SIXPACK_NAMESPACE_END
