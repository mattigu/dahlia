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
    T value_;
    Position pos_{};
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

using Expr = std::variant<IntLiteral, FloatLiteral, BoolLiteral, StringLiteral,
                          VecLiteral, Identifier, FunctionCall, IndexExpr,
                          NegExpr, LengthExpr, NotExpr>;
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

using StatementKind = std::variant<BreakStmt, ContinueStmt, LetBinding>;
using StatementNode = Node<StatementKind>;

struct Block {
    std::vector<StatementNode> statements;

    bool operator==(Block const& other) const = default;
};
using BlockNode = Node<Block>;

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
