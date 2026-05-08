#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>

#include "Ast.h"
#include "Diagnostics.hpp"
#include "Lexer.h"
#include "ParserDiagnostic.h"
#include "Token.h"

template <typename L>
class ParserTemplate {
public:
    explicit ParserTemplate(L lexer)
        : lexer_{std::move(lexer)}, current_{lexer_.current()} {}

    std::optional<ProgramNode> parse() {
        if (current_.is(TokenKind::STX)) {
            nextToken();
        }

        auto const start_pos = current_.pos();
        std::unordered_map<std::string, FunctionNode> functions;

        try {
            while (auto fun = tryParseFunction()) {
                auto const new_pos = fun->pos();
                auto const [iter, inserted] =
                    functions.try_emplace((*fun)->identifier, std::move(*fun));
                if (!inserted) {
                    pushDiag(
                        FunctionRedefined{.identifier = iter->first,
                                          .original_pos = iter->second.pos()},
                        new_pos);
                }
            }
        } catch (ParserDiagnosticKind const&) {
            return std::nullopt;
        }

        if (!consume(TokenKind::ETX)) {
            pushDiag(ExpectedToken{.expected = TokenKind::ETX,
                                   .got = current_.kind()},
                     current_.pos());
            return std::nullopt;
        }

        if (lexer_.diagnostics().hasError() || diagnostics_.hasError()) {
            return std::nullopt;
        }

        return ProgramNode(start_pos,
                           Program{.functions = std::move(functions)});
    }

    [[nodiscard]] Diagnostics<ParserDiagnosticKind> const& diagnostics()
        const noexcept {
        return diagnostics_;
    }

private:
    L lexer_;
    Token current_;
    Diagnostics<ParserDiagnosticKind> diagnostics_;

    Token nextToken() {
        current_ = lexer_.next();
        while (current_.kind() == TokenKind::Comment) {
            current_ = lexer_.next();
        }
        return current_;
    }

    void pushDiag(ParserDiagnosticKind const& kind, Position const& pos,
                  Severity severity = Severity::Error) {
        diagnostics_.push({.kind = kind, .pos = pos, .severity = severity});
    }

    void throwDiag(ParserDiagnosticKind const& kind, Position const& pos,
                   Severity severity = Severity::Fatal) {
        Diagnostic<ParserDiagnosticKind> diag{
            .kind = kind, .pos = current_.pos(), .severity = severity};
        diagnostics_.push(diag);
        throw diag;
    }

    void expect(TokenKind kind) {
        if (!consume(kind)) {
            throwDiag(ExpectedToken{.expected = kind, .got = current_.kind()},
                      current_.pos());
        }
    }

    bool consume(TokenKind kind) {
        if (current_.is(kind)) {
            nextToken();
            return true;
        }
        return false;
    }

    template <TokenValueType T>
    std::optional<T> consumeValue(TokenKind kind) {
        assert(Token::matches(kind, TokenValue{T{}}));
        if (!current_.is(kind)) {
            return std::nullopt;
        }
        auto value = std::get<T>(current_.value());
        nextToken();
        return value;
    }

    std::optional<std::string> consumeIdentifier() {
        return consumeValue<std::string>(TokenKind::Identifier);
    }

    std::optional<std::int64_t> consumeInteger() {
        return consumeValue<std::int64_t>(TokenKind::IntLiteral);
    }

    std::optional<double> consumeFloat() {
        return consumeValue<double>(TokenKind::FloatLiteral);
    }

    std::optional<std::string> consumeString() {
        return consumeValue<std::string>(TokenKind::StrLiteral);
    }

