// Main module of the XML parser library.
#pragma once
#include <string>
#include "stream.h"
#include "utils.h"


namespace xml {

// Actual parsing logic - toplevel parsing function for Document.
void parse(Stream&);
// Accept string to parse.
void parse(const std::string&);

}