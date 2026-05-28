#pragma once
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "Ast.h"

template <class... Ts>
struct Overloaded : Ts... {
    using Ts::operator()...;
};

struct VecValue;

struct UninitVec {
    bool operator==(UninitVec const& other) const = default;
};

using Value = std::variant<std::monostate, bool, std::int64_t, double,
                           std::string, VecValue, UninitVec>;

struct VecValue {
    Type type;
    std::vector<Value> elements;

    bool operator==(VecValue const& other) const = default;
};

constexpr Type typeFor(Value const& value) {
    return std::visit(
        Overloaded{
            [](std::int64_t) -> Type { return PrimitiveType::Int; },
            [](double) -> Type { return PrimitiveType::Float; },
            [](bool) -> Type { return PrimitiveType::Bool; },
            [](std::string const&) -> Type { return PrimitiveType::Str; },
            [](VecValue const& vec) -> Type { return Type::vec(vec.type); },
            [](UninitVec) -> Type {
                return PrimitiveType::Int;
            },  // placeholder
            [](std::monostate) -> Type {
                return PrimitiveType::Int;
            },  // placeholder
        },
        value);
}
