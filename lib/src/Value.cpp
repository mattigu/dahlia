#include "dahlia_lib/Value.h"

std::ostream& operator<<(std::ostream& oss, Value const& value) {
    return oss << std::format("{}", value);
}

inline std::string valueToString(Value const& value) {
    return std::visit(
        Overloaded{
            [](std::monostate) -> std::string { return "None"; },
            [](bool val) -> std::string { return val ? "True" : "False"; },
            [](std::int64_t val) -> std::string { return std::to_string(val); },
            [](double val) -> std::string { return std::format("{}", val); },
            [](std::string const& val) -> std::string { return val; },
            [](VecValue const& vec) -> std::string { return vecToString(vec); },
            [](UninitVec const&) -> std::string { return "[]"; }},
        value);
}

inline std::string vecToString(VecValue const& vec) {
    std::string out = "[";
    out.reserve(vec.elements.size() * 2);

    for (auto const& element : vec.elements) {
        out += valueToString(element);
    }
    out += "]";
    return out;
}
