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

TEST_CASE("eq - int") {
    CHECK(eq(1, 1) == true);
    CHECK(eq(1, 2) == false);
    CHECK(eq(-1, -1) == true);
}

TEST_CASE("eq - double") {
    CHECK(eq(1.0, 1.0) == true);
    CHECK(eq(1.0, 2.0) == false);
}

TEST_CASE("eq - bool") {
    CHECK(eq(true, true) == true);
    CHECK(eq(false, false) == true);
    CHECK(eq(true, false) == false);
}

TEST_CASE("eq - string") {
    CHECK(eq("hello", "hello") == true);
    CHECK(eq("hello", "world") == false);
    CHECK(eq("", "") == true);
}

TEST_CASE("eq - numeric promotions") {
    CHECK(eq(1, 1.0) == true);
    CHECK(eq(1.0, 1) == true);
    CHECK(eq(true, 1) == true);
    CHECK(eq(1, true) == true);
    CHECK(eq(false, 0) == true);
    CHECK(eq(true, 1.0) == true);
    CHECK(eq(1.0, false) == false);
}

TEST_CASE("eq - vec") {
    auto vec1 = VecValue{.type = PrimitiveType::Int, .elements = {1, 2}};
    auto vec3 = VecValue{.type = PrimitiveType::Int, .elements = {1, 3}};
    CHECK(eq(vec1, vec1) == true);
    CHECK(eq(vec1, vec3) == false);
}

TEST_CASE("eq - nested vec") {
    auto inner1 = VecValue{.type = PrimitiveType::Int, .elements = {1, 2}};
    auto inner2 = VecValue{.type = PrimitiveType::Int, .elements = {1, 2}};
    auto outer1 = VecValue{.type = Type::vec(PrimitiveType::Int),
                           .elements = {Value{inner1}}};
    auto outer2 = VecValue{.type = Type::vec(PrimitiveType::Int),
                           .elements = {Value{inner2}}};
    CHECK(eq(outer1, outer2) == true);
}

TEST_CASE("eq - invalid operands") {
    CHECK(eq(1, "hello") ==
          std::unexpected(InvalidOperands{.lhs = PrimitiveType::Int,
                                          .rhs = PrimitiveType::Str}));
    CHECK(eq("hello", 1) ==
          std::unexpected(InvalidOperands{.lhs = PrimitiveType::Str,
                                          .rhs = PrimitiveType::Int}));

    CHECK(
        eq(1, VecValue{.type = PrimitiveType::Int, .elements = {1}}) ==
        std::unexpected(InvalidOperands{.lhs = PrimitiveType::Int,
                                        .rhs = Type::vec(PrimitiveType::Int)}));
}

TEST_CASE("eq - incompatible vector inner types") {
    CHECK(
        eq(VecValue{.type = PrimitiveType::Int, .elements = {1}},
           VecValue{.type = PrimitiveType::Str, .elements = {"a"}}) ==
        std::unexpected(InvalidOperands{.lhs = Type::vec(PrimitiveType::Int),
                                        .rhs = Type::vec(PrimitiveType::Str)}));
}

TEST_CASE("lt - arithmetic") {
    CHECK(lt(1, 2) == true);
    CHECK(lt(2, 1) == false);
    CHECK(lt(1, 1) == false);
    CHECK(lt(1.0, 2.0) == true);
    CHECK(lt(1, 2.0) == true);
    CHECK(lt(1.0, 2) == true);
    CHECK(lt(false, true) == true);
    CHECK(lt(false, 1) == true);
    CHECK(lt(true, 2) == true);
}

TEST_CASE("lt - string") {
    CHECK(lt("abc", "abd") == true);
    CHECK(lt("abc", "abcd") == true);
    CHECK(lt("b", "abc") == false);
    CHECK(lt("", "a") == true);
    CHECK(lt("abc", "abc") == false);
}

TEST_CASE("lt - vec") {
    CHECK(lt(VecValue{.type = PrimitiveType::Int, .elements = {1, 2}},
             VecValue{.type = PrimitiveType::Int, .elements = {1, 3}}) == true);
    CHECK(lt(VecValue{.type = PrimitiveType::Int, .elements = {1, 2}},
             VecValue{.type = PrimitiveType::Int, .elements = {1, 2, 3}}) ==
          true);
    CHECK(lt(VecValue{.type = PrimitiveType::Int, .elements = {1, 2}},
             VecValue{.type = PrimitiveType::Int, .elements = {1, 2}}) ==
          false);
}

TEST_CASE("lt - invalid operands") {
    CHECK(lt(1, "hello") ==
          std::unexpected(InvalidOperands{.lhs = PrimitiveType::Int,
                                          .rhs = PrimitiveType::Str}));
    CHECK(
        lt(VecValue{.type = PrimitiveType::Int, .elements = {1}},
           VecValue{.type = PrimitiveType::Str, .elements = {"a"}}) ==
        std::unexpected(InvalidOperands{.lhs = Type::vec(PrimitiveType::Int),
                                        .rhs = Type::vec(PrimitiveType::Str)}));
}
