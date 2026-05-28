#pragma once

#include <variant>

#include "Value.h"

struct ReturnSignal {
    Value value;
    bool operator==(ReturnSignal const& other) const noexcept = default;
};

struct BreakSignal {
    bool operator==(BreakSignal const& other) const noexcept = default;
};
struct ContinueSignal {
    bool operator==(ContinueSignal const& other) const noexcept = default;
};

using Signal =
    std::variant<std::monostate, ReturnSignal, BreakSignal, ContinueSignal>;