    // function_definition  = "fn", IDENTIFIER, "(", function_params, ")", [
    // "->", type ], block;
    std::optional<FunctionNode> tryParseFunction() {
        auto const start_pos = current_.pos();
        if (!consume(TokenKind::Fn)) {
            return std::nullopt;
        }

        auto identifier = consumeIdentifier();
        if (!identifier) {
            throwDiag(ExpectedToken{.expected = TokenKind::Identifier,
                                    .got = current_.kind()},
                      current_.pos());
        }

        bool const paren_open = consume(TokenKind::ParenOpen);
        auto params = tryParseFunctionParams();
        expect(TokenKind::ParenClose);

        std::optional<TypeNode> return_type;
        if (consume(TokenKind::MinusGreater)) {
            return_type = tryParseType();
            if (!return_type) {
                throwDiag(ExpectedType{}, current_.pos());
            }
        }

        auto block = tryParseBlock();

        if (!block) {
            throwDiag(ExpectedToken{.expected = TokenKind::BraceOpen,
                                    .got = current_.kind()},
                      current_.pos());
        }

        return FunctionNode(start_pos,
                            Function{.identifier = std::move(*identifier),
                                     .params = std::move(params),
                                     .return_type = std::move(return_type),
                                     .block = std::move(*block)});
    }

    // function_params = [ function_param, { ",", function_param } ];
    std::vector<ParamNode> tryParseFunctionParams() {
        auto first_param = tryParseFunctionParam();
        if (!first_param) {
            return {};
        }

        std::vector<ParamNode> params;
        params.push_back(std::move(*first_param));

        while (consume(TokenKind::Comma)) {
            auto param = tryParseFunctionParam();
            if (!param) {
                break;
            }
            params.push_back(std::move(*param));
        }
        return params;
    }

    // function_param = [ "mut" ], IDENTIFIER, ":", type;
    std::optional<ParamNode> tryParseFunctionParam() {
        auto const start_pos = current_.pos();
        if (!current_.isAnyOf(TokenKind::Mut, TokenKind::Identifier)) {
            return std::nullopt;
        }

        bool const mut = consume(TokenKind::Mut);
        auto identifier = consumeIdentifier();
        if (!identifier) {
            throwDiag(ExpectedToken{.expected = TokenKind::Identifier,
                                    .got = current_.kind()},
                      current_.pos());
        }

        expect(TokenKind::Colon);
        auto type = tryParseType();

        return ParamNode(start_pos, Param{.type = std::move(*type),
                                          .identifier = *identifier,
                                          .mut = mut});
    }

    // type = "int" | "float" | "bool" | "str" | "[", type, "]";
    std::optional<TypeNode> tryParseType() {
        auto const start_pos = current_.pos();
        if (consume(TokenKind::BracketOpen)) {
            auto inner = tryParseType();
            if (!inner) {
                throwDiag(ExpectedType{}, current_.pos());
            }
            expect(TokenKind::BracketClose);
            return TypeNode(start_pos, Type::vec(std::move(**inner)));
        }

        if (consume(TokenKind::Int)) {
            return TypeNode(start_pos, PrimitiveType::Int);
        }
        if (consume(TokenKind::Float)) {
            return TypeNode(start_pos, PrimitiveType::Float);
        }
        if (consume(TokenKind::Bool)) {
            return TypeNode(start_pos, PrimitiveType::Bool);
        }
        if (consume(TokenKind::Str)) {
            return TypeNode(start_pos, PrimitiveType::Str);
        }

        return std::nullopt;
    }

    // block = "{", { statement }, "}";
    std::optional<BlockNode> tryParseBlock() {
        auto const start_pos = current_.pos();

        if (!consume(TokenKind::BraceOpen)) {
            return std::nullopt;
        }

        std::vector<StatementNode> statements;
        while (auto statement = tryParseStatement()) {
            statements.push_back(std::move(*statement));
        }

        expect(TokenKind::BraceClose);
        return BlockNode(start_pos, Block{.statements = std::move(statements)});
    }

    // statement = let_binding | assignment_statement | if_statement |
    // while_statement | for_statement | return_statement | break_statement |
    // continue_statement | block | function_call, ";";
    std::optional<StatementNode> tryParseStatement() { return std::nullopt; }

    // std::optional<LetBinding> tryParseLetBinding() {}
};

using Parser = ParserTemplate<Lexer>;
