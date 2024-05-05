// Main module of the XML parser library.
#pragma once
#include <string>
#include "utils.h"
#include "parser.h"


namespace xml {

// Actual parsing logic - toplevel parsing function for Document.
void parse(Parser&);
// Accept string to parse.
void parse(const std::string&);

}