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

bool Block::operator==(Block const&) const = default;
bool Range::operator==(Range const&) const = default;
bool ForLoop::operator==(ForLoop const&) const = default;

bool IfStmt::operator==(IfStmt const& other) const {
    return if_cond == other.if_cond && *if_block == *other.if_block &&
           else_if_branches == other.else_if_branches &&
           static_cast<bool>(else_block) ==
               static_cast<bool>(other.else_block) &&
           (!else_block || *else_block == *other.else_block);
}

bool WhileLoop::operator==(WhileLoop const& other) const {
    return condition == other.condition && *block == *other.block;
}
