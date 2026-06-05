#include "dahlia_lib/Ast.h"

#include "magic_enum/magic_enum.hpp"

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

bool VecLiteral::operator==(VecLiteral const& other) const = default;
bool FunctionCall::operator==(FunctionCall const& other) const = default;

bool Block::operator==(Block const&) const = default;
bool Range::operator==(Range const&) const = default;
bool ForLoop::operator==(ForLoop const&) const = default;

AssignBase::AssignBase(LValue target, ExprNode value)
    : target(std::move(target)), value(std::move(value)) {}

BinaryBase::BinaryBase(ExprNode left, ExprNode right)
    : left(std::make_unique<ExprNode>(std::move(left))),
      right(std::make_unique<ExprNode>(std::move(right))) {}

bool BinaryBase::operator==(BinaryBase const& other) const {
    return *left == *other.left && *right == *other.right;
}

UnaryBase::UnaryBase(ExprNode operand)
    : operand(std::make_unique<ExprNode>(std::move(operand))) {}

bool UnaryBase::operator==(UnaryBase const& other) const {
    return *operand == *other.operand;
}

bool IndexExpr::operator==(IndexExpr const& other) const {
    return *object == *other.object && *index == *other.index;
}

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

VecType::VecType(Type inner)
    : inner{std::make_unique<Type>(std::move(inner))} {}

VecType::VecType(VecType const& other)
    : inner(other.inner ? std::make_unique<Type>(*other.inner) : nullptr) {}

bool VecType::operator==(VecType const& other) const {
    return *inner == *other.inner;
}

VecType& VecType::operator=(VecType const& other) {
    if (this != &other) {
        inner = other.inner ? std::make_unique<Type>(*other.inner) : nullptr;
    }
    return *this;
}

Type::Type(PrimitiveType primitive) : value_(primitive) {}

Type::Type(VecType vec) : value_(std::move(vec)) {}

Type Type::vec(Type inner) { return Type{VecType{std::move(inner)}}; }

bool Type::isVec() const noexcept { return std::holds_alternative<VecType>(value_); }

Type const& Type::element() const {
    assert(isVec());
    return *std::get<VecType>(value_).inner;
}

bool Type::isPrimitive() const noexcept {
    return std::holds_alternative<PrimitiveType>(value_);
}

PrimitiveType Type::asPrimitive() const {
    assert(isPrimitive());
    return std::get<PrimitiveType>(value_);
}
