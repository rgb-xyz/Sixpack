#pragma once
#include "Common.h"

SIXPACK_NAMESPACE_BEGIN

class Exception {
    String mMessage;

public:
    explicit Exception(String message)
        : mMessage(std::move(message)) {}

    virtual ~Exception() = default;

    const String& message() const { return mMessage; }
};

class ParseException : public Exception {
    StringPosition mWhere;

public:
    ParseException(String message, StringPosition where)
        : Exception(std::move(message))
        , mWhere(where) {}

    StringPosition where() const { return mWhere; }
};

class CompileException : public Exception {
public:
    using Exception::Exception;
};

SIXPACK_NAMESPACE_END
