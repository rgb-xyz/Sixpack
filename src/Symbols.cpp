#include "Symbols.h"
#include "Exception.h"
#include <format>

SIXPACK_NAMESPACE_BEGIN

void Lexicon::add(std::shared_ptr<Symbol> symbol) {
    assert(symbol);
    std::shared_ptr<Symbol>& tableSymbol = mSymbols[symbol->name()];
    if (tableSymbol) {
        throw Exception(std::format("Duplicate symbol '{}'", symbol->name()));
    }
    tableSymbol = std::move(symbol);
}

std::shared_ptr<Symbol> Lexicon::find(StringView name) const {
    const auto symbolIt = mSymbols.find(name);
    return symbolIt != mSymbols.end() ? symbolIt->second : nullptr;
}

SIXPACK_NAMESPACE_END
