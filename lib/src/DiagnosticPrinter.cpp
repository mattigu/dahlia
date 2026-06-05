#include "dahlia_lib/DiagnosticPrinter.h"

#include <utf8.h>

#include <algorithm>
#include <format>
#include <print>

#include "dahlia_lib/DiagnosticMessages.h"
#include "dahlia_lib/Position.h"
#include "dahlia_lib/RuntimeError.h"
#include "dahlia_lib/Stack.h"
#include "dahlia_lib/Value.h"
#include "magic_enum/magic_enum.hpp"

DiagnosticPrinter::DiagnosticPrinter(std::string source, std::ostream& output,
                                     std::istream& input)
    : source_name_(std::move(source)), output_(output), input_(input) {
    input_.clear();
}

void DiagnosticPrinter::printLexerAndParserDiags(
    std::vector<LexerDiagnostic> const& lexer_diags,
    std::vector<ParserDiagnostic> const& parser_diags) {
    using AnyDiag = std::variant<LexerDiagnostic, ParserDiagnostic>;
    std::vector<AnyDiag> all;
    all.reserve(lexer_diags.size() + parser_diags.size());

    std::ranges::merge(lexer_diags, parser_diags, std::back_inserter(all),
                       [](auto const& lhs, auto const& rhs) {
                           return lhs.pos.offset < rhs.pos.offset;
                       });

    for (auto const& diagnostic : all) {
        std::visit(Overloaded{[&](auto const& diag) {
                       auto error =
                           std::format("{}: {}", toString(diag.severity),
                                       messageFor(diag.kind));
                       std::println(output_, "{}: {}", formatLocation(diag.pos),
                                    std::move(error));

                       auto const diag_line = getLine(diag.pos);
                       std::println(output_, "  | {}", diag_line);
                       std::println(output_, "  | {}",
                                    pointToErrorLine(diag_line, diag.pos));
                   }},
                   diagnostic);
    }
}

void DiagnosticPrinter::printErrorWithStackTrace(RuntimeError const& err,
                                                 StackTrace const& stacktrace) {
    printStackTrace(stacktrace);
    printError(err);
}

void DiagnosticPrinter::printStackTrace(StackTrace const& stacktrace) const {
    std::println(output_, "Stacktrace:");
    for (auto const& info : stacktrace) {
        println(output_, "{} in {}", formatLocation(info.pos), info.name);
    }
}

void DiagnosticPrinter::printError(RuntimeError const& err) {
    auto error = std::format("Error: {}", messageFor(err.kind));
    std::println(output_, "{}: {}", formatLocation(err.pos), error);
    auto const error_line = getLine(err.pos);
    std::println(output_, "  | {}", error_line);
    std::println(output_, "  | {}", pointToErrorLine(error_line, err.pos));
}

std::string DiagnosticPrinter::formatLocation(Position pos) const {
    return std::format("{}:{}", source_name_, formatPosition(pos));
}

std::string DiagnosticPrinter::formatPosition(Position pos) {
    return std::format("{}:{}", pos.line, pos.column);
}

std::string DiagnosticPrinter::getLine(Position pos) {
    // Assumes stream is seekable
    assert(pos.line > 0);

    input_.seekg(0);
    std::string line;
    for (int current = 1; std::getline(input_, line); ++current) {
        if (current == pos.line) {
            return line;
        }
    }
    return "";
}

std::string DiagnosticPrinter::pointToErrorLine(std::string const& line,
                                                Position pos) {
    assert(pos.column > 0);
    auto const begin = line.begin();
    auto const end = line.begin() + (pos.column - 1);
    auto const width = utf8::distance(begin, end);
    return std::string(width, ' ') + "^";
}

[[nodiscard]] std::string DiagnosticPrinter::toString(Severity severity) {
    return std::string(magic_enum::enum_name(severity));
}
