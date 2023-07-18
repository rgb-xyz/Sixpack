#pragma once
#include "Common.h"
#include <memory>
#include <optional>

SIXPACK_NAMESPACE_BEGIN

namespace ast {
    class Node;
    class Visitor;
}
class ParseException;

class Expression {
    friend class ExpressionParser;

    struct Data {
        String                          input;
        std::unique_ptr<ast::Node>      astRoot;
        std::unique_ptr<ParseException> error;
    };
    std::shared_ptr<Data> mData;

public:
    StringView     input() const;
    StringView     error() const;
    StringPosition errorPosition() const;

    explicit operator bool() const;

    void visitAst(ast::Visitor& visitor) const;
};

SIXPACK_NAMESPACE_END
