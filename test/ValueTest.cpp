#include "dahlia_lib/Ast.h"
#include "dahlia_lib/Value.h"
#include "doctest.h"

namespace doctest {
template <>
struct StringMaker<EvalResult> {
    static String convert(EvalResult const& result) {
        if (!result) {
            return String{"Error"};
        }
        return String{std::format("{}", result.value()).c_str()};
    }
};
}  // namespace doctest

constexpr auto INT_MAX = std::numeric_limits<std::int64_t>::max();
constexpr auto DBL_MAX = std::numeric_limits<double>::max();

// ADD

TEST_CASE("add - arithmetic") {
    CHECK(add(1, 2) == 3);
    CHECK(add(1, 2.0) == 3.0);
    CHECK(add(1.0, 2.0) == 3.0);
    CHECK(add(1.0, 1) == 2.0);
    CHECK(add(true, 1) == 2);
    CHECK(add(true, 1.0) == 2.0);
    CHECK(add(true, true) == 2);
}

TEST_CASE("add - string concatenation") {
    CHECK(add("hello", "world") == "helloworld");
    CHECK(add("x", 1) == "x1");
    CHECK(add("x", 1.0) == "x1");
    CHECK(add("x", true) == "xtrue");
    CHECK(add(1, "x") == "1x");
    CHECK(add(1.0, "x") == "1x");
    CHECK(add(1.23, "x") == "1.23x");
}

TEST_CASE("add - vec append literal") {
    auto vec = VecValue{.type = PrimitiveType::Int, .elements = {1, 2}};
    CHECK(add(vec, 3) ==
          VecValue{.type = PrimitiveType::Int, .elements = {1, 2, 3}});
}

TEST_CASE("add - nested vec append") {
    auto inner = VecValue{.type = PrimitiveType::Int, .elements = {1, 2}};
    auto outer = VecValue{.type = Type::vec(PrimitiveType::Int),
                          .elements = {Value{inner}}};
    auto to_append = VecValue{.type = PrimitiveType::Int, .elements = {3}};
    auto expected = VecValue{.type = Type::vec(PrimitiveType::Int),
                             .elements = {Value{inner}, Value{to_append}}};
    CHECK(add(outer, to_append) == expected);
}

TEST_CASE("add - vec concat") {
    auto vec = VecValue{.type = PrimitiveType::Int, .elements = {1, 2}};
    CHECK(add(vec, VecValue{.type = PrimitiveType::Int, .elements = {3, 4}}) ==
          VecValue{.type = PrimitiveType::Int, .elements = {1, 2, 3, 4}});
}

TEST_CASE("add - overflow") {
    CHECK(add(INT_MAX, 1) == std::unexpected(ArithmeticOverflow{}));
    CHECK(add(DBL_MAX, DBL_MAX) == std::unexpected(ArithmeticOverflow{}));
    CHECK(add(1, INT_MAX) == std::unexpected(ArithmeticOverflow{}));
}

TEST_CASE("add - invalid operands") {
    CHECK(
        add(VecValue{.type = PrimitiveType::Int, .elements = {1, 2}},
            VecValue{.type = PrimitiveType::Str, .elements = {"a"}}) ==
        std::unexpected(InvalidOperands{.lhs = Type::vec(PrimitiveType::Int),
                                        .rhs = Type::vec(PrimitiveType::Str)}));

    CHECK(add(VecValue{.type = PrimitiveType::Int, .elements = {1}}, "hello") ==
          std::unexpected(InvalidOperands{.lhs = Type::vec(PrimitiveType::Int),
                                          .rhs = PrimitiveType::Str}));
}
