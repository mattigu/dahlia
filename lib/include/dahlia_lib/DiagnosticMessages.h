#pragma once

#include "LexerDiagnostics.h"
#include "ParserDiagnostics.h"
#include "RuntimeError.h"

std::string messageFor(LexerDiagnosticKind const&);
std::string messageFor(ParserDiagnosticKind const&);
std::string messageFor(RuntimeErrorKind const&);
