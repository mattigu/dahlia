#pragma once
#include <cstdint>
#include <expected>
#include <format>
#include <string>
#include <variant>
#include <vector>

#include "Ast.h"
#include "dahlia_lib/RuntimeError.h"

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

using EvalResult = std::expected<Value, RuntimeErrorKind>;

bool toBool(Value const& val) noexcept;

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

std::string toString(VecValue const& vec);
std::string toString(Value const& value);

template <>
struct std::formatter<Value> : std::formatter<std::string> {
    auto format(Value const& value, std::format_context& ctx) const {
        return std::formatter<std::string>::format(toString(value), ctx);
    }
};

std::ostream& operator<<(std::ostream& oss, Value const& value);
