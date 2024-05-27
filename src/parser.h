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
    std::size_t pos;
    String version;
    String encoding;
    bool is_external;
    bool is_parameter = false;
    // Parameter entities only.
    bool in_entity_value;
    bool leading_parameter_space_done = false;
    bool trailing_parameter_space_done = false;
    EntityStream(const String&);
    EntityStream(const String&, bool);
    Char get();
    void operator++();
    bool eof();
    void parse_text_declaration();
};


class Parser {
    // Don't repeat code - use parser get/++/eof for external entity parsing.
    friend class EntityStream;
    // Note, unique (dynamic) pointer for strings and normal pointers for input streams. 
    std::variant<unique_istream_ptr, std::istream*> stream;
    std::unique_ptr<StringBuffer> string_buffer = nullptr;
    Char previous_char = -1;
    // Mainly because recursive entity references possible.
    std::stack<EntityStream> general_entity_stack;
    std::stack<EntityStream> parameter_entity_stack;
    bool general_entity_active = false;
    std::size_t parameter_entity_pos = 0;
    bool just_parsed_character_reference = false;
    bool parameter_entity_active = false;
    bool just_parsed_carriage_return = false;
    bool external_dtd_content_active = false;
    bool standalone = false;

    String parse_name(
        const String&, bool validate = true, const ParameterEntities* parameter_entities = nullptr);
    String parse_nmtoken(const String&, const ParameterEntities&);
    String parse_attribute_value(
        const DoctypeDeclaration&, bool references_active = true, bool is_cdata = true);
    String parse_entity_value(const DoctypeDeclaration&);
    Char parse_character_entity();
    String parse_general_entity_name(const GeneralEntities&);
    void parse_general_entity(const GeneralEntities&);
    String parse_general_entity_text(const GeneralEntities&, std::function<void(Char)>, int = 0);
    void end_general_entity();
    void parse_parameter_entity(const ParameterEntities&, bool);
    void end_parameter_entity();
    std::pair<String, String> parse_attribute(
        const DoctypeDeclaration&, bool references_active = true,
        bool is_cdata = true, const String* tag_name = nullptr);
    Tag parse_tag(const DoctypeDeclaration&);
    void parse_comment();
    String parse_cdata();
    ProcessingInstruction parse_processing_instruction(bool detect_xml_declaration = false);
    void parse_xml_declaration(Document&);
    DoctypeDeclaration parse_doctype_declaration();
    String parse_public_id(const ParameterEntities* parameter_entities = nullptr);
    String parse_system_id(const ParameterEntities* parameter_entities = nullptr);
    ExternalID parse_external_id(const ParameterEntities* parameter_entities = nullptr);
    void parse_dtd_subsets(DoctypeDeclaration&, bool = false, bool = false);
    void start_external_subset(const String&);
    void parse_markup_declaration(DoctypeDeclaration&);
    void parse_conditional_section(DoctypeDeclaration&, int);
    void parse_include_section(DoctypeDeclaration&);
    void parse_ignore_section();
    void parse_element_declaration(DoctypeDeclaration&);
    ElementContentModel parse_element_content_model(const ParameterEntities&, int);
    MixedContentModel parse_mixed_content_model(const ParameterEntities&, int);
    void parse_attribute_list_declaration(DoctypeDeclaration&);
    void parse_attribute_declaration(DoctypeDeclaration&, AttributeListDeclaration&);
    std::set<String> parse_enumerated_attribute(AttributeType, const ParameterEntities&);
    std::set<String> parse_notations(const ParameterEntities&);
    std::set<String> parse_enumeration(const ParameterEntities&);
    void parse_entity_declaration(DoctypeDeclaration&);
    void parse_general_entity_declaration(DoctypeDeclaration&);
    void parse_parameter_entity_declaration(DoctypeDeclaration&);
    void parse_notation_declaration(DoctypeDeclaration&);
    Char get();
    Char get(const GeneralEntities&);
    Char get(
        const ParameterEntities&, bool in_markup = true, bool in_entity_value = false,
        bool ignore_whitespace_after_percent_sign = false);
    void operator++();
    bool eof();
    bool general_entity_eof();
    bool parameter_entity_eof();
    void ignore_whitespace();
    void ignore_whitespace(const ParameterEntities& parameter_entities);
    Element parse_element(const DoctypeDeclaration&, bool = false);
    public:
        Element parse_element();
        Document parse_document(bool validate_elements = false, bool validate_attribute = false);
        Parser(const std::string&);
        Parser(std::istream&);
};


}