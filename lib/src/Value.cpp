#include "dahlia_lib/Value.h"

#include <stdckdint.h>

#include <expected>
#include <limits>
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
                rhs.insert_range(rhs.cbegin(), std::to_string(lhs));
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
                rhs.insert_range(rhs.cbegin(), std::to_string(lhs));
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

EvalResult checkedAdd(std::int64_t lhs, std::int64_t rhs) {
    std::int64_t res = 0;
    if (!ckd_add(&res, lhs, rhs)) {
        return res;
    }
    return std::unexpected(ArithmeticOverflow{});
}

EvalResult checkedMul(std::int64_t lhs, std::int64_t rhs) {
    std::int64_t res = 0;
    if (!ckd_mul(&res, lhs, rhs)) {
        return res;
    }
    return std::unexpected(ArithmeticOverflow{});
}

EvalResult checkedSub(std::int64_t lhs, std::int64_t rhs) {
    std::int64_t res = 0;
    if (!ckd_sub(&res, lhs, rhs)) {
        return res;
    }
    return std::unexpected(ArithmeticOverflow{});
}
EvalResult checkedDiv(std::int64_t lhs, std::int64_t rhs) {
    if (rhs == 0) {
        return std::unexpected(DivisionByZero{});
    }
    if (lhs == std::numeric_limits<std::int64_t>::min() && rhs == -1) {
        return std::unexpected(ArithmeticOverflow{});
    }
    return lhs / rhs;
}

EvalResult checkedDoubleDiv(double lhs, double rhs) {
    if (rhs == 0.0) {
        return std::unexpected(DivisionByZero{});
    }
    auto const result = lhs / rhs;
    if (!std::isfinite(result)) {
        return std::unexpected(ArithmeticOverflow{});
    }
    return result;
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
            [](std::monostate) -> std::string { return "None"; },
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
    out.reserve(vec.elements.size() * 2);

    for (auto const& element : vec.elements) {
        out += toString(element);
        out += ", ";
    }
    out += "]";
    return out;
}
