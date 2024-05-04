#include "utils.h"


namespace xml {

bool is_whitespace(Char c) {
    return std::find(WHITESPACE.cbegin(), WHITESPACE.cend(), c) != WHITESPACE.cend();
}

}