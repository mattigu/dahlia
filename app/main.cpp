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

int main(int argc, char** argv) {
    CLI::App app;

    std::string file_path;
    std::string program_string;
    bool print_result{false};

    auto* const input_group = app.add_option_group("input", "Input source");
    auto* const program_opt = input_group->add_option(
        "-p,--program", program_string, "The program to execute");
    auto* const file_opt =
        input_group->add_option("-f,--file", file_path, "The file to execute")
            ->transform(CLI::ExistingFile);
    input_group->require_option(1);

    auto* const print_opt = app.add_flag(
        "-r,--result", print_result, "Print the result of the main function");

    CLI11_PARSE(app, argc, argv);

    file_path = std::filesystem::path(file_path).lexically_normal().string();

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
    if (print_result) {
        std::println("{}", *result);
    }
    return 0;
}
