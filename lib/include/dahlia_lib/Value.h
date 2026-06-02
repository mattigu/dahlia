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
EvalResult subtract(Value lhs, Value rhs);
EvalResult multiply(Value lhs, Value rhs);
EvalResult divide(Value lhs, Value rhs);
EvalResult modulo(Value lhs, Value rhs);

using IntResult = std::expected<std::int64_t, RuntimeErrorKind>;
using DoubleResult = std::expected<double, RuntimeErrorKind>;

IntResult checkedAdd(std::int64_t lhs, std::int64_t rhs);
IntResult checkedSub(std::int64_t lhs, std::int64_t rhs);
IntResult checkedMul(std::int64_t lhs, std::int64_t rhs);
IntResult checkedDiv(std::int64_t lhs, std::int64_t rhs);
IntResult checkedMod(std::int64_t lhs, std::int64_t rhs);

DoubleResult checkedDoubleDiv(double lhs, double rhs);

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

EvalResult logicalNot(Value const& val);

EvalResult length(Value const& val);
EvalResult negation(Value const& val);

EvalResult intersect(Value lhs, Value rhs);
EvalResult contains(Value const& lhs, Value const& rhs);




EvalResult coerce(Value const& val, Type const& target) noexcept;

bool toBool(Value const& val) noexcept;

std::expected<double, RuntimeErrorKind> toFloat(Value const& val) noexcept;
std::expected<std::int64_t, RuntimeErrorKind> toInt(Value const& val) noexcept;

std::string toString(VecValue const& vec);
std::string toString(Value const& value);

std::string repeat(std::string const& str, std::int64_t n);

template <typename Op>
concept IntBinaryOp =
    std::invocable<Op, std::int64_t, std::int64_t> &&
    std::same_as<std::invoke_result_t<Op, std::int64_t, std::int64_t>,
                 IntResult>;

template <typename Op>
concept DoubleBinaryOp =
    std::invocable<Op, double, double> &&
    std::same_as<std::invoke_result_t<Op, double, double>, DoubleResult>;

enum class StringIntPolicy : std::uint8_t { Repeat, ParseAsFloat };

template <IntBinaryOp IntOp, DoubleBinaryOp DoubleOp>
EvalResult arithmeticOp(
    Value left, Value right, IntOp int_op, DoubleOp double_op,
    StringIntPolicy str_int = StringIntPolicy::ParseAsFloat) {
    return std::visit(
        Overloaded{

            [&](std::int64_t lhs, std::int64_t rhs) -> EvalResult {
                return int_op(lhs, rhs);
            },
            [&](std::int64_t lhs, double rhs) -> EvalResult {
                return double_op(static_cast<double>(lhs), rhs);
            },
            [&](std::int64_t lhs, std::string const& rhs) -> EvalResult {
                if (str_int == StringIntPolicy::ParseAsFloat) {
                    auto str_as_int = toInt(rhs);
                    if (!str_as_int) {
                        return str_as_int;
                    }
                    return int_op(lhs, *str_as_int);
                }
                return repeat(rhs, lhs);
            },
            [&](std::int64_t lhs, bool rhs) -> EvalResult {
                return int_op(lhs, static_cast<std::int64_t>(rhs));
            },
            [&](double lhs, std::int64_t rhs) -> EvalResult {
                return double_op(lhs, static_cast<double>(rhs));
            },
            [&](double lhs, double rhs) -> EvalResult {
                return double_op(lhs, rhs);
            },
            [&](double lhs, std::string const& rhs) -> EvalResult {
                auto str_as_float = toFloat(rhs);
                if (!str_as_float) {
                    return str_as_float;
                }
                return double_op(lhs, str_as_float.value());
            },
            [&](double lhs, bool rhs) -> EvalResult {
                return double_op(lhs, static_cast<double>(rhs));
            },
            [&](std::string const& lhs, std::string const& rhs) -> EvalResult {
                auto lhs_as_float = toFloat(lhs);
                if (!lhs_as_float.has_value()) {
                    return lhs_as_float;
                }
                auto rhs_as_float = toFloat(rhs);
                if (!rhs_as_float.has_value()) {
                    return rhs_as_float;
                }
                return double_op(lhs_as_float.value(), rhs_as_float.value());
            },
            [&](std::string const& lhs, double rhs) -> EvalResult {
                auto str_as_float = toFloat(lhs);
                if (!str_as_float) {
                    return str_as_float;
                }
                return double_op(str_as_float.value(), rhs);
            },

            [&](std::string const& lhs, std::int64_t rhs) -> EvalResult {
                if (str_int == StringIntPolicy::ParseAsFloat) {
                    auto str_as_int = toInt(lhs);
                    if (!str_as_int) {
                        return str_as_int;
                    }
                    return int_op(*str_as_int, rhs);
                }
                return repeat(lhs, rhs);
            },
            [&](std::string const& lhs, bool rhs) -> EvalResult {
                auto str_as_int = toInt(lhs);
                if (!str_as_int) {
                    return str_as_int;
                }

                return int_op(*str_as_int, static_cast<std::int64_t>(rhs));
            },
            [&](bool lhs, bool rhs) -> EvalResult {
                return int_op(static_cast<std::int64_t>(lhs),
                              static_cast<std::int64_t>(rhs));
            },
            [&](bool lhs, double rhs) -> EvalResult {
                return double_op(static_cast<double>(lhs), rhs);
            },
            [&](bool lhs, std::int64_t rhs) -> EvalResult {
                return int_op(static_cast<std::int64_t>(lhs), rhs);
            },
            [&](auto const& lhs, auto const& rhs) -> EvalResult {
                return std::unexpected(
                    InvalidOperands{.lhs = typeFor(lhs), .rhs = typeFor(rhs)});
            }},
        std::move(left), std::move(right));
}

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
