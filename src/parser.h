// Main parsing logic goes here.
#pragma once
#include <functional>
#include <istream>
#include <memory>
#include <string>
#include <utility>
#include <stack>
#include <variant>
#include <vector>
#include "utils.h"


namespace xml {

typedef std::unique_ptr<std::istream> unique_istream_ptr;

class Parser;

struct EntityStream {
    String text;
    std::unique_ptr<Parser> parser = nullptr;
    unique_istream_ptr stream = nullptr;
    std::size_t pos = 0;
    String version = "1.0";
    String encoding = "utf-8";
    bool is_external;
    EntityStream(const String&);
    EntityStream(const String&, bool);
    Char get();
    void operator++();
    bool eof();
};


class Parser {
    // Don't repeat code - use parser get/++/eof for external entity parsing.
    friend class EntityStream;
    // Note, unique (dynamic) pointer for strings and normal pointers for input streams. 
    std::variant<unique_istream_ptr, std::istream*> stream;
    std::unique_ptr<StringBuffer> string_buffer;
    Char previous_char = -1;
    // Mainly because recursive entity references possible.
    std::stack<EntityStream> general_entity_stack;
    std::stack<EntityStream> parameter_entity_stack;
    bool general_entity_active = false;
    std::size_t parameter_entity_pos = 0;
    bool just_parsed_character_reference = false;
    bool parameter_entity_active = false;
    bool just_parsed_carriage_return = false;

    String parse_name(const String&, bool validate = true);
    String parse_nmtoken(const String&);
    String parse_attribute_value(const DoctypeDeclaration&, bool references_active = true);
    String parse_entity_value(const DoctypeDeclaration&);
    Char parse_character_entity();
    String parse_general_entity_name(const GeneralEntities&);
    void parse_general_entity(const GeneralEntities&);
    String parse_general_entity_text(const GeneralEntities&, std::function<void(Char)>, int = 0);
    void end_general_entity();
    void parse_parameter_entity(const ParameterEntities&);
    void end_parameter_entity();
    std::pair<String, String> parse_attribute(const DoctypeDeclaration&, bool references_active = true);
    Tag parse_tag(const DoctypeDeclaration&);
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
    void parse_attribute_declaration(DoctypeDeclaration&, AttributeListDeclaration&);
    std::set<String> parse_enumerated_attribute(AttributeType);
    std::set<String> parse_notations();
    std::set<String> parse_enumeration();
    void parse_entity_declaration(DoctypeDeclaration&);
    void parse_general_entity_declaration(DoctypeDeclaration&);
    void parse_parameter_entity_declaration(DoctypeDeclaration&);
    void parse_notation_declaration(DoctypeDeclaration&);
    Char get();
    Char get(const GeneralEntities&);
    Char get(const ParameterEntities&);
    void operator++();
    bool eof();
    bool general_entity_eof();
    bool parameter_entity_eof();
    void ignore_whitespace();
    Element parse_element(const DoctypeDeclaration&, bool = false);
    public:
        Element parse_element();
        Document parse_document();
        Parser(const std::string&);
        Parser(std::istream&);
};


}