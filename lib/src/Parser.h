#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

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
        } catch (Diagnostic<ParserDiagnosticKind> const&) {
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

    // statement = let_binding, ";" | assignment_statement, ";" |
    // return_statement, ";" | break_statement, ";" | continue_statement, ";" |
    // function_call, ";" | block | if_statement | while_loop | for_loop;
    std::optional<StatementNode> tryParseStatement() {
        auto const start_pos = current_.pos();

        auto let = tryParseLetBinding();
        if (let) {
            auto statement = StatementNode(start_pos, std::move(*let));
            expect(TokenKind::Semicolon);
            return statement;
        }
        return std::nullopt;
    }

    // If_expression skipped for now
    // let_binding = "let", [ "mut" ], IDENTIFIER, [ ":", type ], "=", (
    // if_expression | expression );
    std::optional<LetBinding> tryParseLetBinding() {
        if (!consume(TokenKind::Let)) {
            return std::nullopt;
        }
        bool const mut = consume(TokenKind::Mut);

        auto const identifier = consumeIdentifier();
        if (!identifier) {
            throwDiag({ExpectedToken{.expected = TokenKind::Identifier,
                                     .got = current_.kind()}},
                      current_.pos());
        }

        std::optional<TypeNode> type;
        if (consume(TokenKind::Colon)) {
            type = tryParseType();
            if (!type) {
                throwDiag(ExpectedType{}, current_.pos());
            }
        }

        expect(TokenKind::Equal);

        auto expr = tryParseExpression();
        if (!expr) {
            throwDiag(ExpectedExpression{}, current_.pos());
        }
        return LetBinding{.identifier = std::move(*identifier),
                          .mut = mut,
                          .type = std::move(type),
                          .value = std::move(*expr)};
    }

    std::optional<ExprNode> tryParseExpression() {
        return tryParseMultiplicativeExpr();
    }

    // multiplicative_expr= unary_expr, { ( "*" | "/" ), unary_expr };
    std::optional<ExprNode> tryParseMultiplicativeExpr() {
        auto left = tryParseUnaryExpr();
        if (!left) {
            return std::nullopt;
        }

        while (true) {
            auto const pos = current_.pos();
            if (consume(TokenKind::Asterisk)) {
                auto right = tryParseUnaryExpr();
                if (!right) {
                    throwDiag(ExpectedExpression{}, current_.pos());
                }
                left =
                    ExprNode(pos, MulExpr(std::move(*left), std::move(*right)));
            } else if (consume(TokenKind::Slash)) {
                auto right = tryParseUnaryExpr();
                if (!right) {
                    throwDiag(ExpectedExpression{}, current_.pos());
                }
                left =
                    ExprNode(pos, DivExpr(std::move(*left), std::move(*right)));
            } else {
                break;
            }
        }
        return left;
    }

    // unary_expr = [ "-" | "!" | "@" ], index_expr;
    std::optional<ExprNode> tryParseUnaryExpr() {
        auto const start_pos = current_.pos();

        if (consume(TokenKind::Minus)) {
            auto operand = tryParseIndexExpr();
            if (!operand) {
                throwDiag(ExpectedExpression{}, current_.pos());
            }
            return ExprNode(start_pos, NegExpr(std::move(*operand)));
        }
        if (consume(TokenKind::Exclamation)) {
            auto operand = tryParseIndexExpr();
            if (!operand) {
                throwDiag(ExpectedExpression{}, current_.pos());
            }
            return ExprNode(start_pos, NotExpr(std::move(*operand)));
        }
        if (consume(TokenKind::At)) {
            auto operand = tryParseIndexExpr();
            if (!operand) {
                throwDiag(ExpectedExpression{}, current_.pos());
            }
            return ExprNode(start_pos, LengthExpr(std::move(*operand)));
        }

        return tryParseIndexExpr();
    }

    // index_expr = term, { "[", expression, "]" };
    std::optional<ExprNode> tryParseIndexExpr() {
        auto const start_pos = current_.pos();
        auto term = tryParseTerm();
        if (!term) {
            return std::nullopt;
        }

        while (consume(TokenKind::BracketOpen)) {
            auto index = tryParseExpression();
            if (!index) {
                throwDiag(ExpectedExpression{}, current_.pos());
            }
            expect(TokenKind::BracketClose);
            auto const pos = term->pos();
            term = ExprNode(
                pos,
                Expr{IndexExpr{
                    .object = std::make_unique<ExprNode>(std::move(*term)),
                    .index = std::make_unique<ExprNode>(std::move(*index))}});
        }
        return term;
    }

    // term = literal | identifier_or_call | "(", expression, ")";
    std::optional<ExprNode> tryParseTerm() {
        return tryParseLiteral()
            .or_else([this]() { return tryParseIdentifierOrCall(); })
            .or_else([this]() { return tryParseGroupedExpression(); });
    }

    // identifier_or_call = IDENTIFIER, [ "(", function_arguments, ")" ];
    std::optional<ExprNode> tryParseIdentifierOrCall() {
        auto const start_pos = current_.pos();
        auto const identifier = consumeIdentifier();
        if (!identifier) {
            return std::nullopt;
        }
        if (!consume(TokenKind::ParenOpen)) {
            return ExprNode(start_pos, Identifier{std::move(*identifier)});
        }
        auto args = tryParseFunctionArgs();

        expect(TokenKind::ParenClose);
        return ExprNode(start_pos,
                        FunctionCall{.identifier = std::move(*identifier),
                                     .args = std::move(args)});
    }

    // function_arguments = [ expression, { ",", expression } ];
    std::vector<ExprNode> tryParseFunctionArgs() {
        auto const start_pos = current_.pos();
        auto first_expr = tryParseExpression();

        if (!first_expr) {
            return {};
        }

        std::vector<ExprNode> args;
        args.emplace_back(std::move(*first_expr));

        while (consume(TokenKind::Comma)) {
            auto expr = tryParseExpression();
            if (!expr) {
                break;
            }
            args.push_back(std::move(*expr));
        }
        return args;
    }
    // Helper for "(", expresion, ")"
    std::optional<ExprNode> tryParseGroupedExpression() {
        if (!consume(TokenKind::ParenOpen)) {
            return std::nullopt;
        }

        auto expr = tryParseExpression();
        if (!expr) {
            throwDiag(ExpectedExpression{}, current_.pos());
        }

        expect(TokenKind::ParenClose);

        return std::move(expr);
    }

    // literal = INTEGER | FLOAT | STRING | BOOL | vec_literal;
    std::optional<ExprNode> tryParseLiteral() {
        auto const start_pos = current_.pos();
        if (auto const int_val = consumeInteger()) {
            return ExprNode(start_pos, IntLiteral{.value = *int_val});
        }
        if (auto const float_val = consumeFloat()) {
            return ExprNode(start_pos, FloatLiteral{.value = *float_val});
        }
        if (auto const str_val = consumeString()) {
            return ExprNode(start_pos, StringLiteral{.value = *str_val});
        }
        if (consume(TokenKind::True)) {
            return ExprNode(start_pos, BoolLiteral{.value = true});
        }
        if (consume(TokenKind::False)) {
            return ExprNode(start_pos, BoolLiteral{.value = false});
        }
        if (!consume(TokenKind::BracketOpen)) {
            return std::nullopt;
        }

        auto first_elem = tryParseExpression();
        std::vector<ExprNode> elements;

        if (first_elem) {
            elements.push_back(std::move(*first_elem));
        };

        while (consume(TokenKind::Comma) && !elements.empty()) {
            if (auto elem = tryParseExpression()) {
                elements.push_back(std::move(*elem));
            } else {
                break;
            }
        }
        consume(TokenKind::Comma);
        expect(TokenKind::BracketClose);
        return ExprNode(start_pos, VecLiteral{.elements = std::move(elements)});
    };
};
using Parser = ParserTemplate<Lexer>;
