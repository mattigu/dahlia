#pragma once

#include <functional>
#include <iostream>
#include <unordered_map>

#include "Value.h"

struct BuiltinParam {
    Type type;
    bool mut = false;
};

struct BuiltinFunction {
    std::vector<BuiltinParam> params;
    std::function<EvalResult(std::vector<Value>)> fn;
};

template <typename... Args>
BuiltinFunction makeBuiltin(std::vector<BuiltinParam> params,
                            std::function<EvalResult(Args...)> fn) {
    return {
        .params = std::move(params),
        .fn = [fn](std::vector<Value> args) -> EvalResult {
            auto tuple_args = [&]<std::size_t... I>(std::index_sequence<I...>) {
                return std::make_tuple(std::get<Args>(std::move(args[I]))...);
            }(std::make_index_sequence<sizeof...(Args)>{});
            return std::apply(fn, std::move(tuple_args));
        }};
}
using BuiltinMap = std::unordered_map<std::string, BuiltinFunction>;
BuiltinMap makeDefaultBuiltins(std::ostream& out = std::cout);

BuiltinFunction makePrintln(std::ostream& out = std::cout);
