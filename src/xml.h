// Main module of the XML parser library.
#pragma once
#include <string>
#include "utils.h"
#include "parser.h"


namespace xml {

// Accept string to parse, and returns the parsed document.
Document parse(const std::string&);
// Accemt input stream to parse, and returns the parsed document.
// Leaves the stream at end if successfully otherwise unknown state if error.
Document parse(std::istream&);

}