#include "dahlia_lib/Stack.h"

#include <cassert>

void Stack::pushContext() { callContexts_.emplace_back(); }

void Stack::popContext() {
    assert(!callContexts_.empty());
    callContexts_.pop_back();
}

CallContext& Stack::current() noexcept {
    assert(!callContexts_.empty());
    return callContexts_.back();
}

CallContext const& Stack::current() const noexcept {
    assert(!callContexts_.empty());
    return callContexts_.back();
}

void CallContext::pushScope() { scopes_.emplace_back(); }

void CallContext::popScope() {
    assert(!scopes_.empty());
    scopes_.pop_back();
}

Value const* CallContext::lookupValue(
    std::string const& identifier) const noexcept {
    assert(!scopes_.empty());

    auto const& scope = scopes_.back();

    auto iter = scope.variables.find(identifier);
    if (iter == scope.variables.cend()) {
        return nullptr;
    }
    return &iter->second.data();
}

Variable* CallContext::lookupVariable(std::string const& identifier) noexcept {
    assert(!scopes_.empty());

    auto& scope = scopes_.back();

    auto iter = scope.variables.find(identifier);
    if (iter == scope.variables.cend()) {
        return nullptr;
    }
    return &iter->second;
}

std::optional<Position> CallContext::declareVariable(std::string identifier,
                                                     Variable var) {  // NOLINT
    assert(!scopes_.empty());

    auto& scope = scopes_.back();

    auto const [iter, duplicate] =
        scope.variables.try_emplace(std::move(identifier), std::move(var));

    if (duplicate) {
        return iter->second.pos();
    }
    return std::nullopt;
}
