#include "Utilities.h"
#include "Asg.h"
#include "Ast.h"
#include "Exception.h"
#include "Program.h"
#include "Symbols.h"
#include <algorithm>
#include <format>
#include <iostream>

SIXPACK_NAMESPACE_BEGIN

namespace {

    static StringView getTypeName(const auto& object) {
        const StringView typeName = typeid(object).name();
        return typeName.substr(typeName.rfind(':') + 1);
    }

    //========================================================================================================
    // TabulatedPrintout
    //========================================================================================================

    template <int COLUMNS>
    class TabulatedPrintout {
        std::vector<std::array<String, COLUMNS>> mRows;
        std::array<bool, COLUMNS>                mColumnVisibilities;

    public:
        TabulatedPrintout() { mColumnVisibilities.fill(true); }

        void addRow(std::array<String, COLUMNS> row) { mRows.push_back(std::move(row)); }

        void setColumnVisiblity(int column, bool visible) { mColumnVisibilities[column] = visible; }

        void sortByColumn(int column) {
            std::stable_sort(mRows.begin(), mRows.end(), [column](const auto& row1, const auto& row2) {
                return row1[column] < row2[column];
            });
        }

        void print(std::ostream& output) const {
            std::array<size_t, COLUMNS> columnWidths{};
            for (const auto& row : mRows) {
                for (int i = 0; i < COLUMNS; ++i) {
                    columnWidths[i] = std::max(columnWidths[i], row[i].size());
                }
            }
            for (const auto& row : mRows) {
                int count = 0;
                for (int column = 0; column < COLUMNS; ++column) {
                    if (!mColumnVisibilities[column]) {
                        continue;
                    }
                    if (count > 0) {
                        output << "\t";
                    }
                    ++count;
                    output << std::format("{:{}}", row[column], columnWidths[column]);
                }
                output << std::endl;
            }
        }
    };

    //========================================================================================================
    // TreePrinter
    //========================================================================================================

    class TreePrinter {
        std::vector<int> mIndents;

    public:
        String printNode(StringView text) {
            String result;
            for (size_t i = 0; i < mIndents.size(); ++i) {
                if (mIndents[i] > 0) {
                    result += i + 1 < mIndents.size() ? "  | " : "  +-";
                } else {
                    result += "    ";
                }
            }
            result += text;
            if (!mIndents.empty()) {
                mIndents.back()--;
            }
            return result;
        }

        void enterChildren(int count) { mIndents.push_back(count); }
        void leaveChildren() { mIndents.pop_back(); }
    };

    //========================================================================================================
    // StringifyAstVisitor
    //========================================================================================================

    class StringifyAstVisitor final : public ast::Visitor {
        const Notation mNotation;
        String         mResult;

    public:
        explicit StringifyAstVisitor(Notation notation)
            : mNotation(notation) {}

        String result() const { return mResult; }

    private:
        static int getPriority(const ast::Node& node) {
            // >0 -- associative
            // <0 -- not associative
            if (const auto* binaryOp = dynamic_cast<const ast::BinaryOperator*>(&node)) {
                switch (binaryOp->type()) {
                case ast::BinaryOperator::Type::CARET: return -1;
                case ast::BinaryOperator::Type::SLASH: return -2;
                case ast::BinaryOperator::Type::ASTERISK: return 2;
                case ast::BinaryOperator::Type::MINUS: return -3;
                case ast::BinaryOperator::Type::PLUS: return 3;
                }
            }
            return 0;
        }

        void addToResult(StringView text) {
            if (mNotation != Notation::INFIX && !mResult.empty()) {
                mResult += ' ';
            }
            mResult += text;
        }

        // Visitor Interface

        virtual void visit(const ast::Node& node) override { throw Exception("Unhandled node category."); }
        virtual void visit(const ast::Literal& node) override { addToResult(node.innerSourceView()); }
        virtual void visit(const ast::Value& node) override { addToResult(node.valueSymbol().name()); }

        virtual void visit(const ast::UnaryFunction& node) override {
            if (mNotation == Notation::INFIX) {
                addToResult(node.functionSymbol().name() + "(");
                node.argument().accept(*this);
                addToResult(")");
            } else {
                if (mNotation == Notation::PREFIX) {
                    addToResult(node.functionSymbol().name());
                }
                node.argument().accept(*this);
                if (mNotation == Notation::POSTFIX) {
                    addToResult(node.functionSymbol().name());
                }
            }
        }

