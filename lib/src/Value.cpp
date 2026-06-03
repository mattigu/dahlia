#include "dahlia_lib/Value.h"

#include <stdckdint.h>

#include <algorithm>
#include <charconv>
#include <compare>
#include <cstddef>
#include <expected>
#include <functional>
#include <limits>
#include <ranges>
#include <string>
#include <variant>

#include "dahlia_lib/Ast.h"
#include "dahlia_lib/RuntimeError.h"

EvalResult add(Value lhs, Value rhs) {
    return std::visit(
        Overloaded{

            [](std::int64_t lhs, std::int64_t rhs) -> EvalResult {
                return checkedAdd(lhs, rhs);
            },
            [](std::int64_t lhs, double rhs) -> EvalResult {
                return checkedDoubleOp(static_cast<double>(lhs), rhs,
                                       std::plus<>{});
            },
            [](std::int64_t lhs, std::string rhs) -> EvalResult {
                rhs.insert_range(rhs.cbegin(), toString(lhs));
                return rhs;
            },
            [](std::int64_t lhs, bool rhs) -> EvalResult {
                return checkedAdd(lhs, static_cast<std::int64_t>(rhs));
            },
            [](double lhs, std::int64_t rhs) -> EvalResult {
                return checkedDoubleOp(lhs, static_cast<double>(rhs),
                                       std::plus<>{});
            },
            [](double lhs, double rhs) -> EvalResult {
                return checkedDoubleOp(lhs, rhs, std::plus<>{});
            },
            [](double lhs, std::string rhs) -> EvalResult {
                rhs.insert_range(rhs.cbegin(), toString(lhs));
                return rhs;
            },
            [](double lhs, bool rhs) -> EvalResult {
                return checkedDoubleOp(lhs, static_cast<double>(rhs),
                                       std::plus<>{});
            },
            [](std::string lhs, std::string rhs) -> EvalResult {
                return Value{lhs.append_range(rhs)};
            },
            [](std::string lhs, double rhs) -> EvalResult {
                return Value{lhs.append_range(toString(rhs))};
            },
            [](std::string lhs, std::int64_t rhs) -> EvalResult {
                return Value{lhs.append_range(toString(rhs))};
            },
            [](std::string lhs, bool rhs) -> EvalResult {
                return Value{lhs.append_range(toString(rhs))};
            },
            [](bool lhs, bool rhs) -> EvalResult {
                return static_cast<std::int64_t>(lhs) +
                       static_cast<std::int64_t>(rhs);
            },
            [](bool lhs, double rhs) -> EvalResult {
                return checkedDoubleOp(static_cast<double>(lhs), rhs,
                                       std::plus<>{});
            },
            [](bool lhs, std::int64_t rhs) -> EvalResult {
                return checkedAdd(static_cast<std::int64_t>(lhs), rhs);
            },
            [](VecValue lhs, VecValue rhs) -> EvalResult {
                if (lhs.type == rhs.type) {
                    lhs.elements.append_range(rhs.elements);
                    return lhs;
                }
                if (lhs.type == Type::vec(rhs.type)) {
                    lhs.elements.emplace_back(std::move(rhs));
                    return lhs;
                }

                return std::unexpected(InvalidOperands{
                    .lhs = Type::vec(lhs.type), .rhs = Type::vec(rhs.type)});
            },
            [](VecValue lhs, auto rhs) -> EvalResult {
                if (lhs.type == typeFor(rhs)) {
                    lhs.elements.push_back(std::move(rhs));
                    return lhs;
                }
                return std::unexpected(InvalidOperands{
                    .lhs = Type::vec(lhs.type), .rhs = typeFor(rhs)});
            },
            [](auto const& lhs, auto const& rhs) -> EvalResult {
                return std::unexpected(
                    InvalidOperands{.lhs = typeFor(lhs), .rhs = typeFor(rhs)});
            }},
        std::move(lhs), std::move(rhs));
}

