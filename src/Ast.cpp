#include "Ast.h"

SIXPACK_NAMESPACE_BEGIN

void ast::NiladicNode::accept(Visitor& visitor) const {
    visitor.visit(*this);
}

void ast::MonadicNode::accept(Visitor& visitor) const {
    visitor.visit(*this);
}

void ast::DyadicNode::accept(Visitor& visitor) const {
    visitor.visit(*this);
}

void ast::Literal::accept(Visitor& visitor) const {
    visitor.visit(*this);
}

void ast::Value::accept(Visitor& visitor) const {
    visitor.visit(*this);
}

void ast::UnaryFunction::accept(Visitor& visitor) const {
    visitor.visit(*this);
}

void ast::UnaryOperator::accept(Visitor& visitor) const {
    visitor.visit(*this);
}

void ast::BinaryOperator::accept(Visitor& visitor) const {
    visitor.visit(*this);
}

SIXPACK_NAMESPACE_END
