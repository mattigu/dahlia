#pragma once

#include <variant>

#include "Value.h"
#include "dahlia_lib/Position.h"

struct ReturnSignal {
    Value value;
    bool operator==(ReturnSignal const& other) const noexcept = default;
};

// The position could be removed by adding a loop depth counter in the
// interpreter. Not sure which is better, but this is simpler for now.

struct BreakSignal {
    Position pos;
    bool operator==(BreakSignal const& other) const noexcept = default;
};
struct ContinueSignal {
    Position pos;
    bool operator==(ContinueSignal const& other) const noexcept = default;
};

using Signal =
    std::variant<std::monostate, ReturnSignal, BreakSignal, ContinueSignal>;
