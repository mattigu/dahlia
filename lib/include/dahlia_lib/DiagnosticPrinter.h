#pragma once

#include <istream>
#include <ostream>

#include "dahlia_lib/Interpreter.h"
#include "dahlia_lib/Position.h"
#include "dahlia_lib/RuntimeError.h"
#include "dahlia_lib/Stack.h"

class DiagnosticPrinter {
public:
    DiagnosticPrinter(std::string source, std::ostream& output,
                      std::istream& input);

    void printErrorWithStackTrace(RuntimeError const& err,
                                  StackTrace const& stacktrace);

    void printStackTrace(StackTrace const& stacktrace) const;

    void printError(RuntimeError const& err);

private:
    [[nodiscard]] std::string formatLocation(Position pos) const;
    static std::string formatPosition(Position pos);

    static std::string pointToErrorLine(std::string const& line,
                                                  Position pos);

    std::string getLine(Position pos);

    std::string source_name_;

    std::istream& input_;   // NOLINT
    std::ostream& output_;  // NOLINT
};
