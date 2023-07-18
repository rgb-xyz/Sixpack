#include "Expression.h"
#include "Ast.h"
#include "Exception.h"
#include <format>

SIXPACK_NAMESPACE_BEGIN

StringView Expression::input() const {
    return mData ? mData->input : StringView{};
}

StringView Expression::error() const {
    if (!mData) {
        return "Uninitialized object";
    }
    if (mData->error) {
        return mData->error->message();
    } else {
        assert(mData->astRoot);
        return "No error";
    }
}

StringPosition Expression::errorPosition() const {
    if (mData && mData->error) {
        return mData->error->where();
    } else {
        return String::npos;
    }
}

Expression::operator bool() const {
    return mData && mData->astRoot;
}

void Expression::visitAst(ast::Visitor& visitor) const {
    if (mData && mData->astRoot) {
        mData->astRoot->accept(visitor);
    } else if (mData->error) {
        throw Exception(
            std::format("{} at character {}", mData->error->message(), mData->error->where() + 1));
    } else {
        throw Exception(String(error()));
    }
}

SIXPACK_NAMESPACE_END