EvalResult subtract(Value lhs, Value rhs) {
    return arithmeticOp(std::move(lhs), std::move(rhs), checkedSub,
                        [](double lhs, double rhs) {
                            return checkedDoubleOp(lhs, rhs, std::minus<>{});
                        });
}

EvalResult multiply(Value lhs, Value rhs) {
    return arithmeticOp(
        std::move(lhs), std::move(rhs), checkedMul,
        [](double lhs, double rhs) {
            return checkedDoubleOp(lhs, rhs, std::multiplies<>{});
        },
        StringIntPolicy::Repeat);
}

EvalResult divide(Value lhs, Value rhs) {
    return arithmeticOp(std::move(lhs), std::move(rhs), checkedDiv,
                        checkedDoubleDiv);
}

EvalResult modulo(Value lhs, Value rhs) {
    return arithmeticOp(std::move(lhs), std::move(rhs), checkedMod,
                        [](double lhs, double rhs) -> DoubleResult {
                            if (rhs == 0) {
                                return std::unexpected(DivisionByZero{});
                            }
                            return std::fmod(lhs, rhs);
                        });
}

std::partial_ordering operator<=>(Value const& lhs, Value const& rhs) {
    return std::visit<std::partial_ordering>(
        Overloaded{
            [](bool lhs, bool rhs) { return lhs <=> rhs; },
            [](std::int64_t lhs, std::int64_t rhs) { return lhs <=> rhs; },
            [](double lhs, double rhs) { return lhs <=> rhs; },
            [](std::string const& lhs, std::string const& rhs) {
                return lhs <=> rhs;
            },
            [](VecValue const& lhs, VecValue const& rhs) {
                return lhs <=> rhs;
            },
            [](std::int64_t lhs, double rhs) {
                return static_cast<double>(lhs) <=> rhs;
            },
            [](double lhs, std::int64_t rhs) {
                return lhs <=> static_cast<double>(rhs);
            },
            [](bool lhs, std::int64_t rhs) {
                return static_cast<std::int64_t>(lhs) <=> rhs;
            },
            [](std::int64_t lhs, bool rhs) {
                return lhs <=> static_cast<std::int64_t>(rhs);
            },
            [](bool lhs, double rhs) {
                return static_cast<double>(lhs) <=> rhs;
            },
            [](double lhs, bool rhs) {
                return lhs <=> static_cast<double>(rhs);
            },
            [](auto const&, auto const&) {
                return std::partial_ordering::unordered;
            },
        },
        lhs, rhs);
}

std::partial_ordering VecValue::operator<=>(VecValue const& other) const {
    return elements <=> other.elements;
}

CmpResult valueCompare(Value const& lhs, Value const& rhs) {
    return std::visit<CmpResult>(
        Overloaded{
            [](bool lhs, bool rhs) { return lhs <=> rhs; },
            [](std::int64_t lhs, std::int64_t rhs) { return lhs <=> rhs; },
            [](double lhs, double rhs) { return lhs <=> rhs; },
            [](std::string const& lhs, std::string const& rhs) {
                return lhs <=> rhs;
            },
            [](VecValue const& lhs, VecValue const& rhs) -> CmpResult {
                if (lhs.type != rhs.type) {
                    return std::unexpected(
                        InvalidOperands{.lhs = Type::vec(lhs.type),
                                        .rhs = Type::vec(rhs.type)});
                }
                return lhs <=> rhs;
            },
            [](std::int64_t lhs, double rhs) {
                return static_cast<double>(lhs) <=> rhs;
            },
            [](double lhs, std::int64_t rhs) {
                return lhs <=> static_cast<double>(rhs);
            },
            [](bool lhs, std::int64_t rhs) {
                return static_cast<std::int64_t>(lhs) <=> rhs;
            },
            [](std::int64_t lhs, bool rhs) {
                return lhs <=> static_cast<std::int64_t>(rhs);
            },
            [](bool lhs, double rhs) {
                return static_cast<double>(lhs) <=> rhs;
            },
            [](double lhs, bool rhs) {
                return lhs <=> static_cast<double>(rhs);
            },
            [](auto const& lhs, auto const& rhs) {
                return std::unexpected(
                    InvalidOperands{.lhs = typeFor(lhs), .rhs = typeFor(rhs)});
            },
        },
        lhs, rhs);
}

