#include "dahlia_lib/Stack.h"

#include <cassert>
#include <ranges>

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

    for (auto const& scope : scopes_ | std::views::reverse) {
        auto const iter = scope.variables.find(identifier);

        if (iter != scope.variables.cend()) {
            return &iter->second.data();
        }
    }

    return nullptr;
}

Variable* CallContext::lookupVariable(std::string const& identifier) noexcept {
    assert(!scopes_.empty());

    for (auto& scope : scopes_ | std::views::reverse) {
        auto const iter = scope.variables.find(identifier);

        if (iter != scope.variables.cend()) {
            return &iter->second;
        }
    }

    return nullptr;
}

std::optional<Position> CallContext::declareVariable(std::string identifier,
                                                     Variable var) {  // NOLINT
    assert(!scopes_.empty());

    auto& scope = scopes_.back();

    auto const [iter, inserted] =
        scope.variables.try_emplace(std::move(identifier), std::move(var));

    if (!inserted) {
        return iter->second.pos();
    }
    return std::nullopt;
}

std::size_t Stack::callDepth() const noexcept { return callContexts_.size(); };
