#pragma once
#include <algorithm>
#include <cstdint>
#include <cassert>
#include <vector>

#include "Position.h"

enum class Severity : std::int8_t {
    Warning,
    Error,
    Fatal,
};

template <typename Kind>
struct Diagnostic {
    Kind kind;
    Position pos;
    Severity severity;

    bool operator==(Diagnostic const&) const = default;
};

template <typename Kind>
class Diagnostics {
public:
    void push(Diagnostic<Kind> const& diagnostic) {
        diagnostics_.push_back(diagnostic);
    };

    auto const& all() const noexcept { return diagnostics_; };

    Diagnostic<Kind> last() const noexcept {
        assert(!diagnostics_.empty());
        return diagnostics_.back();
    }

    [[nodiscard]] bool empty() const noexcept { return diagnostics_.empty(); }

    [[nodiscard]] bool hasError() const noexcept {
        return std::ranges::any_of(
            diagnostics_, [](Diagnostic<Kind> const& diag) {
                return diag.severity == Severity::Error ||
                       diag.severity == Severity::Fatal;
            });
    }

private:
    std::vector<Diagnostic<Kind>> diagnostics_;
};
