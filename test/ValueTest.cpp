#include "dahlia_lib/Ast.h"
#include "dahlia_lib/RuntimeError.h"
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
constexpr auto INT_MIN = std::numeric_limits<std::int64_t>::min();

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
    auto outer =
        VecValue{.type = Type::vec(PrimitiveType::Int), .elements = {inner}};
    auto to_append = VecValue{.type = PrimitiveType::Int, .elements = {3}};
    auto expected = VecValue{.type = Type::vec(PrimitiveType::Int),
                             .elements = {inner, to_append}};
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
    auto outer1 =
        VecValue{.type = Type::vec(PrimitiveType::Int), .elements = {inner1}};
    auto outer2 =
        VecValue{.type = Type::vec(PrimitiveType::Int), .elements = {inner2}};
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

TEST_CASE("sub - int int") {
    CHECK(subtract(5, 3) == 2);
    CHECK(subtract(3, 5) == -2);
    CHECK(subtract(0, 0) == 0);
    CHECK(subtract(INT_MIN, 1) == std::unexpected(ArithmeticOverflow{}));
}

TEST_CASE("sub - float float") {
    CHECK(subtract(5.0, 3.0) == 2.0);
    CHECK(subtract(DBL_MAX, -DBL_MAX) == std::unexpected(ArithmeticOverflow{}));
}

TEST_CASE("sub - int float promotion") {
    CHECK(subtract(5, 2.0) == 3.0);
    CHECK(subtract(2.0, 5) == -3.0);
}

TEST_CASE("sub - bool promotion") {
    CHECK(subtract(true, true) == 0);
    CHECK(subtract(true, false) == 1);
    CHECK(subtract(false, true) == -1);
    CHECK(subtract(true, 1) == 0);
    CHECK(subtract(1, true) == 0);
    CHECK(subtract(true, 1.0) == 0.0);
    CHECK(subtract(1.0, true) == 0.0);
}

TEST_CASE("sub - str int") {
    CHECK(subtract("10", 3) == 7);
    CHECK(subtract(3, "10") == -7);
    CHECK(subtract("foo", 3) ==
          std::unexpected(UnparsableString{.val = "foo",
                                           .targetType = PrimitiveType::Int}));
}

TEST_CASE("sub - str float") {
    CHECK(subtract("3.5", 1.0) == 2.5);
    CHECK(subtract(1.0, "3.5") == -2.5);
    CHECK(subtract("foo", 1.0) ==
          std::unexpected(UnparsableString{
              .val = "foo", .targetType = PrimitiveType::Float}));
}

TEST_CASE("sub - str bool") {
    CHECK(subtract("5", true) == 4);
    CHECK(subtract("foo", true) ==
          std::unexpected(UnparsableString{.val = "foo",
                                           .targetType = PrimitiveType::Int}));
}

TEST_CASE("sub - str str") {
    CHECK(subtract("5.0", "3.0") == 2.0);
    CHECK(subtract("foo", "3.0") ==
          std::unexpected(UnparsableString{
              .val = "foo", .targetType = PrimitiveType::Float}));
    CHECK(subtract("5.0", "bar") ==
          std::unexpected(UnparsableString{
              .val = "bar", .targetType = PrimitiveType::Float}));
}

TEST_CASE("sub - invalid operands") {
    CHECK(subtract(VecValue{.type = PrimitiveType::Int, .elements = {1}}, 1) ==
          std::unexpected(InvalidOperands{.lhs = Type::vec(PrimitiveType::Int),
                                          .rhs = PrimitiveType::Int}));
}

TEST_CASE("multiply - int int") {
    CHECK(multiply(5, 3) == 15);
    CHECK(multiply(0, 100) == 0);
    CHECK(multiply(INT64_MAX, 2) == std::unexpected(ArithmeticOverflow{}));
}

TEST_CASE("multiply - float float") {
    CHECK(multiply(2.0, 3.0) == 6.0);
    CHECK(multiply(DBL_MAX, 2.0) == std::unexpected(ArithmeticOverflow{}));
}

