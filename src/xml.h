// Main module of the XML parser library.
#pragma once
#include <string>
#include "utils.h"
#include "parser.h"


namespace xml {

// Accepts a std::string to parse, and returns the parsed document.
// By default, elements and attributes are thoroughly validated but
// this can be turned off by setting the corresponding parameter to false.
Document parse(const std::string&, bool = true, bool = true);
// Accepts an input stream to parse, and returns the parsed document.
// Leaves the stream at the end if successfully parsed, otherwise unknown state if error.
// By default, elements and attributes are thoroughly validated but
// this can be turned off by setting the corresponding parameter to false.
Document parse(std::istream&, bool = true, bool = true);

}