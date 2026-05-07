#include "Ast.h"

#include "magic_enum/magic_enum.hpp"

bool VecType::operator==(VecType const& other) const {
    return *inner == *other.inner;
}

std::ostream& operator<<(std::ostream& oss, Type const& type) {
    return oss << std::format("{}", type);
}
std::ostream& operator<<(std::ostream& oss, PrimitiveType type) {
    return oss << std::format("{}", type);
}
std::ostream& operator<<(std::ostream& oss, Param const& param) {
    return oss << std::format("{}", param);
}

std::string toString(PrimitiveType type) {
    return std::string(magic_enum::enum_name(type));
}
