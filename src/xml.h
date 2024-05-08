// Main module of the XML parser library.
#pragma once
#include <string>
#include "utils.h"
#include "parser.h"


namespace xml {

// Accept string to parse, and returns the parsed document.
Document parse(const std::string&);

}