TEST_CASE("multiply - int float promotion") {
    CHECK(multiply(3, 2.0) == 6.0);
    CHECK(multiply(2.0, 3) == 6.0);
}

TEST_CASE("multiply - bool promotion") {
    CHECK(multiply(true, true) == 1);
    CHECK(multiply(true, false) == 0);
    CHECK(multiply(true, 5) == 5);
    CHECK(multiply(5, true) == 5);
    CHECK(multiply(true, 2.0) == 2.0);
    CHECK(multiply(2.0, true) == 2.0);
}

TEST_CASE("multiply - str int repeat") {
    CHECK(multiply("ab", 3) == "ababab");
    CHECK(multiply(3, "ab") == "ababab");
    CHECK(multiply("ab", 0) == "");
    CHECK(multiply("ab", -1) == "");
}

TEST_CASE("multiply - str float") {
    CHECK(multiply("3.0", 2.0) == 6.0);
    CHECK(multiply(2.0, "3.0") == 6.0);
    CHECK(multiply("foo", 2.0) ==
          std::unexpected(UnparsableString{
              .val = "foo", .targetType = PrimitiveType::Float}));
}

TEST_CASE("multiply - str bool") {
    CHECK(multiply("5", true) == 5);
    CHECK(multiply("foo", true) ==
          std::unexpected(UnparsableString{.val = "foo",
                                           .targetType = PrimitiveType::Int}));
}

TEST_CASE("multiply - str str") {
    CHECK(multiply("3.0", "2.0") == 6.0);
    CHECK(multiply("foo", "2.0") ==
          std::unexpected(UnparsableString{
              .val = "foo", .targetType = PrimitiveType::Float}));
    CHECK(multiply("3.0", "bar") ==
          std::unexpected(UnparsableString{
              .val = "bar", .targetType = PrimitiveType::Float}));
}

TEST_CASE("multiply - invalid operands") {
    CHECK(multiply(VecValue{.type = PrimitiveType::Int, .elements = {1}}, 1) ==
          std::unexpected(InvalidOperands{.lhs = Type::vec(PrimitiveType::Int),
                                          .rhs = PrimitiveType::Int}));
}

TEST_CASE("divide - int int") {
    CHECK(divide(6, 3) == 2);
    CHECK(divide(7, 2) == 3);  // integer division truncates
    CHECK(divide(1, 0) == std::unexpected(DivisionByZero{}));
    CHECK(divide(INT_MIN, -1) == std::unexpected(ArithmeticOverflow{}));
}

TEST_CASE("divide - float float") {
    CHECK(divide(6.0, 2.0) == 3.0);
    CHECK(divide(1.0, 0.0) == std::unexpected(DivisionByZero{}));
}

TEST_CASE("divide - int float promotion") {
    CHECK(divide(6, 2.0) == 3.0);
    CHECK(divide(2.0, 4) == 0.5);
}

TEST_CASE("divide - bool promotion") {
    CHECK(divide(true, true) == 1);
    CHECK(divide(false, true) == 0);
    CHECK(divide(true, false) == std::unexpected(DivisionByZero{}));
    CHECK(divide(true, 2) == 0);
    CHECK(divide(4, true) == 4);
    CHECK(divide(true, 2.0) == 0.5);
    CHECK(divide(2.0, true) == 2.0);
}

TEST_CASE("divide - str int") {
    CHECK(divide("6", 3) == 2);
    CHECK(divide("6", 0) == std::unexpected(DivisionByZero{}));
    CHECK(divide(6, "3") == 2);
    CHECK(divide("foo", 3) ==
          std::unexpected(UnparsableString{.val = "foo",
                                           .targetType = PrimitiveType::Int}));
}

TEST_CASE("divide - str float") {
    CHECK(divide("6.0", 2.0) == 3.0);
    CHECK(divide(2.0, "4.0") == 0.5);
    CHECK(divide("foo", 2.0) ==
          std::unexpected(UnparsableString{
              .val = "foo", .targetType = PrimitiveType::Float}));
}