EvalResult eq(Value const& lhs, Value const& rhs) {
    return compareOp(lhs, rhs, std::is_eq);
}

EvalResult neq(Value const& lhs, Value const& rhs) {
    return compareOp(lhs, rhs, std::is_neq);
}

EvalResult lt(Value const& lhs, Value const& rhs) {
    return compareOp(lhs, rhs, std::is_lt);
}

EvalResult gt(Value const& lhs, Value const& rhs) {
    return compareOp(lhs, rhs, std::is_gt);
}

EvalResult le(Value const& lhs, Value const& rhs) {
    return compareOp(lhs, rhs, std::is_lteq);
}

EvalResult ge(Value const& lhs, Value const& rhs) {
    return compareOp(lhs, rhs, std::is_gteq);
}

IntResult checkedAdd(std::int64_t lhs, std::int64_t rhs) noexcept {
    std::int64_t res = 0;
    if (!ckd_add(&res, lhs, rhs)) {
        return res;
    }
    return std::unexpected(ArithmeticOverflow{});
}

IntResult checkedMul(std::int64_t lhs, std::int64_t rhs) noexcept {
    std::int64_t res = 0;
    if (!ckd_mul(&res, lhs, rhs)) {
        return res;
    }
    return std::unexpected(ArithmeticOverflow{});
}

IntResult checkedSub(std::int64_t lhs, std::int64_t rhs) noexcept {
    std::int64_t res = 0;
    if (!ckd_sub(&res, lhs, rhs)) {
        return res;
    }
    return std::unexpected(ArithmeticOverflow{});
}
IntResult checkedDiv(std::int64_t lhs, std::int64_t rhs) noexcept {
    if (rhs == 0) {
        return std::unexpected(DivisionByZero{});
    }
    if (lhs == std::numeric_limits<std::int64_t>::min() && rhs == -1) {
        return std::unexpected(ArithmeticOverflow{});
    }
    return lhs / rhs;
}

IntResult checkedMod(std::int64_t lhs, std::int64_t rhs) noexcept {
    if (rhs == 0) {
        return std::unexpected(DivisionByZero{});
    }
    if (lhs == std::numeric_limits<std::int64_t>::min() && rhs == -1) {
        return 0;
    }
    return lhs % rhs;
}

DoubleResult checkedDoubleDiv(double lhs, double rhs) noexcept {
    if (rhs == 0.0) {
        return std::unexpected(DivisionByZero{});
    }
    auto const result = lhs / rhs;
    if (!std::isfinite(result)) {
        return std::unexpected(ArithmeticOverflow{});
    }
    return result;
}

EvalResult length(Value const& val) noexcept {
    return std::visit(
        Overloaded{[](std::string const& str) -> EvalResult {
                       return static_cast<std::int64_t>(str.length());
                   },
                   [](VecValue const& vec) -> EvalResult {
                       return static_cast<std::int64_t>(vec.elements.size());
                   },
                   [](auto const& type) -> EvalResult {
                       return std::unexpected(
                           InvalidOperand{.type = typeFor(type)});
                   }},
        val);
}

EvalResult negation(Value const& val) {
    return std::visit(
        Overloaded{
            [](std::string const& str) -> EvalResult {
                auto as_int = toInt(str);
                if (!as_int) {
                    return as_int;
                }
                return -*as_int;
            },
            [](std::int64_t val) -> EvalResult { return checkedSub(0, val); },
            [](double val) -> EvalResult { return -val; },
            [](bool val) -> EvalResult {
                return -static_cast<std::int64_t>(val);
            },

            [](auto const& type) -> EvalResult {
                return std::unexpected(InvalidOperand{.type = typeFor(type)});
            }},
        val);
}

