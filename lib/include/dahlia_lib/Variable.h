#pragma once
#include "Value.h"

class Variable {
public:
    Variable(Value data, bool mut, Position pos) noexcept
        : storage_{std::move(data)}, mut_{mut}, pos_{pos} {}

    Variable(Value* ref, bool mut, Position pos) noexcept
        : storage_{ref}, mut_{mut}, pos_{pos} {}

    Value& data() noexcept {
        return std::visit(Overloaded{
                              [](Value& val) -> Value& { return val; },
                              [](Value* val) -> Value& { return *val; },
                          },
                          storage_);
    }

    [[nodiscard]] Value const& data() const noexcept {
        return std::visit(
            Overloaded{
                [](Value const& val) -> Value const& { return val; },  // NOLINT
                [](Value* val) -> Value const& { return *val; },
            },
            storage_);
    }

    [[nodiscard]] bool mut() const noexcept { return mut_; }
    [[nodiscard]] Position pos() const noexcept { return pos_; }

private:
    std::variant<Value, Value*> storage_;
    Position pos_;
    bool mut_;
};
