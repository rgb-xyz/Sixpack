#pragma once
#include <cassert>
#include <string>
#include <string_view>

// Macros
//============================================================================================================

#define SIXPACK_NAMESPACE_BEGIN namespace sixpack {
#define SIXPACK_NAMESPACE_END   } // namespace sixpack

SIXPACK_NAMESPACE_BEGIN

// Configuration / Aliases
//============================================================================================================

using String         = std::string;
using StringView     = std::basic_string_view<String::value_type>;
using StringPosition = String::size_type;

using Real         = double;
using RealFunction = Real (*)(Real);

SIXPACK_NAMESPACE_END