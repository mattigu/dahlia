#include <cassert>
#include <cstdint>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "Position.h"

template <typename T>
struct Node {
public:
    template <typename... Args>
    explicit Node(Position pos, Args&&... args)
        : pos_{pos}, value_(std::forward<Args>(args)...) {}

    [[nodiscard]] Position pos() const noexcept { return pos_; }

    T const& operator*() const { return value_; }
    T& operator*() { return value_; }

    T const* operator->() const { return &value_; }
    T* operator->() { return &value_; }

    bool operator==(Node const& other) const { return value_ == other.value_; }

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

struct IfStatement {};
using StatementKind = std::variant<IfStatement>;
using StatementNode = Node<StatementKind>;

struct Block {
    std::vector<StatementNode> statements;
};
using BlockNode = Node<Block>;

struct Param {
    TypeNode type;
    std::string identifier;
    bool mut;
};

using ParamNode = Node<Param>;

struct Function {
    std::string identifier;

    std::vector<ParamNode> params;
    std::optional<TypeNode> return_type;
    BlockNode block;
};
using FunctionNode = Node<Function>;

struct Program {
    std::vector<FunctionNode> functions;
};

using ProgramNode = Node<Program>;

// Formatters only below

template <>
struct std::formatter<PrimitiveType> : std::formatter<std::string_view> {
    auto format(PrimitiveType type, std::format_context& ctx) const {
        std::string_view name = "?";

        switch (type) {
            case PrimitiveType::Int:
                name = "int";
                break;
            case PrimitiveType::Float:
                name = "float";
                break;
            case PrimitiveType::Bool:
                name = "bool";
                break;
            case PrimitiveType::Str:
                name = "str";
                break;
        }

        return std::formatter<std::string_view>::format(name, ctx);
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

std::ostream& operator<<(std::ostream& oss, Type const& type);
std::ostream& operator<<(std::ostream& oss, PrimitiveType type);
