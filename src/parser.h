// Main parsing logic goes here.
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
    String parse_name(const String&, bool = true);
    std::pair<String, String> parse_attribute();
    Tag parse_tag();
    void parse_comment();
    String parse_cdata();
    ProcessingInstruction parse_processing_instruction();
    public:
        Parser(const std::string&);
        Char get();
        Char peek();
        void operator++();
        bool eof();
        Element parse_element(bool = false);
};

}