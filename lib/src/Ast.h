#pragma once
#include <cassert>
#include <cstdint>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "Position.h"

template <typename T>
struct Node {
public:
    template <typename... Args>
    Node(Position pos, Args&&... args)
        : pos_{pos}, value_(std::forward<Args>(args)...) {}

    [[nodiscard]] Position pos() const noexcept { return pos_; }

    T const& operator*() const { return value_; }
    T& operator*() { return value_; }

    T const* operator->() const { return &value_; }
    T* operator->() { return &value_; }

    bool operator==(Node const& other) const = default;

private:
    Position pos_;
    T value_;
};

class Type;
using TypeNode = Node<Type>;

enum class PrimitiveType : std::int8_t { Int, Float, Bool, Str };

struct VecType {
    std::unique_ptr<Type> inner;

    bool operator==(VecType const& other) const;
};

class Type {
public:
    Type(PrimitiveType primitive) : value_(primitive) {}

    Type(VecType vec) : value_(std::move(vec)) {}

    static Type vec(Type inner) {
        return Type(VecType{.inner = std::make_unique<Type>(std::move(inner))});
    }

    [[nodiscard]] bool isVec() const {
        return std::holds_alternative<VecType>(value_);
    }

    //! Returns the inner type of Vec.
    [[nodiscard]] Type const& element() const {
        assert(isVec());
        return *std::get<VecType>(value_).inner;
    }

    [[nodiscard]] bool isPrimitive() const {
        return std::holds_alternative<PrimitiveType>(value_);
    }

    [[nodiscard]] PrimitiveType asPrimitive() const {
        assert(isPrimitive());
        return std::get<PrimitiveType>(value_);
    }

    bool operator==(Type const& other) const = default;

private:
    std::variant<PrimitiveType, VecType> value_;
};

struct IntLiteral {
    std::int64_t value;
    bool operator==(IntLiteral const& other) const = default;
};

struct FloatLiteral {
    double value;
    bool operator==(FloatLiteral const& other) const = default;
};

struct BoolLiteral {
    bool value;
    bool operator==(BoolLiteral const& other) const = default;
};

struct StringLiteral {
    std::string value;
    bool operator==(StringLiteral const& other) const = default;
};
struct Identifier {
    std::string identifier;
    bool operator==(Identifier const& other) const = default;
};

struct VecLiteral;
struct FunctionCall;
struct IndexExpr;
struct NegExpr;
struct LengthExpr;
struct NotExpr;
struct MulExpr;
struct DivExpr;
struct AddExpr;
struct SubExpr;
struct IntersectExpr;
struct InExpr;
struct EqExpr;
struct NeqExpr;
struct LtExpr;
struct GtExpr;
struct LeExpr;
struct GeExpr;
struct AndExpr;
struct OrExpr;
struct FilterExpr;
struct MapExpr;

using Expr =
    std::variant<IntLiteral, FloatLiteral, BoolLiteral, StringLiteral,
                 VecLiteral, Identifier, FunctionCall, IndexExpr, NegExpr,
                 LengthExpr, NotExpr, MulExpr, DivExpr, AddExpr, SubExpr,
                 IntersectExpr, InExpr, EqExpr, NeqExpr, LtExpr, GtExpr, LeExpr,
                 GeExpr, AndExpr, OrExpr, FilterExpr, MapExpr>;
using ExprNode = Node<Expr>;

struct FunctionCall {
    std::string identifier;
    std::vector<ExprNode> args;
    bool operator==(FunctionCall const& other) const;
};

struct VecLiteral {
    std::vector<ExprNode> elements;
    bool operator==(VecLiteral const& other) const;
};

struct IndexExpr {
    std::unique_ptr<ExprNode> object;
    std::unique_ptr<ExprNode> index;
    bool operator==(IndexExpr const& other) const;
};

struct BinaryBase {
    std::unique_ptr<ExprNode> left;
    std::unique_ptr<ExprNode> right;

    BinaryBase(ExprNode left, ExprNode right);

    bool operator==(BinaryBase const& other) const;
};

