#include "dahlia_lib/Value.h"

#include <expected>
#include <ranges>
#include <string>
#include <variant>

#include "dahlia_lib/RuntimeError.h"

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

inline std::string toString(Value const& value) {
    return std::visit(
        Overloaded{
            [](std::monostate) -> std::string { return "None"; },
            [](bool val) -> std::string { return val ? "True" : "False"; },
            [](std::int64_t val) -> std::string { return std::to_string(val); },
            [](double val) -> std::string { return std::format("{}", val); },
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
    }
    out += "]";
    return out;
}
