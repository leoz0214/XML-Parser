// Wraps input data into an abstracted stream, returning the current
// Unicode character as an integer value each time it is requested.
#pragma once
#include <string>
#include <utility>
#include <vector>
#include "utils.h"


namespace xml {

class Parser {
    const std::string* data;
    std::size_t pos = 0;
    std::size_t increment = 0;
    String parse_name(const String&);
    std::pair<String, String> parse_attribute();
    Tag parse_tag();
    public:
        Parser(const std::string&);
        Char get();
        void operator++();
        bool eof();
        Element parse_element(bool = false);
};

}