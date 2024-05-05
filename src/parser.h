// Wraps input data into an abstracted stream, returning the current
// Unicode character as an integer value each time it is requested.
#pragma once
#include <string>
#include <vector>
#include "utils.h"


namespace xml {

class Parser {
    const std::string* data;
    std::size_t pos = 0;
    std::size_t increment;
    public:
        Parser(const std::string&);
        Char get();
        void operator++();
        bool eof();

        String parse_name(const std::vector<char>&);
        Tag parse_tag();
        void parse_element();
};

}