// General utilities for the parser.
#pragma once
#include <algorithm>
#include <array>
#include "stream.h"


namespace xml {


// Whitespace characters as per standard.
static std::array<Char, 4> WHITESPACE {0x20, 0x09, 0x0D, 0x0A};
// Returns True if characters is indeed whitespace.
bool is_whitespace(Char);

}