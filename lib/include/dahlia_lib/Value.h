#pragma once
#include <cmath>
#include <compare>
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
    std::partial_ordering operator<=>(VecValue const& other) const;
};

Type typeFor(Value const& value);

using EvalResult = std::expected<Value, RuntimeErrorKind>;

EvalResult add(Value lhs, Value rhs);
EvalResult checkedAdd(std::int64_t lhs, std::int64_t rhs);
EvalResult checkedMul(std::int64_t lhs, std::int64_t rhs);
EvalResult checkedSub(std::int64_t lhs, std::int64_t rhs);
EvalResult checkedDiv(std::int64_t lhs, std::int64_t rhs);
EvalResult checkedDoubleDiv(double lhs, double rhs);

using CmpResult = std::expected<std::partial_ordering, InvalidOperands>;

std::partial_ordering operator<=>(Value const& lhs, Value const& rhs);
CmpResult valueCompare(Value const& lhs, Value const& rhs);

template <typename Pred>
EvalResult compareOp(Value const& lhs, Value const& rhs, Pred pred) {
    return valueCompare(lhs, rhs).transform(
        [pred](std::partial_ordering ord) -> Value { return pred(ord); });
}

EvalResult eq(Value const& lhs, Value const& rhs);
EvalResult neq(Value const& lhs, Value const& rhs);
EvalResult lt(Value const& lhs, Value const& rhs);
EvalResult gt(Value const& lhs, Value const& rhs);
EvalResult le(Value const& lhs, Value const& rhs);
EvalResult ge(Value const& lhs, Value const& rhs);

template <typename Op>
std::expected<double, RuntimeErrorKind> checkedDoubleOp(double lhs, double rhs,
                                                        Op oper) {
    auto result = oper(lhs, rhs);
    if (!std::isfinite(result)) {
        return std::unexpected(ArithmeticOverflow{});
    }
    return result;
}

EvalResult coerce(Value const& val, Type const& target) noexcept;

bool toBool(Value const& val) noexcept;

std::expected<double, RuntimeErrorKind> toFloat(Value const& val) noexcept;
std::expected<std::int64_t, RuntimeErrorKind> toInt(Value const& val) noexcept;

std::string toString(VecValue const& vec);
std::string toString(Value const& value);

template <>
struct std::formatter<Value> : std::formatter<std::string> {
    auto format(Value const& value, std::format_context& ctx) const {
        return std::formatter<std::string>::format(toString(value), ctx);
    }
};

template <>
struct std::formatter<VecValue> : std::formatter<std::string> {
    auto format(VecValue const& value, std::format_context& ctx) const {
        return std::formatter<std::string>::format(toString(value), ctx);
    }
};

std::ostream& operator<<(std::ostream& oss, Value const& value);
std::ostream& operator<<(std::ostream& oss, VecValue const& value);
