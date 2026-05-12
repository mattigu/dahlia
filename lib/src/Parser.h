#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Ast.h"
#include "Diagnostics.hpp"
#include "Lexer.h"
#include "ParserDiagnostic.h"
#include "Position.h"
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

        auto block_stmt = tryParseBlock();
        if (block_stmt) {
            return StatementNode(start_pos, std::move(**block_stmt));
        }

        auto unterminated_stmt =
            tryParseIfStmt().or_else([this]() { return tryParseWhileLoop(); });
        if (unterminated_stmt) {
            return std::move(*unterminated_stmt);
        }

        auto terminated_stmt =
            tryParseLetBinding()
                .or_else([this]() { return tryParseCallOrAssign(); })
                .or_else([this]() { return tryParseContinueStmt(); })
                .or_else([this]() { return tryParseBreakStmt(); })
                .or_else([this]() { return tryParseReturnStmt(); });
        if (terminated_stmt) {
            expect(TokenKind::Semicolon);
            return terminated_stmt;
        }
        return std::nullopt;
    }

    // If_expression skipped for now
    // let_binding = "let", [ "mut" ], IDENTIFIER, [ ":", type ], "=", (
    // if_expression | expression );
    std::optional<StatementNode> tryParseLetBinding() {
        auto const start_pos = current_.pos();
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
        return StatementNode(start_pos,
                             LetBinding{.identifier = std::move(*identifier),
                                        .mut = mut,
                                        .type = std::move(type),
                                        .value = std::move(*expr)});
    }

    // These 2 take the identifier and build the full FunctionCall or AssignStmt
    // call_or_assign = IDENTIFIER, ( call_suffix | assignment_suffix );
    std::optional<StatementNode> tryParseCallOrAssign() {
        auto const start_pos = current_.pos();

        auto const identifier = consumeIdentifier();
        if (!identifier) {
            return std::nullopt;
        }
        auto call_or_assign =
            tryParseCallSuffix(start_pos, *identifier).or_else([&]() {
                return tryParseAssignmentSuffix(start_pos, *identifier);
            });

        return call_or_assign;
    }

    // call_suffix = "(", function_arguments, ")";
    std::optional<StatementNode> tryParseCallSuffix(
        Position start_pos, std::string const& identifier) {
        if (!consume(TokenKind::ParenOpen)) {
            return std::nullopt;
        }
        auto args = tryParseFunctionArgs();

        expect(TokenKind::ParenClose);
        return StatementNode(start_pos, FunctionCall{.identifier = identifier,
                                                     .args = std::move(args)});
    }

    // if expression skipped for now
    // assignment_suffix = { "[", expression, "]" }, ( "=" |
    // compound_assignment_op ), ( if_expression | expression );
    std::optional<StatementNode> tryParseAssignmentSuffix(
        auto const start_pos, std::string const& identifier) {
        std::vector<ExprNode> indices;
        while (consume(TokenKind::BracketOpen)) {
            auto expr = tryParseExpression();
            if (!expr) {
                throwDiag(ExpectedExpression{}, current_.pos());
            }
            expect(TokenKind::BracketClose);
            indices.push_back(std::move(*expr));
        }
        auto lvalue =
            LValue{.identifier = identifier, .indices = std::move(indices)};

        using Constructor = std::function<StatementKind(LValue, ExprNode)>;
        auto static const ops =
            std::to_array<std::pair<TokenKind, Constructor>>({
                {TokenKind::Equal,
                 [](LValue lval, ExprNode expr) {
                     return AssignStmt{std::move(lval), std::move(expr)};
                 }},
                {TokenKind::PlusEqual,
                 [](LValue lval, ExprNode expr) {
                     return AddAssignStmt{std::move(lval), std::move(expr)};
                 }},
                {TokenKind::MinusEqual,
                 [](LValue lval, ExprNode expr) {
                     return SubAssignStmt{std::move(lval), std::move(expr)};
                 }},
                {TokenKind::AsteriskEqual,
                 [](LValue lval, ExprNode expr) {
                     return MulAssignStmt{std::move(lval), std::move(expr)};
                 }},
                {TokenKind::SlashEqual,
                 [](LValue lval, ExprNode expr) {
                     return DivAssignStmt{std::move(lval), std::move(expr)};
                 }},
                {TokenKind::PercentEqual,
                 [](LValue lval, ExprNode expr) {
                     return ModAssignStmt{std::move(lval), std::move(expr)};
                 }},
            });
        for (auto const& [kind, constructor] : ops) {
            if (consume(kind)) {
                auto expr = tryParseExpression();
                if (!expr) {
                    throwDiag(ExpectedExpression{}, current_.pos());
                }
                return StatementNode(start_pos, constructor(std::move(lvalue),
                                                            std::move(*expr)));
            }
        }
        return std::nullopt;
    }
    std::optional<StatementNode> tryParseBreakStmt() {
        auto const start_pos = current_.pos();
        if (!consume(TokenKind::Break)) {
            return std::nullopt;
        }
        return StatementNode(start_pos, BreakStmt{});
    }

    std::optional<StatementNode> tryParseContinueStmt() {
        auto const start_pos = current_.pos();
        if (!consume(TokenKind::Continue)) {
            return std::nullopt;
        }
        return StatementNode(start_pos, ContinueStmt{});
    }

    // return_statement = "return", [ expression ], ";";
    std::optional<StatementNode> tryParseReturnStmt() {
        auto const start_pos = current_.pos();
        if (!consume(TokenKind::Return)) {
            return std::nullopt;
        }
        auto expr = tryParseExpression();

        return StatementNode(start_pos, ReturnStmt{.value = std::move(expr)});
    }

    // if_statement = "if", expression, block, { "else", "if", expression, block
    // }, [ "else", block ];
    std::optional<StatementNode> tryParseIfStmt() {
        auto const start_pos = current_.pos();
        if (!consume(TokenKind::If)) {
            return std::nullopt;
        }
        auto if_cond = tryParseExpression();
        if (!if_cond) {
            throwDiag(ExpectedExpression{}, current_.pos());
        }
        auto if_block = tryParseBlock();
        if (!if_block) {
            throwDiag(ExpectedToken{.expected = TokenKind::BraceOpen,
                                    .got = current_.kind()},
                      current_.pos());
        }

        std::vector<std::pair<ExprNode, BlockNode>> else_ifs;
        std::optional<BlockNode> else_block = std::nullopt;

        while (consume(TokenKind::Else)) {
            if (consume(TokenKind::If)) {
                auto cond = tryParseExpression();
                if (!cond) {
                    throwDiag(ExpectedExpression{}, current_.pos());
                }
                auto block = tryParseBlock();
                if (!block) {
                    throwDiag(ExpectedToken{.expected = TokenKind::BraceOpen,
                                            .got = current_.kind()},
                              current_.pos());
                }
                else_ifs.push_back({std::move(*cond), std::move(*block)});

            } else {
                else_block = std::move(tryParseBlock());
                if (!else_block) {
                    throwDiag(ExpectedToken{.expected = TokenKind::BraceOpen,
                                            .got = current_.kind()},
                              current_.pos());
                }
                break;
            }
        }
        return StatementNode(start_pos,
                             IfStmt{.if_cond = std::move(*if_cond),
                                    .if_block = std::move(*if_block),
                                    .else_if_branches = std::move(else_ifs),
                                    .else_block = std::move(else_block)});
    }

    // while_loop = "while", expression, block;
    std::optional<StatementNode> tryParseWhileLoop() {
        auto const start_pos = current_.pos();

        if (!consume(TokenKind::While)) {
            return std::nullopt;
        }

        auto condition = tryParseExpression();
        if (!condition) {
            throwDiag(ExpectedExpression{}, current_.pos());
        }
        auto block = tryParseBlock();
        if (!block) {
            throwDiag(ExpectedToken{.expected = TokenKind::BraceOpen,
                                    .got = current_.kind()},
                      current_.pos());
        }
        return StatementNode(start_pos, WhileLoop{
                                            .condition = std::move(*condition),
                                            .block = std::move(*block),
                                        });
    }

    // expression = logical_or_expr, { ( "?" | ":>" ), logical_or_expr };
    std::optional<ExprNode> tryParseExpression() {
        return tryParseBinaryExpr(&ParserTemplate::tryParseDisjunctionExpr,
                                  binOp<FilterExpr>(TokenKind::Question),
                                  binOp<MapExpr>(TokenKind::ColonGreater));
    }

    // logical_or_expr = logical_and_expr, { "or", logical_and_expr };
    std::optional<ExprNode> tryParseDisjunctionExpr() {
        return tryParseBinaryExpr(&ParserTemplate::tryParseConjunctionExpr,
                                  binOp<OrExpr>(TokenKind::Or));
    }

    // logical_and_expr = relation_expr, { "and", relation_expr };
    std::optional<ExprNode> tryParseConjunctionExpr() {
        return tryParseBinaryExpr(&ParserTemplate::tryParseRelationExpr,
                                  binOp<AndExpr>(TokenKind::And));
    }

    // relation_expr = in_expr, { relation_operator, in_expr };
    std::optional<ExprNode> tryParseRelationExpr() {
        return tryParseBinaryExpr(&ParserTemplate::tryParseInExpr,
                                  binOp<GeExpr>(TokenKind::GreaterEqual),
                                  binOp<GtExpr>(TokenKind::Greater),
                                  binOp<LeExpr>(TokenKind::LessEqual),
                                  binOp<LtExpr>(TokenKind::Less),
                                  binOp<EqExpr>(TokenKind::EqualEqual),
                                  binOp<NeqExpr>(TokenKind::ExclamationEqual));
    }

    // in_expr = intersection_expr, {"in", intersection_expr};
    std::optional<ExprNode> tryParseInExpr() {
        return tryParseBinaryExpr(&ParserTemplate::tryParseIntersectionExpr,
                                  binOp<InExpr>(TokenKind::In));
    }

    // intersection_expr = additive_expr, { "><", additive_expr };
    std::optional<ExprNode> tryParseIntersectionExpr() {
        return tryParseBinaryExpr(&ParserTemplate::tryParseAdditiveExpr,
                                  binOp<IntersectExpr>(TokenKind::GreaterLess));
    }

    // additive_expr = multiplicative_expr, { ( "+" | "-" ),
    // multiplicative_expr
    // };
    std::optional<ExprNode> tryParseAdditiveExpr() {
        return tryParseBinaryExpr(&ParserTemplate::tryParseMultiplicativeExpr,
                                  binOp<AddExpr>(TokenKind::Plus),
                                  binOp<SubExpr>(TokenKind::Minus));
    }

    // multiplicative_expr = unary_expr, { ( "*" | "/" ), unary_expr };
    std::optional<ExprNode> tryParseMultiplicativeExpr() {
        return tryParseBinaryExpr(&ParserTemplate::tryParseUnaryExpr,
                                  binOp<MulExpr>(TokenKind::Asterisk),
                                  binOp<DivExpr>(TokenKind::Slash));
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
                throwDiag(ExpectedExpression{}, current_.pos());
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
        expect(TokenKind::BracketClose);
        return ExprNode(start_pos, VecLiteral{.elements = std::move(elements)});
    };

    template <typename NextLevel, typename... Ops>
    std::optional<ExprNode> tryParseBinaryExpr(NextLevel next, Ops... ops) {
        auto left = (this->*next)();
        if (!left) {
            return std::nullopt;
        }

        while (true) {
            auto const pos = current_.pos();
            bool matched = false;
            auto try_op = [&](auto kind, auto constructor) {
                if (!matched && consume(kind)) {
                    auto right = (this->*next)();
                    if (!right) {
                        throwDiag(ExpectedExpression{}, current_.pos());
                    }
                    left = ExprNode(
                        pos, constructor(std::move(*left), std::move(*right)));
                    matched = true;
                }
            };
            (try_op(ops.first, ops.second), ...);
            if (!matched) {
                break;
            }
        }
        return left;
    }

    template <typename T>
    auto binOp(TokenKind kind) {
        return std::make_pair(kind, [](ExprNode lhs, ExprNode rhs) {
            return Expr{T(std::move(lhs), std::move(rhs))};
        });
    }
};
using Parser = ParserTemplate<Lexer>;
