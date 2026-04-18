#include "Token.h"


std::ostream& operator<<(std::ostream& oss, Token const& pos) {
    return oss << std::format("{}", pos);
}