EvalResult logicalNot(Value const& val) noexcept { return !toBool(val); }

EvalResult contains(Value const& lhs, Value const& rhs) noexcept {
    // Maybe it should cast to the vec type and ignore conversion errors?
    return std::visit(
        Overloaded{[](VecValue const& lhs, auto const& rhs) -> EvalResult {
                       return std::ranges::contains(lhs.elements, Value{rhs});
                   },
                   [](auto const& lhs, auto const& rhs) -> EvalResult {
                       return std::unexpected(InvalidOperands{
                           .lhs = typeFor(lhs), .rhs = typeFor(rhs)});
                   }},
        lhs, rhs);
}
EvalResult intersect(Value lhs, Value rhs) {
    return std::visit(
        Overloaded{[](VecValue lhs, VecValue rhs) -> EvalResult {
                       if (lhs.type != rhs.type) {
                           return std::unexpected(InvalidOperands{
                               .lhs = lhs.type, .rhs = rhs.type});
                       }
                       std::vector<Value> intersection;

                       for (auto& elem1 : lhs.elements) {
                           if (std::ranges::contains(rhs.elements, elem1)) {
                               intersection.push_back(std::move(elem1));
                           }
                       }
                       return VecValue{.type = lhs.type,
                                       .elements = std::move(intersection)};
                   },
                   [](auto const& lhs, auto const& rhs) -> EvalResult {
                       return std::unexpected(InvalidOperands{
                           .lhs = typeFor(lhs), .rhs = typeFor(rhs)});
                   }},
        std::move(lhs), std::move(rhs));
}

IndexResult index(Value& lhs, Value const& rhs) noexcept {
    return std::visit(
        Overloaded{[](VecValue& lhs, auto const& rhs) -> IndexResult {
                       auto const index = toInt(rhs);
                       if (!index) {
                           return std::unexpected(index.error());
                       }
                       if (*index >= lhs.elements.size() || *index < 0) {
                           return std::unexpected(
                               IndexOutOfBounds{.index = *index});
                       }
                       return &lhs.elements[static_cast<std::size_t>(*index)];
                   },
                   [](auto const& lhs, auto const& rhs) -> IndexResult {
                       return std::unexpected(InvalidOperands{
                           .lhs = typeFor(lhs), .rhs = typeFor(rhs)});
                   }},
        lhs, rhs);
};

Type typeFor(Value const& value) {
    return std::visit(
        Overloaded{
            [](std::int64_t) -> Type { return PrimitiveType::Int; },
            [](double) -> Type { return PrimitiveType::Float; },
            [](bool) -> Type { return PrimitiveType::Bool; },
            [](std::string const&) -> Type { return PrimitiveType::Str; },
            [](VecValue const& vec) -> Type { return Type::vec(vec.type); },
            [](UninitVec) -> Type { return PrimitiveType::EmptyVec; },
            [](std::monostate) -> Type {
                assert(false);
                return PrimitiveType::Int;
            },  // placeholder
        },
        value);
}

EvalResult coerce(Value const& value, Type const& target) noexcept {
    if (target == Type(PrimitiveType::Bool)) {
        return toBool(value);
    }
    if (target == Type(PrimitiveType::Str)) {
        return toString(value);
    }
    if (target == Type(PrimitiveType::Float)) {
        return toFloat(value);
    }
    if (target == Type(PrimitiveType::Int)) {
        return toInt(value);
    }
    return std::unexpected(
        InvalidConversion{.from = typeFor(value), .to = target});
}