TEST_CASE("divide - str bool") {
    CHECK(divide("6", true) == 6);
    CHECK(divide("6", false) == std::unexpected(DivisionByZero{}));
    CHECK(divide("foo", true) ==
          std::unexpected(UnparsableString{.val = "foo",
                                           .targetType = PrimitiveType::Int}));
}

TEST_CASE("divide - str str") {
    CHECK(divide("6.0", "2.0") == 3.0);
    CHECK(divide("foo", "2.0") ==
          std::unexpected(UnparsableString{
              .val = "foo", .targetType = PrimitiveType::Float}));
    CHECK(divide("6.0", "bar") ==
          std::unexpected(UnparsableString{
              .val = "bar", .targetType = PrimitiveType::Float}));
}

TEST_CASE("divide - invalid operands") {
    CHECK(divide(VecValue{.type = PrimitiveType::Int, .elements = {1}}, 1) ==
          std::unexpected(InvalidOperands{.lhs = Type::vec(PrimitiveType::Int),
                                          .rhs = PrimitiveType::Int}));
}

TEST_CASE("modulo - int int") {
    CHECK(modulo(10, 3) == 1);
    CHECK(modulo(10, 2) == 0);
    CHECK(modulo(-10, 3) == -1);
    CHECK(modulo(10, 0) == std::unexpected(DivisionByZero{}));
    CHECK(modulo(INT_MIN, -1) == 0);
}

TEST_CASE("modulo - float float") {
    CHECK(modulo(10.0, 3.0) == std::fmod(10.0, 3.0));
    CHECK(modulo(10.0, 0.0) == std::unexpected(DivisionByZero{}));
}

TEST_CASE("modulo - int float promotion") {
    CHECK(modulo(10, 3.0) == std::fmod(10.0, 3.0));
    CHECK(modulo(10.0, 3) == std::fmod(10.0, 3.0));
}

TEST_CASE("modulo - bool promotion") {
    CHECK(modulo(true, true) == 0);
    CHECK(modulo(false, true) == 0);
    CHECK(modulo(true, false) == std::unexpected(DivisionByZero{}));
    CHECK(modulo(true, 3) == 1);
    CHECK(modulo(10, true) == 0);
    CHECK(modulo(true, 3.0) == std::fmod(1.0, 3.0));
    CHECK(modulo(10.0, true) == std::fmod(10.0, 1.0));
}

TEST_CASE("modulo - str int") {
    CHECK(modulo("10", 3) == 1);
    CHECK(modulo("10", 0) == std::unexpected(DivisionByZero{}));
    CHECK(modulo(10, "3") == 1);
    CHECK(modulo("foo", 3) ==
          std::unexpected(UnparsableString{.val = "foo",
                                           .targetType = PrimitiveType::Int}));
}

TEST_CASE("modulo - str float") {
    CHECK(modulo("10.0", 3.0) == std::fmod(10.0, 3.0));
    CHECK(modulo(10.0, "3.0") == std::fmod(10.0, 3.0));
    CHECK(modulo("10.0", 0.0) == std::unexpected(DivisionByZero{}));
    CHECK(modulo("foo", 3.0) ==
          std::unexpected(UnparsableString{
              .val = "foo", .targetType = PrimitiveType::Float}));
}

TEST_CASE("modulo - str bool") {
    CHECK(modulo("10", true) == 0);
    CHECK(modulo("10", false) == std::unexpected(DivisionByZero{}));
    CHECK(modulo("foo", true) ==
          std::unexpected(UnparsableString{.val = "foo",
                                           .targetType = PrimitiveType::Int}));
}

TEST_CASE("modulo - str str") {
    CHECK(modulo("10.0", "3.0") == std::fmod(10.0, 3.0));
    CHECK(modulo("10.0", "0.0") == std::unexpected(DivisionByZero{}));
    CHECK(modulo("foo", "3.0") ==
          std::unexpected(UnparsableString{
              .val = "foo", .targetType = PrimitiveType::Float}));
    CHECK(modulo("10.0", "bar") ==
          std::unexpected(UnparsableString{
              .val = "bar", .targetType = PrimitiveType::Float}));
}