struct UnaryBase {
    std::unique_ptr<ExprNode> operand;

    UnaryBase(ExprNode operand);

    bool operator==(UnaryBase const& other) const;
};

struct NegExpr : UnaryBase {
    using UnaryBase::UnaryBase;
};
struct NotExpr : UnaryBase {
    using UnaryBase::UnaryBase;
};
struct LengthExpr : UnaryBase {
    using UnaryBase::UnaryBase;
};
struct MulExpr : BinaryBase {
    using BinaryBase::BinaryBase;
};
struct DivExpr : BinaryBase {
    using BinaryBase::BinaryBase;
};

struct AddExpr : BinaryBase {
    using BinaryBase::BinaryBase;
};
struct SubExpr : BinaryBase {
    using BinaryBase::BinaryBase;
};
struct IntersectExpr : BinaryBase {
    using BinaryBase::BinaryBase;
};
struct InExpr : BinaryBase {
    using BinaryBase::BinaryBase;
};
struct EqExpr : BinaryBase {
    using BinaryBase::BinaryBase;
};
struct NeqExpr : BinaryBase {
    using BinaryBase::BinaryBase;
};
struct LtExpr : BinaryBase {
    using BinaryBase::BinaryBase;
};
struct GtExpr : BinaryBase {
    using BinaryBase::BinaryBase;
};
struct LeExpr : BinaryBase {
    using BinaryBase::BinaryBase;
};
struct GeExpr : BinaryBase {
    using BinaryBase::BinaryBase;
};
struct AndExpr : BinaryBase {
    using BinaryBase::BinaryBase;
};
struct OrExpr : BinaryBase {
    using BinaryBase::BinaryBase;
};
struct FilterExpr : BinaryBase {
    using BinaryBase::BinaryBase;
};
struct MapExpr : BinaryBase {
    using BinaryBase::BinaryBase;
};

inline BinaryBase::BinaryBase(ExprNode left, ExprNode right)
    : left(std::make_unique<ExprNode>(std::move(left))),
      right(std::make_unique<ExprNode>(std::move(right))) {}

inline bool BinaryBase::operator==(BinaryBase const& other) const {
    return *left == *other.left && *right == *other.right;
}

inline UnaryBase::UnaryBase(ExprNode operand)
    : operand(std::make_unique<ExprNode>(std::move(operand))) {}

inline bool UnaryBase::operator==(UnaryBase const& other) const {
    return *operand == *other.operand;
}

inline bool IndexExpr::operator==(IndexExpr const& other) const {
    return *object == *other.object && *index == *other.index;
}
inline bool VecLiteral::operator==(VecLiteral const& other) const = default;
inline bool FunctionCall::operator==(FunctionCall const& other) const = default;

struct BreakStmt {
    bool operator==(BreakStmt const& other) const = default;
};
struct ContinueStmt {
    bool operator==(ContinueStmt const& other) const = default;
};

struct ReturnStmt {
    std::optional<ExprNode> value;
    bool operator==(ReturnStmt const& other) const = default;
};

struct ForLoop {
    bool operator==(ForLoop const& other) const = default;
};
struct LetBinding {
    std::string identifier;
    bool mut;
    std::optional<TypeNode> type;
    ExprNode value;

    bool operator==(LetBinding const& other) const = default;
};

struct LValue {
    std::string identifier;
    std::vector<ExprNode> indices;
    bool operator==(LValue const& other) const = default;
};

struct AssignBase {
    LValue target;
    ExprNode value;
    AssignBase(LValue target, ExprNode value)
        : target(std::move(target)), value(std::move(value)) {}
    bool operator==(AssignBase const& other) const = default;
};

struct AssignStmt : AssignBase {
    using AssignBase::AssignBase;
};
struct AddAssignStmt : AssignBase {
    using AssignBase::AssignBase;
};
struct SubAssignStmt : AssignBase {
    using AssignBase::AssignBase;
};
struct MulAssignStmt : AssignBase {
    using AssignBase::AssignBase;
};
struct DivAssignStmt : AssignBase {
    using AssignBase::AssignBase;
};
struct ModAssignStmt : AssignBase {
    using AssignBase::AssignBase;
};

