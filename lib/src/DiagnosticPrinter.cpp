#include "dahlia_lib/DiagnosticPrinter.h"

#include <format>
#include <print>

#include "dahlia_lib/DiagnosticMessages.h"
#include "dahlia_lib/Position.h"
#include "dahlia_lib/RuntimeError.h"
#include "dahlia_lib/Stack.h"

#include <utf8.h>

DiagnosticPrinter::DiagnosticPrinter(std::string source, std::ostream& output,
                                     std::istream& input)
    : source_name_(std::move(source)), output_(output), input_(input) {
    input_.clear();
}

void DiagnosticPrinter::printErrorWithStackTrace(RuntimeError const& err,
                                                 StackTrace const& stacktrace) {
    printStackTrace(stacktrace);
    printError(err);
}

void DiagnosticPrinter::printStackTrace(StackTrace const& stacktrace) const {
    for (auto const& info : stacktrace) {
        println(output_, "{} in {}", formatLocation(info.pos), info.name);
    }
}

void DiagnosticPrinter::printError(RuntimeError const& err) {
    auto msg = messageFor(err.kind);
    std::println(output_, "{}", formatLocation(err.pos));
    auto const error_line = getLine(err.pos);
    std::println(output_, "{}", error_line);
    std::println(output_, "{}", pointToErrorLine(error_line, err.pos));
    std::println(output_, "Error: {}", std::move(msg));
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
