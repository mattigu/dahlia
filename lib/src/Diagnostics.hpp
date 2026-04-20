#pragma once
#include <vector>

#include "Position.h"

template <typename Kind>
struct Diagnostic {
    Kind kind;
    Position pos;
};

template <typename Kind>
class Diagnostics {
public:
    void push(Diagnostic<Kind> const& diagnostic) {
        diagnostics_.push_back(diagnostic);
    };

    auto const& all() const noexcept { return diagnostics_; };

    auto const& last() const noexcept { return diagnostics_.back(); }

    [[nodiscard]] bool empty() const noexcept { return diagnostics_.empty(); }

private:
    std::vector<Diagnostic<Kind>> diagnostics_;
};