        virtual void visit(const ast::UnaryOperator& node) override {
            if (mNotation == Notation::INFIX) {
                addToResult(node.innerSourceView());
                const bool needsParentheses = std::abs(getPriority(node.operand())) >= 3;
                if (needsParentheses) {
                    addToResult("(");
                }
                node.operand().accept(*this);
                if (needsParentheses) {
                    addToResult(")");
                }
            } else {
                if (mNotation == Notation::PREFIX) {
                    addToResult("u" + String(node.innerSourceView()));
                }
                node.operand().accept(*this);
                if (mNotation == Notation::POSTFIX) {
                    addToResult("u" + String(node.innerSourceView()));
                }
            }
        }

        virtual void visit(const ast::BinaryOperator& node) override {
            if (mNotation == Notation::INFIX) {
                const int  priority             = getPriority(node);
                const int  leftPriority         = getPriority(node.leftOperand());
                const int  rightPriority        = getPriority(node.rightOperand());
                const bool needsLeftParentheses = std::abs(leftPriority) > std::abs(priority);
                const bool needsRightParentheses =
                    std::abs(rightPriority) > std::abs(priority) ||
                    (std::abs(rightPriority) == std::abs(priority) && priority < 0);
                if (needsLeftParentheses) {
                    addToResult("(");
                }
                node.leftOperand().accept(*this);
                if (needsLeftParentheses) {
                    addToResult(")");
                }
                addToResult(" " + String(node.innerSourceView()) + " ");
                if (needsRightParentheses) {
                    addToResult("(");
                }
                node.rightOperand().accept(*this);
                if (needsRightParentheses) {
                    addToResult(")");
                }
            } else {
                if (mNotation == Notation::PREFIX) {
                    addToResult(node.innerSourceView());
                }
                node.leftOperand().accept(*this);
                node.rightOperand().accept(*this);
                if (mNotation == Notation::POSTFIX) {
                    addToResult(node.innerSourceView());
                }
            }
        }
    };

    //========================================================================================================
    // DumpAstVisitor
    //========================================================================================================

    class DumpAstVisitor final : public ast::Visitor {
        const StringView     mSourceView;
        TabulatedPrintout<3> mPrintout;
        TreePrinter          mTreePrinter;

    public:
        explicit DumpAstVisitor(StringView sourceView)
            : mSourceView(sourceView) {
            mPrintout.setColumnVisiblity(0, !mSourceView.empty());
        }

        const auto& printout() const { return mPrintout; }

    private:
        String getColorSourceView(StringView innerView, StringView outerView) const {
            if (!mSourceView.empty()) {
                const ptrdiff_t innerOffset = innerView.data() - mSourceView.data();
                const ptrdiff_t outerOffset = outerView.data() - mSourceView.data();
                if ((innerOffset >= 0 && size_t(innerOffset) < mSourceView.size()) &&
                    (outerOffset >= 0 && size_t(outerOffset) < mSourceView.size())) {
                    String result{ mSourceView };
                    result.insert(outerOffset + outerView.size(), "\033[90m\033[2m");
                    result.insert(innerOffset + innerView.size(), "\033[27m");
                    result.insert(innerOffset, "\033[7m");
                    result.insert(outerOffset, "\033[37m\033[22m");
                    result.insert(0, "\033[90m\033[2m");
                    result += "\033[0m";
                    return result;
                }
            }
            return String{};
        }

        // Visitor Interface

        virtual void visit(const ast::Node& node) override {
            String valueType;
            if (const auto* value = dynamic_cast<const ast::Value*>(&node)) {
                valueType = " -> " + String(getTypeName(value->valueSymbol()));
            } else if (const auto* function = dynamic_cast<const ast::UnaryFunction*>(&node)) {
                valueType = " -> " + String(getTypeName(function->functionSymbol()));
            }
            mPrintout.addRow({ getColorSourceView(node.innerSourceView(), node.outerSourceView()),
                               mTreePrinter.printNode(String(getTypeName(node)) + valueType),
                               "'" + String(node.innerSourceView()) + "'" });
            mTreePrinter.enterChildren(int(node.children().size()));
            for (const auto& child : node.children()) {
                child->accept(*this);
            }
            mTreePrinter.leaveChildren();
        }
    };

    //========================================================================================================
    // DumpAsgVisitor
    //========================================================================================================

    class DumpAsgVisitor final : public asg::Visitor {
        TabulatedPrintout<4> mPrintout;
        TreePrinter          mTreePrinter;

        mutable std::unordered_map<const asg::Term*, size_t> mTermIds;

    public:
        const auto& printout() const { return mPrintout; }

    private:
        String getTermId(const asg::Term& term) const {
            return std::format("[{:04}]", mTermIds.insert({ &term, mTermIds.size() + 1 }).first->second);
        }

