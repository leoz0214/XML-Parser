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
    String parse_name(const String&, bool validate = true);
    std::pair<String, String> parse_attribute();
    Tag parse_tag();
    void parse_comment();
    String parse_cdata();
    ProcessingInstruction parse_processing_instruction(bool detect_xml_declaration = false);
    void parse_xml_declaration(Document&);
    DoctypeDeclaration parse_doctype_declaration();
    String parse_public_id();
    String parse_system_id();
    ExternalID parse_external_id();
    void parse_internal_dtd_subset(DoctypeDeclaration&);
    void parse_markup_declaration(DoctypeDeclaration&);
    void parse_element_declaration(DoctypeDeclaration&);
    ElementContentModel parse_element_content_model();
    MixedContentModel parse_mixed_content_model();
    void parse_attribute_list_declaration(DoctypeDeclaration&);
    void parse_attribute_declaration(AttributeListDeclaration&);
    void parse_notation_declaration(DoctypeDeclaration&);
    Char get();
    Char peek();
    void operator++();
    bool eof();
    void ignore_whitespace();
    public:
        Parser(const std::string&);
        Element parse_element(bool = false);
        Document parse_document();
};

}