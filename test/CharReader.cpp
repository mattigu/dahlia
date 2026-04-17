#include "src/CharReader.h"

#include <sstream>

#include "doctest.h"
#include "src/Position.h"

TEST_CASE("CharReader first token is STX") {
    std::istringstream input("123");
    CharReader reader(input);
    CHECK(reader.current() == STX);
}

TEST_CASE("CharReader last token is ETX") {
    std::istringstream input("1");
    CharReader reader(input);

    CHECK(reader.next() == '1');
    CHECK(reader.next() == ETX);
    CHECK(reader.next() == ETX);
}

TEST_CASE("CharReader reads correctly from a stream") {
    std::istringstream input("123");
    CharReader reader(input);

    CHECK(reader.next() == '1');
    CHECK(reader.next() == '2');
    CHECK(reader.next() == '3');
    CHECK(reader.next() == ETX);
}

TEST_CASE("CharReader initial position") {
    std::istringstream input("12");
    CharReader reader(input);

    CHECK(reader.position() == Position{.line = 1, .column = 0, .offset = -1});
}

TEST_CASE("CharReader position") {
    std::istringstream input("12");
    CharReader reader(input);

    CHECK(reader.position() == Position{.line = 1, .column = 0, .offset = -1});
    reader.next();
    CHECK(reader.position() == Position{.line = 1, .column = 1, .offset = 0});
    reader.next();
    CHECK(reader.position() == Position{.line = 1, .column = 2, .offset = 1});
}

TEST_CASE("CharReader position with new lines") {
    std::istringstream input("1\n2\n\n3");
    CharReader reader(input);

    CHECK(reader.position() == Position{.line = 1, .column = 0, .offset = -1});
    CHECK(reader.current() == STX);
    reader.next();

    CHECK(reader.position() == Position{.line = 1, .column = 1, .offset = 0});
    CHECK(reader.current() == '1');
    reader.next();

    CHECK(reader.position() == Position{.line = 1, .column = 2, .offset = 1});
    CHECK(reader.current() == '\n');
    reader.next();

    CHECK(reader.position() == Position{.line = 2, .column = 1, .offset = 2});
    CHECK(reader.current() == '2');
    reader.next();

    CHECK(reader.position() == Position{.line = 2, .column = 2, .offset = 3});
    CHECK(reader.current() == '\n');
    reader.next();

    CHECK(reader.position() == Position{.line = 3, .column = 1, .offset = 4});
    CHECK(reader.current() == '\n');
    reader.next();

    CHECK(reader.position() == Position{.line = 4, .column = 1, .offset = 5});
    CHECK(reader.current() == '3');
    reader.next();

    CHECK(reader.position() == Position{.line = 4, .column = 2, .offset = 6});
    CHECK(reader.current() == ETX);
}

TEST_CASE("CharReader ETX position") {
    std::istringstream input("1");
    CharReader reader(input);

    reader.next();
    reader.next();

    auto const etx_pos = Position{.line = 1, .column = 2, .offset = 1};

    reader.next();
    reader.next();
    reader.next();

    CHECK(etx_pos == reader.position());
}