        void addRow(const asg::Term& term, StringView typeName, String extra = {}) {
            String value{ typeName };
            if (!extra.empty()) {
                value = std::format("{} ({})", value, extra);
            }
            String source;
            if (const ast::Node* sourceNode = term.sourceNode()) {
                source = std::format("'{}'", sourceNode->outerSourceView());
            }
            mPrintout.addRow({ getTermId(term),
                               std::format("{}", term.depth()),
                               mTreePrinter.printNode(value),
                               std::move(source) });
        }

        void handleTerm(const asg::Term& term) {
            const auto termIt = mTermIds.find(&term);
            if (termIt != mTermIds.end()) {
                addRow(term, std::format("->[{:04}] ({})", termIt->second, getTypeName(term)));
                return;
            }
            term.accept(*this);
        }

        // Visitor Interface

        virtual void visit(const asg::Sequence& term) override {
            addRow(term, getTypeName(term));
            mTreePrinter.enterChildren(int(term.terms().size()));
            for (const auto& t : term.terms()) {
                handleTerm(*t);
            }
            mTreePrinter.leaveChildren();
        }

        virtual void visit(const asg::Constant& term) override {
            addRow(term, getTypeName(term), std::format("{}", term.value()));
        }

        virtual void visit(const asg::Input& term) override {
            addRow(term, getTypeName(term), std::format("{}", term.name()));
        }

        virtual void visit(const asg::Output& term) override {
            addRow(term, getTypeName(term), std::format("{}", term.name()));
            mTreePrinter.enterChildren(1);
            handleTerm(*term.term());
            mTreePrinter.leaveChildren();
        }

        virtual void visit(const asg::UnaryFunction& term) override {
            addRow(term,
                   getTypeName(term),
                   std::format("{:#x}", reinterpret_cast<uintptr_t>(term.function())));
            mTreePrinter.enterChildren(1);
            handleTerm(*term.argument());
            mTreePrinter.leaveChildren();
        }

        void visit(const asg::GroupOperation& term) {
            addRow(term, getTypeName(term));
            const auto [positiveSign, negativeSign] = term.operatorSigns();
            const bool hasConstant                  = term.constantTerm()->value() != term.identity();
            const bool hasPositiveTerms             = !term.positiveTerms().empty();
            const bool hasNegativeTerms             = !term.negativeTerms().empty();
            mTreePrinter.enterChildren(int(hasConstant) + int(hasPositiveTerms) + int(hasNegativeTerms));
            if (hasConstant) {
                handleTerm(*term.constantTerm());
            }
            if (hasPositiveTerms) {
                mPrintout.addRow(
                    { String{}, String{}, mTreePrinter.printNode(std::format("{{ {} }}", positiveSign)) });
                {
                    mTreePrinter.enterChildren(int(term.positiveTerms().size()));
                    for (const auto& t : term.positiveTerms()) {
                        handleTerm(*t);
                    }
                    mTreePrinter.leaveChildren();
                }
            }
            if (hasNegativeTerms) {
                mPrintout.addRow(
                    { String{}, String{}, mTreePrinter.printNode(std::format("{{ {} }}", negativeSign)) });
                {
                    mTreePrinter.enterChildren(int(term.negativeTerms().size()));
                    for (const auto& t : term.negativeTerms()) {
                        handleTerm(*t);
                    }
                    mTreePrinter.leaveChildren();
                }
            }
            mTreePrinter.leaveChildren();
        }

        virtual void visit(const asg::Addition& term) override {
            visit(static_cast<const asg::GroupOperation&>(term));
        }

        virtual void visit(const asg::Multiplication& term) override {
            visit(static_cast<const asg::GroupOperation&>(term));
        }

        virtual void visit(const asg::Exponentiation& term) override {
            addRow(term, getTypeName(term));
            mTreePrinter.enterChildren(2);
            handleTerm(*term.base());
            handleTerm(*term.exponent());
            mTreePrinter.leaveChildren();
        }

        virtual void visit(const asg::Squaring& term) override {
            addRow(term, getTypeName(term));
            mTreePrinter.enterChildren(1);
            handleTerm(*term.base());
            mTreePrinter.leaveChildren();
        }
    };

} // anonymous namespace

String stringifyExpression(const ast::Node& root, Notation notation) {
    StringifyAstVisitor visitor(notation);
    root.accept(visitor);
    return visitor.result();
}

String stringifyExpression(const Expression& expression, Notation notation) {
    if (expression) {
        StringifyAstVisitor visitor(notation);
        expression.visitAst(visitor);
        return visitor.result();
    } else {
        return std::format("*** Error: {} at character {}.",
                           expression.error(),
                           expression.errorPosition() + 1);
    }
}

