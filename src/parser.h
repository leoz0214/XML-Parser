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
    ProcessingInstruction parse_processing_instruction(bool = false);
    void parse_xml_declaration(Document&);
    DoctypeDeclaration parse_doctype_declaration();
    void parse_internal_dtd_subset(DoctypeDeclaration&);
    void parse_markup_declaration(DoctypeDeclaration&);
    String parse_public_id();
    String parse_system_id();
    ExternalID parse_external_id();
    Char get();
    Char peek();
    void operator++();
    bool eof();
    public:
        Parser(const std::string&);
        Element parse_element(bool = false);
        Document parse_document();
};

}