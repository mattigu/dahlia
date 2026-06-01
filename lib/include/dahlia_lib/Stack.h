#pragma once
#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

#include "Position.h"
#include "Value.h"
#include "Variable.h"

struct Scope {
    std::unordered_map<std::string, Variable> variables;
};

class CallContext {
public:
    CallContext() noexcept = default;

    void pushScope();
    void popScope();

    [[nodiscard]] Value const* lookupValue(
        std::string const& identifier) const noexcept;

    [[nodiscard]] Variable* lookupVariable(
        std::string const& identifier) noexcept;

    std::optional<Position> declareVariable(std::string identifier,
                                            Variable var);

private:
    std::vector<Scope> scopes_;
};

class Stack {
public:
    void pushContext();
    void popContext();

    [[nodiscard]] CallContext& current() noexcept;
    [[nodiscard]] CallContext const& current() const noexcept;

    [[nodiscard]] std::size_t callDepth() const noexcept;

private:
    std::vector<CallContext> callContexts_;
};

// declare
// declare ref for function params
// lookup value, must return a ref correctly
//
