// Main parsing logic goes here.
#pragma once
#include <filesystem>
#include <functional>
#include <istream>
#include <map>
#include <memory>
#include <stack>
#include <string>
#include <utility>
#include <variant>
#include <vector>
#include "utils.h"


namespace xml {
// Pointer to a existing input stream.
typedef std::unique_ptr<std::istream> unique_istream_ptr;

class Parser;

// General/parameter entity stream (may be internal or from a file - external).
struct EntityStream {
    String text; // Entity text (internal only).
    String name; // Entity name
    std::unique_ptr<Parser> parser = nullptr; // Pointer to wrapped parser (external only).
    unique_istream_ptr stream = nullptr; // Pointer to file stream (external only).
    std::filesystem::path file_path; // File path (external only).
    std::size_t pos; // Position in string (internal only).
    String version; // XML Version (external only).
    String encoding; // Encoding (external only).
    bool is_external; // Is external entity?
    bool is_parameter = false; // Is parameter entity?
    // Currently in an entity value (for whether to include leading/trailing space in PE).
    bool in_entity_value;
    // PE only. All PE text starts with space if not in entity.
    bool leading_parameter_space_done = false; 
     // PE only. All PE text starts with space if not in entity.
    bool trailing_parameter_space_done = false;
    // Internal entity constructor.
    EntityStream(const String&, const String&);
    // External entity constructor.
    EntityStream(const std::filesystem::path&, const String&);
    // Returns current character in entity stream.
    Char get();
    // Increments the entity stream to the next character.
    void operator++();
    // End of entity reached (either end of file stream or end of string).
    bool eof();
    // Parse text declaration if present (external only).
    void parse_text_declaration();
    // Returns error object due to premature EOF.
    XmlError get_eof_error_object();
};


// Main parser class - used to parse document whilst maintaining relevant state.
class Parser {
    // Don't repeat code - use parser get/++/eof for external entity parsing.
    friend class EntityStream;
    // Note, unique (dynamic) pointer for strings and normal pointers for input streams. 
    std::variant<unique_istream_ptr, std::istream*> stream;
    // Stream that can wrap a string and access it without copying.
    std::unique_ptr<StringBuffer> string_buffer = nullptr;
    // Value of previously parsed character.
    Char previous_char = -1;
    // Stack to track general entities.
    // Mainly because recursive general entity references possible.
    std::stack<EntityStream> general_entity_stack;
    // Stack to track parameter entities.
    // Mainly because recursive parameter entity references possible.
    std::stack<EntityStream> parameter_entity_stack;
    // Track file paths of all resources (external files).
    std::stack<std::filesystem::path> resource_paths;
    // Maps each resource path to its corresponding stream object.
    std::map<std::filesystem::path, EntityStream*> resource_to_stream;
    // Track seen general entity names to detect self references (avoid infinite recursion).
    std::set<String> general_entity_names;
    // Track seen parameter entity names to detect self references (avoid infinite recursion).
    std::set<String> parameter_entity_names;
    bool general_entity_active = false; // Currently inside general entity?
    bool just_parsed_character_reference = false; // Last returned character was character reference?
    bool parameter_entity_active = false; // Currently inside parameter entity?
    bool just_parsed_carriage_return = false; // Last returned character was carriage return?
    bool external_dtd_content_active = false; // Currently inside external DTD?
    bool standalone = false; // Document is standalone (avoid passing around document object like crazy).
    std::size_t line_number = 1; // Current line number based on stream position (start from 1).
    std::size_t line_pos = 1; // Position on current line based on stream position (start from 1).

