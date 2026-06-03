#include "dahlia_lib/Builtins.h"

BuiltinFunction makePrintln(std::ostream& out) {
    return makeBuiltin({{.type = PrimitiveType::Str, .mut = false}},
                       std::function{[&out](std::string str) -> EvalResult {
                           std::println(out, "{}", str);
                           return Value{};
                       }});
}

BuiltinMap makeDefaultBuiltins(std::ostream& out) {
    BuiltinMap builtins;
    builtins["println"] = makePrintln(out);
    return builtins;
}
