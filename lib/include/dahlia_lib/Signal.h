#pragma once

#include <variant>

#include "Value.h"
#include "dahlia_lib/Position.h"

struct ReturnSignal {
    Value value;
};

struct BreakSignal {};
struct ContinueSignal {};

struct Signal {
    std::variant<std::monostate, ReturnSignal, BreakSignal, ContinueSignal>
        kind;

    Position pos;
};
