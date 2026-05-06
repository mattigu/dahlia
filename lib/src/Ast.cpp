#include "Ast.h"

bool VecType::operator==(VecType const& other) const {
    return *inner == *other.inner;
}

std::ostream& operator<<(std::ostream& oss, Type const& type) {
    return oss << std::format("{}", type);
}
std::ostream& operator<<(std::ostream& oss, PrimitiveType type) {
    return oss << std::format("{}", type);
}