TEST_CASE("modulo - invalid operands") {
    CHECK(modulo(VecValue{.type = PrimitiveType::Int, .elements = {1}}, 1) ==
          std::unexpected(InvalidOperands{.lhs = Type::vec(PrimitiveType::Int),
                                          .rhs = PrimitiveType::Int}));
}

TEST_CASE("length - string") {
    CHECK(length("") == 0);
    CHECK(length("12") == 2);
}

TEST_CASE("length - vec") {
    CHECK(length(VecValue{.type = PrimitiveType::Int, .elements = {}}) == 0);
    CHECK(length(VecValue{.type = PrimitiveType::Int, .elements = {1, 2}}) ==
          2);
}

TEST_CASE("length - invalid operand") {
    CHECK(length(1) ==
          std::unexpected(InvalidOperand{.type = PrimitiveType::Int}));
    CHECK(length(1.4) ==
          std::unexpected(InvalidOperand{.type = PrimitiveType::Float}));
}

TEST_CASE("negation - int") {
    CHECK(negation(1) == Value{-1});
    CHECK(negation(-1) == Value{1});
    CHECK(negation(0) == Value{0});
    CHECK(negation(INT_MIN) == std::unexpected(ArithmeticOverflow{}));
}

TEST_CASE("negation - bool") {
    CHECK(negation(true) == Value{-1});
    CHECK(negation(false) == Value{0});
}

TEST_CASE("negation - float") {
    CHECK(negation(1.0) == Value{-1.0});
    CHECK(negation(-1.0) == Value{1.0});
    CHECK(negation(0.0) == Value{-0.0});
}

TEST_CASE("negation - string as int") {
    CHECK(negation(std::string{"5"}) == Value{-5});
    CHECK(negation(std::string{"-5"}) == Value{5});
    CHECK(negation(std::string{"foo"}) ==
          std::unexpected(UnparsableString{.val = "foo",
                                           .targetType = PrimitiveType::Int}));
}

TEST_CASE("negation - invalid operands") {
    CHECK(
        negation(VecValue{.type = PrimitiveType::Int, .elements = {}}) ==
        std::unexpected(InvalidOperand{.type = Type::vec(PrimitiveType::Int)}));
}

TEST_CASE("logical not - bool") {
    CHECK(logicalNot(true) == Value{false});
    CHECK(logicalNot(false) == Value{true});
}

TEST_CASE("logical not - int") {
    CHECK(logicalNot(0) == Value{true});
    CHECK(logicalNot(1) == Value{false});
    CHECK(logicalNot(-1) == Value{false});
}

TEST_CASE("logical not - float") {
    CHECK(logicalNot(0.0) == Value{true});
    CHECK(logicalNot(1.0) == Value{false});
}

TEST_CASE("contains - int") {
    CHECK(contains(VecValue{.type = PrimitiveType::Int, .elements = {1, 2}},
                   1) == true);
    CHECK(contains(VecValue{.type = PrimitiveType::Int, .elements = {1, 2}},
                   3) == false);
}

TEST_CASE("contains - nested int") {
    CHECK(contains(VecValue{.type = Type::vec(PrimitiveType::Int),
                            .elements = {VecValue{.type = PrimitiveType::Int,
                                                  .elements = {1, 2}}}},
                   VecValue{.type = PrimitiveType::Int, .elements = {1, 2}}) ==
          true);

    CHECK(contains(VecValue{.type = Type::vec(PrimitiveType::Int),
                            .elements = {VecValue{.type = PrimitiveType::Int,
                                                  .elements = {1, 2}}}},
                   VecValue{.type = PrimitiveType::Int, .elements = {1, 3}}) ==
          false);
}

TEST_CASE("contains - invalid operands") {
    CHECK(contains("abc", 1) ==
          std::unexpected(InvalidOperands{.lhs = PrimitiveType::Str,
                                          .rhs = PrimitiveType::Int}));
}

TEST_CASE("logical not - string") {
    CHECK(logicalNot(std::string{""}) == Value{true});
    CHECK(logicalNot(std::string{"foo"}) == Value{false});
}

