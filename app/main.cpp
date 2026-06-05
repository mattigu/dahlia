#include <filesystem>
#include <fstream>
#include <iostream>
#include <print>
#include <sstream>

#include "CLI/CLI.hpp"
#include "dahlia_lib/Builtins.h"
#include "dahlia_lib/DiagnosticPrinter.h"
#include "dahlia_lib/Interpreter.h"
#include "dahlia_lib/Lexer.h"
#include "dahlia_lib/Parser.h"
#include "dahlia_lib/Value.h"

int main(int argc, char** argv) {
    CLI::App app;

    std::string file_path;
    std::string program_string;

    auto* program_opt =
        app.add_option("program", program_string, "The program to execute");
    auto* file_opt =
        app.add_option("-f,--file", file_path, "The file to execute")
            ->transform(CLI::ExistingFile);

    app.require_option(1);
    CLI11_PARSE(app, argc, argv);

    std::optional<std::ifstream> file_stream;
    std::optional<std::istringstream> string_stream;

    std::istream* input = nullptr;

    if (*file_opt) {
        file_stream.emplace(file_path);
        input = &*file_stream;
    } else if (*program_opt) {
        string_stream.emplace(program_string);
        input = &*string_stream;
    }

    auto parser = Parser(Lexer(*input, LexerOptions{}));

    auto const program = parser.parse();

    std::string source_name = *file_opt ? file_path : "input";
    auto printer = DiagnosticPrinter(std::move(source_name), std::cerr, *input);
    printer.printLexerAndParserDiags(parser.lexerDiagnostics().all(),
                                     parser.diagnostics().all());
    if (!program) {
        std::println("Parsing failed");
        return 1;
    }

    auto interpreter = Interpreter(makeDefaultBuiltins(std::cout),
                                   InterpreterOpts{.max_call_depth = 200});

    auto const result = interpreter.run(*program);
    if (!result) {
        printer.printErrorWithStackTrace(result.error(),
                                         interpreter.stackTrace());
        return 1;
    }
    std::println("{}", *result);
    return 0;
}