    // Parse a Name, with optional validation (default active), parameter entities
    // and a set of names that are exempt to validation requirements.
    String parse_name(
        const String&, bool validate = true, const ParameterEntities* parameter_entities = nullptr,
        const std::set<String>* validation_exeptions = nullptr);
    // Parse a Nmtoken.
    String parse_nmtoken(const String&, const ParameterEntities&);
    // Parse an attribute value, where references may or may not be recognised, 
    // including normalisation that varies by attribute type (CDATA or not).
    String parse_attribute_value(
        const DoctypeDeclaration&, bool references_active = true, bool is_cdata = true);
    // Parse an entity value in an entity declaration.
    String parse_entity_value(const DoctypeDeclaration&);
    // Parse a character reference.
    Char parse_character_reference();
    // Parse the name of a general entity.
    String parse_general_entity_name(const GeneralEntities&);
    // Parse each general entity character, applying a given function on each character.
    String parse_general_entity_text(const GeneralEntities&, std::function<void(Char)>, int);
    // Parse the occurrence of a general entity.
    void parse_general_entity(const GeneralEntities&, bool);
    // Retrieves the folder of the current entity, blank if in the main document.
    std::filesystem::path get_folder_path();
    // Retrieves the file of the current entity, blank if in the main document.
    std::filesystem::path get_file_path();
    // Confirms the end of a general entity and resets relevant variables.
    void end_general_entity();
    // Parse the occurrence of a parameter entity.
    void parse_parameter_entity(const ParameterEntities&, bool);
    void end_parameter_entity();
    // Parse an attribute {name, value} pair.
    std::pair<String, String> parse_attribute(
        const DoctypeDeclaration&, bool references_active = true,
        bool is_cdata = true, const String* tag_name = nullptr);
    // Parse a start, end or empty tag.
    Tag parse_tag(const DoctypeDeclaration&);
    // Ignores a comment (stripped away).
    void parse_comment();
    // Retrieves the text of a CDATA (raw text) section where markup is not detected.
    String parse_cdata();
    // Parse a processing instruction, determing the target and instruction.
    ProcessingInstruction parse_processing_instruction(bool detect_xml_declaration = false);
    // Parse the XML declaration (metadata about the document)
    void parse_xml_declaration(Document&);
    // Parse the DOCTYPE declaration section of the document.
    DoctypeDeclaration parse_doctype_declaration();
    // Parse a public ID, returning the corresponding path.
    std::filesystem::path parse_public_id();
    // Parse a system ID, returning the corresponding path.
    std::filesystem::path parse_system_id();
    // Parse an external ID, returning the corresponding object.
    ExternalID parse_external_id(const ParameterEntities* parameter_entities = nullptr);
    // Parse either the internal DTD, external DTD, or INCLUDE section.
    void parse_dtd_subsets(DoctypeDeclaration&, bool = false, bool = false);
    // Set up parsing the external DTD subset.
    void start_external_subset(const std::filesystem::path&);
    // Detects the markup declaration type and calls the corresponding parse method.
    void parse_markup_declaration(DoctypeDeclaration&);
    // Detect conditional section type and calls the correponding parse method.
    void parse_conditional_section(DoctypeDeclaration&, int);
    // Parse an INCLUDE section.
    void parse_include_section(DoctypeDeclaration&);
    // Parse an IGNORE section (ignore all data, do not recognise parameter entities).
    void parse_ignore_section();
    // Parse an element declaration <!ELEMENT ...>
    void parse_element_declaration(DoctypeDeclaration&);
    // Parse an element content model specifying pattern of child element appearance.
    ElementContentModel parse_element_content_model(const ParameterEntities&, int);
    // Parse a mixed content model specifying permitted child element types.
    MixedContentModel parse_mixed_content_model(const ParameterEntities&, int);
    // Parse an attribute list declaration <!ATTLIST ...>
    void parse_attribute_list_declaration(DoctypeDeclaration&);
    // Parse an attribute declaration: name, type, (presence), (default).
    void parse_attribute_declaration(DoctypeDeclaration&, AttributeListDeclaration&);
    // DRY method for parsing list of notation names / enumeration values within attribute declaration.
    std::set<String> parse_enumerated_attribute(AttributeType, const ParameterEntities&);
    // Parse notation names for NOTATION attribute.
    std::set<String> parse_notations(const ParameterEntities&);
    // Parse enumeration values for enumeration attribute.
    std::set<String> parse_enumeration(const ParameterEntities&);
    // Parse a general/parameter entity declaration <!ENTITY ...>
    void parse_entity_declaration(DoctypeDeclaration&);
    // Parse a general entity declaration.
    void parse_general_entity_declaration(DoctypeDeclaration&);
    // Parse a parameter entity declaration.
    void parse_parameter_entity_declaration(DoctypeDeclaration&);
    // Parse a notation declaration <!NOTATION ...>.
    void parse_notation_declaration(DoctypeDeclaration&);
    // Gets the current character without detecting references of any kind.
    Char get();
    // Gets the current character whilst possibly detecting character or general entity references.
    Char get(const GeneralEntities&, bool in_attribute_value = false);
    // Gets the current character whilst possibly detecting parameter entity references.
    Char get(
        const ParameterEntities&, bool in_markup = true, bool in_entity_value = false,
        bool ignore_whitespace_after_percent_sign = false);
    // Increments the parser to process the next character.
    void operator++();
    // Returns true if the end of the stream of the parser has been reached.
    bool eof();
    // Returns true if exactly one general entity is active and it is at its EOF.
    bool general_entity_eof();
    // Returns true if exactly one parameter entity is active and it is at its EOF.
    bool parameter_entity_eof();
    // Skips all whitespace characters.
    void ignore_whitespace();
    // Skips all whitespace characters with parameter entities in mind.
    void ignore_whitespace(const ParameterEntities&);
    // Parse a given element.
    Element parse_element(const DoctypeDeclaration&, bool = false);
    // Returns an error object with the current stream position included.
    XmlError get_error_object(const std::string&);
    public:
        // Parse a give element.
        Element parse_element();
        // Start method - document parsing begins here.
        Document parse_document(bool validate_elements = false, bool validate_attributes = false);
        // String constructor.
        Parser(const std::string&);
        // Stream constructor.
        Parser(std::istream&);
};


}