void dumpSyntaxTree(const ast::Node& root, std::ostream& output, StringView sourceView) {
    DumpAstVisitor visitor(sourceView);
    root.accept(visitor);
    visitor.printout().print(output);
}

void dumpSyntaxTree(const Expression& expression, std::ostream& output, bool includeSourceView) {
    if (expression) {
        DumpAstVisitor visitor(includeSourceView ? expression.input() : StringView{});
        expression.visitAst(visitor);
        visitor.printout().print(output);
    }
}

void dumpSemanticGraph(const asg::Term& root, std::ostream& output) {
    DumpAsgVisitor visitor;
    root.accept(visitor);
    visitor.printout().print(output);
}

void dumpProgram(const Program& program, std::ostream& output) {
    const auto formatAddress = [](Program::Address address) {
        return std::format("[{:#04}]", address);
    };
    const auto formatComment = [&](Program::Address address) {
        const auto commentIt = program.comments().find(address);
        if (commentIt != program.comments().end()) {
            return std::format("; {}", commentIt->second);
        }
        return String{};
    };

    const Program::Address&                  constSection = program.constants().memoryOffset;
    const std::vector<Real>&                 constValues  = program.constants().values;
    const Program::Address&                  codeSection  = program.instructions().memoryOffset;
    const std::vector<Program::Instruction>& code         = program.instructions().instructions;

    TabulatedPrintout<4> printout;
    printout.addRow({ formatAddress(0), ".data" });
    for (Program::Address address = 0; address < codeSection; ++address) {
        String value;
        if (address >= constSection && address < constSection + constValues.size()) {
            value = std::format("{}", constValues[address - constSection]);
        } else {
            value = "?";
        }
        printout.addRow({ formatAddress(address), " word", value, formatComment(address) });
    }
    printout.addRow({ formatAddress(codeSection), {}, {}, {} });
    printout.addRow({ formatAddress(codeSection), ".start" });
    for (Program::Address i = 0; i < code.size(); ++i) {
        const Program::Address address = codeSection + i;
        String                 mnemonic, arguments;
        switch (code[i].opcode) {
        case Program::Opcode::NOP: mnemonic = "nop"; break;
        case Program::Opcode::ADD:
            mnemonic = "add";
            arguments =
                std::format("{}, {}", formatAddress(code[i].operand1), formatAddress(code[i].operand2));
            break;
        case Program::Opcode::ADD_IMM:
            mnemonic  = "add";
            arguments = std::format("{}, {}", code[i].immediate, formatAddress(code[i].operand2));
            break;
        case Program::Opcode::SUBTRACT:
            mnemonic = "sub";
            arguments =
                std::format("{}, {}", formatAddress(code[i].operand1), formatAddress(code[i].operand2));
            break;
        case Program::Opcode::SUBTRACT_IMM:
            mnemonic  = "sub";
            arguments = std::format("{}, {}", code[i].immediate, formatAddress(code[i].operand2));
            break;
        case Program::Opcode::MULTIPLY:
            mnemonic = "mul";
            arguments =
                std::format("{}, {}", formatAddress(code[i].operand1), formatAddress(code[i].operand2));
            break;
        case Program::Opcode::MULTIPLY_IMM:
            mnemonic  = "mul";
            arguments = std::format("{}, {}", code[i].immediate, formatAddress(code[i].operand2));
            break;
        case Program::Opcode::DIVIDE:
            mnemonic = "div";
            arguments =
                std::format("{}, {}", formatAddress(code[i].operand1), formatAddress(code[i].operand2));
            break;
        case Program::Opcode::DIVIDE_IMM:
            mnemonic  = "div";
            arguments = std::format("{}, {}", code[i].immediate, formatAddress(code[i].operand2));
            break;
        case Program::Opcode::POWER:
            mnemonic = "pow";
            arguments =
                std::format("{}, {}", formatAddress(code[i].operand1), formatAddress(code[i].operand2));
            break;
        case Program::Opcode::SINCOS:
            mnemonic  = "sincos";
            arguments = std::format("${:+}, {}", code[i].displacement, formatAddress(code[i].operand2));
            break;
        case Program::Opcode::CALL:
            mnemonic  = "call";
            arguments = std::format("{:p}, {}",
                                    reinterpret_cast<void*>(code[i].function),
                                    formatAddress(code[i].operand2));
            break;
        default: assert(false); mnemonic = "???";
        }
        printout.addRow({ formatAddress(address), " " + mnemonic, arguments, formatComment(address) });
    }
    printout.sortByColumn(0);
    printout.print(output);
}

SIXPACK_NAMESPACE_END