TEST_CASE("insersect - int") {
    CHECK(intersect(VecValue{.type = PrimitiveType::Int, .elements = {1, 2}},
                    VecValue{.type = PrimitiveType::Int, .elements = {1, 3}}) ==
          VecValue{.type = PrimitiveType::Int, .elements = {1}});

    CHECK(intersect(VecValue{.type = PrimitiveType::Int, .elements = {2}},
                    VecValue{.type = PrimitiveType::Int, .elements = {1, 3}}) ==
          VecValue{.type = PrimitiveType::Int, .elements = {}});
}

TEST_CASE("insersect - invalid operands") {
    CHECK(intersect(VecValue{.type = PrimitiveType::Int, .elements = {2}},
                    Value{1}) ==
          std::unexpected(InvalidOperands{.lhs = Type::vec(PrimitiveType::Int),
                                          .rhs = PrimitiveType::Int}));
}

TEST_CASE("toFloat") {
    CHECK(toFloat(1) == 1.0);
    CHECK(toFloat(1.5) == 1.5);
    CHECK(toFloat("1.5") == 1.5);
    CHECK(toFloat("1e355") == std::unexpected(ArithmeticOverflow{}));
    CHECK(toFloat("1abc") ==
          std::unexpected(UnparsableString{
              .val = "1abc", .targetType = PrimitiveType::Float}));
    CHECK(toFloat(true) == 1.0);
    CHECK(
        toFloat(VecValue{.type = PrimitiveType::Int, .elements = {1, 2}}) ==
        std::unexpected(InvalidConversion{.from = Type::vec(PrimitiveType::Int),
                                          .to = Type(PrimitiveType::Float)}));
}

TEST_CASE("toInt") {
    CHECK(toInt(1) == 1);
    CHECK(toInt(1.9) == 1);  // truncates
    CHECK(toInt("42") == 42);
    CHECK(toInt("9999999999999999999") ==
          std::unexpected(ArithmeticOverflow{}));
    CHECK(toInt("abc") == std::unexpected(UnparsableString{
                              .val = "abc", .targetType = PrimitiveType::Int}));
    CHECK(toInt(static_cast<double>(INT_MAX) * 2.0) ==
          std::unexpected(ArithmeticOverflow{}));
    CHECK(toInt(true) == 1);

    CHECK(
        toInt(VecValue{.type = PrimitiveType::Int, .elements = {1, 2}}) ==
        std::unexpected(InvalidConversion{.from = Type::vec(PrimitiveType::Int),
                                          .to = Type(PrimitiveType::Int)}));
}

TEST_CASE("toBool") {
    CHECK(toBool(true) == true);
    CHECK(toBool(false) == false);
    CHECK(toBool(1) == true);
    CHECK(toBool(0) == false);
    CHECK(toBool(-1) == true);
    CHECK(toBool(1.0) == true);
    CHECK(toBool(0.0) == false);
    CHECK(toBool("hello") == true);
    CHECK(toBool("") == false);
    CHECK(toBool(VecValue{.type = PrimitiveType::Int, .elements = {1}}) ==
          true);
    CHECK(toBool(VecValue{.type = PrimitiveType::Int, .elements = {}}) ==
          false);
}

TEST_CASE("toString") {
    CHECK(toString(true) == "true");
    CHECK(toString(false) == "false");
    CHECK(toString(1) == "1");
    CHECK(toString(-42) == "-42");
    CHECK(toString(1.0) == "1");
    CHECK(toString(1.5) == "1.5");
    CHECK(toString(1.23456) == "1.235");
    CHECK(toString("hello") == "hello");
    CHECK(toString("") == "");
    CHECK(toString(VecValue{.type = PrimitiveType::Int,
                            .elements = {1, 2, 3}}) == "[1, 2, 3]");
    CHECK(toString(VecValue{.type = PrimitiveType::Int, .elements = {}}) ==
          "[]");
    CHECK(toString(VecValue{
              .type = Type::vec(PrimitiveType::Int),
              .elements = {
                  VecValue{.type = PrimitiveType::Int, .elements = {1, 2}},
                  Value{VecValue{.type = PrimitiveType::Int,
                                 .elements = {3, 4}}}}}) == "[[1, 2], [3, 4]]");
}
