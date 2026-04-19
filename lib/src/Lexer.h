#pragma once

#include <functional>
#include <iostream>
#include <istream>
#include <optional>

#include "CharReader.h"
#include "Token.h"

class Lexer {
public:
    Lexer(std::istream& src, std::ostream& diagnostics = std::cerr) noexcept;

    Token next();
    Token current() const noexcept;

private:
    CharReader src_;

    Token current_;

    std::optional<Token> tryBuildToken();
    std::optional<Token> tryBuildSingleCharToken();
    std::optional<Token> tryBuildComment();

    std::optional<Token> tryBuildString();
    std::optional<char> tryBuildEscapeSequence();
    std::optional<char> tryBuildHexEscape();

    void skipWhile(std::function<bool(char)> const& predicate);
    std::string buildTextWhile(std::function<bool(char)> const& predicate);
};