using DoubleResult = std::expected<double, RuntimeErrorKind>;
DoubleResult toFloat(Value const& val) noexcept {
    return std::visit(
        Overloaded{
            [](std::int64_t num) -> DoubleResult {
                return static_cast<double>(num);
            },
            [](bool num) -> DoubleResult { return static_cast<double>(num); },
            [](double num) -> DoubleResult { return num; },
            [](std::string const& num) -> DoubleResult {
                double result{};
                auto const [ptr, ec] = std::from_chars(
                    num.data(), num.data() + num.size(), result);

                if (ec == std::errc() && ptr == num.data() + num.size()) {
                    return result;
                }
                if (ec == std::errc::result_out_of_range) {
                    return std::unexpected(ArithmeticOverflow{});
                }

                return std::unexpected(UnparsableString{
                    .val = num, .targetType = Type(PrimitiveType::Float)});
            },
            [](auto const& num) -> DoubleResult {
                return std::unexpected(InvalidConversion{
                    .from = typeFor(num), .to = Type(PrimitiveType::Float)});
            }},
        val);
}

using IntResult = std::expected<std::int64_t, RuntimeErrorKind>;
IntResult toInt(Value const& val) noexcept {
    return std::visit(
        Overloaded{
            [](std::int64_t num) -> IntResult { return num; },
            [](bool num) -> IntResult {
                return static_cast<std::int64_t>(num);
            },
            [](double num) -> IntResult {
                if (num > static_cast<double>(
                              std::numeric_limits<std::int64_t>::max()) ||
                    num < static_cast<double>(
                              std::numeric_limits<std::int64_t>::min())) {
                    return std::unexpected(ArithmeticOverflow{});
                }
                return static_cast<std::int64_t>(num);
            },
            [](std::string const& num) -> IntResult {
                std::int64_t result{};
                auto const [ptr, ec] = std::from_chars(
                    num.data(), num.data() + num.size(), result);

                if (ec == std::errc() && ptr == num.data() + num.size()) {
                    return result;
                }
                if (ec == std::errc::invalid_argument) {
                    return std::unexpected(UnparsableString{
                        .val = num, .targetType = Type(PrimitiveType::Int)});
                }
                if (ec == std::errc::result_out_of_range) {
                    return std::unexpected(ArithmeticOverflow{});
                }
            },
            [](auto const& num) -> IntResult {
                return std::unexpected(InvalidConversion{
                    .from = typeFor(num), .to = Type(PrimitiveType::Int)});
            }},
        val);
}

bool toBool(Value const& val) noexcept {
    return std::visit(
        Overloaded{
            [](std::monostate) { return false; },  // Should not happen
            [](UninitVec val) { return false; },   // Should not happen
            [](bool val) { return val; },
            [](std::int64_t val) { return val != 0; },
            [](double val) { return val != 0.0; },
            [](std::string const& val) { return !val.empty(); },
            [](VecValue const& val) { return !val.elements.empty(); },
        },
        val);
};

std::ostream& operator<<(std::ostream& oss, Value const& value) {
    return oss << std::format("{}", value);
}

std::ostream& operator<<(std::ostream& oss, VecValue const& value) {
    return oss << std::format("{}", value);
}

inline std::string toString(Value const& value) {
    return std::visit(
        Overloaded{
            [](std::monostate) -> std::string {
                return "None";
            },  // Never happens
            [](bool val) -> std::string { return val ? "true" : "false"; },
            [](std::int64_t val) -> std::string { return std::to_string(val); },
            [](double val) -> std::string {
                return std::format("{:.4g}", val);
            },
            [](std::string const& val) -> std::string { return val; },
            [](VecValue const& vec) -> std::string { return toString(vec); },
            [](UninitVec const&) -> std::string { return "[]"; }},
        value);
}

inline std::string toString(VecValue const& vec) {
    std::string out = "[";
    out.reserve(vec.elements.size() * 3);

    if (vec.elements.size() > 0) {
        out.append(toString(vec.elements[0]));
    }
    for (auto const& element : vec.elements | std::views::drop(1)) {
        out += ", ";
        out += toString(element);
    }
    out += "]";
    return out;
}

std::string repeat(std::string const& str, std::int64_t n) {
    if (n <= 0) {
        return {};
    }
    std::string result;
    result.reserve(str.size() * n);
    for (std::int64_t i = 0; i < n; ++i) {
        result.append(str);
    }
    return result;
}