struct Block;
using BlockNode = Node<Block>;
struct IfStmt;

using StatementKind =
    std::variant<BreakStmt, ContinueStmt, ReturnStmt, LetBinding, AssignStmt,
                 AddAssignStmt, SubAssignStmt, MulAssignStmt, DivAssignStmt,
                 ModAssignStmt, FunctionCall, Block, IfStmt>;
using StatementNode = Node<StatementKind>;

struct Block {
    std::vector<StatementNode> statements;

    bool operator==(Block const& other) const;
};

struct IfStmt {
    ExprNode if_cond;
    std::unique_ptr<BlockNode> if_block;

    std::vector<std::pair<ExprNode, BlockNode>> else_if_branches;
    std::unique_ptr<BlockNode> else_block;

    IfStmt(ExprNode if_cond, BlockNode if_block,
           std::vector<std::pair<ExprNode, BlockNode>> else_if_branches,
           std::optional<BlockNode> else_block)
        : if_cond(std::move(if_cond)),
          if_block(std::make_unique<BlockNode>(std::move(if_block))),
          else_if_branches(std::move(else_if_branches)),
          else_block(else_block
                         ? std::make_unique<BlockNode>(std::move(*else_block))
                         : nullptr) {}

    bool operator==(IfStmt const& other) const;
};

inline bool Block::operator==(Block const&) const = default;
inline bool IfStmt::operator==(IfStmt const& other) const {
    return if_cond == other.if_cond && *if_block == *other.if_block &&
           else_if_branches == other.else_if_branches &&
           static_cast<bool>(else_block) ==
               static_cast<bool>(other.else_block) &&
           (!else_block || *else_block == *other.else_block);
}

struct Param {
    TypeNode type;
    std::string identifier;
    bool mut;

    bool operator==(Param const& other) const = default;
};
using ParamNode = Node<Param>;

struct Function {
    std::string identifier;

    std::vector<ParamNode> params;
    std::optional<TypeNode> return_type;
    BlockNode block;

    bool operator==(Function const& other) const = default;
};
using FunctionNode = Node<Function>;

struct Program {
    std::unordered_map<std::string, FunctionNode> functions;
};

using ProgramNode = Node<Program>;

// Formatters only below

std::string toString(PrimitiveType type);

template <typename T>
struct std::formatter<Node<T>> : std::formatter<std::string> {
    auto format(Node<T> const& node, std::format_context& ctx) const {
        if constexpr (std::formattable<T, char>) {
            return std::format_to(ctx.out(), "{{{} @ {}}}", *node, node.pos());
        } else {
            return std::format_to(ctx.out(), "{{Node @ {}}}", node.pos());
        }
    }
};

template <>
struct std::formatter<PrimitiveType> : std::formatter<std::string_view> {
    auto format(PrimitiveType type, std::format_context& ctx) const {
        return std::formatter<std::string_view>::format(toString(type), ctx);
    }
};

template <>
struct std::formatter<Type> : std::formatter<std::string_view> {
    auto format(Type const& type, std::format_context& ctx) const {
        if (type.isPrimitive()) {
            return std::formatter<std::string_view>::format(
                std::format("{}", type.asPrimitive()), ctx);
        }

        return std::formatter<std::string_view>::format(
            std::format("[{}]", type.element()), ctx);
    }
};

template <>
struct std::formatter<Param> : std::formatter<std::string_view> {
    static auto format(Param const& param, std::format_context& ctx) {
        return std::format_to(ctx.out(), "{{{}{}: {}}}",
                              param.mut ? "mut " : "", param.identifier,
                              param.type);
    }
};

std::ostream& operator<<(std::ostream& oss, Type const& type);
std::ostream& operator<<(std::ostream& oss, PrimitiveType type);
std::ostream& operator<<(std::ostream& oss, Param const& param);

template <typename T>
std::ostream& operator<<(std::ostream& oss, Node<T> const& node) {
    return oss << std::format("{}", node);
}
