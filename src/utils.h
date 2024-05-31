// General utilities for the parser.
#pragma once
#include <algorithm>
#include <climits>
#include <filesystem>
#include <initializer_list>
#include <istream>
#include <map>
#include <set>
#include <stdexcept>
#include <streambuf>
#include <string>
#include <utility>
#include <vector>


namespace xml {

// Each character is represented as a 4-byte integer to support the full Unicode range.
typedef int Char;
// Unicode string class for use in the XML parser.
class String : public std::vector<Char> {
    public:
        using std::vector<Char>::vector;
        String(const char*);
        String(const std::string&);
        // Display the string (converts to std::string first).
        friend std::ostream& operator<<(std::ostream&, const String&);
        // Convert Unicode characters in their numeric values
        // back to their original binary form (std::string compatibility).
        operator std::string() const;
};
// Max values in UTF-8 representable in 1 byte, 2 bytes, 3 bytes, 4 bytes.
constexpr Char UTF8_BYTE_LIMITS[4] {0x7F, 0x7FF, 0xFFFF, 0x10FFFF};
// Fills UTF-8 byte with values (number of right bits to use and current value left).
void fill_utf8_byte(char&, Char&, int);
// Parses a UTF-8 character
Char parse_utf8(std::istream&);
// Convenient streambuf wrapper for handling std::string objects as buffer.
// Can then be used in istream. Better to have one stream interface
// for both strings and normal streams than duplicate similar code.
class StringBuffer : public std::streambuf {
    public:
        StringBuffer(const std::string&);
};

// Error class for all errors that occur during XML parsing/validation.
// Can be considered a runtime error since XML data is parsed dynamically.
class XmlError : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

// Character constants.
constexpr Char LEFT_ANGLE_BRACKET = '<';
constexpr Char RIGHT_ANGLE_BRACKET = '>';
constexpr Char LEFT_SQUARE_BRACKET = '[';
constexpr Char RIGHT_SQUARE_BRACKET = ']';
constexpr Char LEFT_PARENTHESIS = '(';
constexpr Char RIGHT_PARENTHESIS = ')';
constexpr Char SOLIDUS = '/';
constexpr Char EQUAL = '=';
constexpr Char SINGLE_QUOTE = '\'';
constexpr Char DOUBLE_QUOTE = '"';
constexpr Char AMPERSAND = '&';
constexpr Char EXCLAMATION_MARK = '!';
constexpr Char QUESTION_MARK = '?';
constexpr Char HYPHEN = '-';
constexpr Char ASTERISK = '*';
constexpr Char PLUS = '+';
constexpr Char COMMA = ',';
constexpr Char VERTICAL_BAR = '|';
constexpr Char OCTOTHORPE = '#';
constexpr Char SPACE = ' ';
constexpr Char PERCENT_SIGN = '%';
constexpr Char SEMI_COLON = ';';
constexpr Char CARRIAGE_RETURN = '\r';
constexpr Char LINE_FEED = '\n';

// Whitespace characters as per standard.
static String WHITESPACE {SPACE, 0x09, CARRIAGE_RETURN, LINE_FEED};
static String WHITESPACE_AND_RIGHT_ANGLE_BRACKET = []{
    String valid_chars = WHITESPACE;
    valid_chars.push_back(RIGHT_ANGLE_BRACKET);
    return valid_chars;
}();

// Returns true if character is indeed whitespace.
bool is_whitespace(Char);

// Characters which may signal end of tag name.
static String START_EMPTY_TAG_NAME_TERMINATORS = []{
    String valid_chars = WHITESPACE;
    valid_chars.insert(valid_chars.end(), {RIGHT_ANGLE_BRACKET, SOLIDUS});
    return valid_chars;
}();
// End tag name cannot end with solidus (solidus at back of other vector - exclude it).
static String END_TAG_NAME_TERMINATORS = []{
    String valid_chars = START_EMPTY_TAG_NAME_TERMINATORS;
    valid_chars.erase(std::find(valid_chars.begin(), valid_chars.end(), SOLIDUS));
    return valid_chars;
}();

// Order character ranges by max in range.
static auto character_ranges_comparator = [](const std::pair<Char, Char>& a, const std::pair<Char, Char>& b) {
    return a.second < b.second;
};
// Character ranges are min/max Unicode value pairs in ascending order.
typedef std::set<std::pair<Char, Char>, decltype(character_ranges_comparator)> CharacterRanges;

// Accepted character data as per standard. All chars in document must lie in one of these ranges.
static CharacterRanges CHARACTER_RANGES({
    {0x09, 0x0A}, {0x0D, 0x0D}, {0x20, 0xD7FF}, {0xE000, 0xFFFD}, {0x10000, 0x10FFFF}
}, character_ranges_comparator);
// Returns true if a character is in one of the given character ranges.
bool in_any_character_range(Char, const CharacterRanges&);
// Returns true if a character is allowed at all in XML data.
bool valid_character(Char);

// Valid start name character ranges.
static CharacterRanges NAME_START_CHARACTER_RANGES({
    {':', ':'}, {'A', 'Z'}, {'_', '_'}, {'a', 'z'}, {0xC0, 0xD6}, {0xD8, 0xF6}, {0xF8, 0x2FF},
    {0x370, 0x37D}, {0x37F, 0x1FFF}, {0x200C, 0x200D}, {0x2070, 0x218F}, {0x2C00, 0x2FEF},
    {0x3001, 0xD7FF}, {0xF900, 0xFDCF}, {0xFDF0, 0xFFFD}, {0x10000, 0xEFFFF}
}, character_ranges_comparator);
// Valid name character ranges.
static CharacterRanges NAME_CHARACTER_RANGES = []{
    CharacterRanges character_ranges = NAME_START_CHARACTER_RANGES;
    // Characters not allowed at start but allowed after the first character.
    CharacterRanges additional_character_ranges ({
        {'-', '.'}, {'0', '9'}, {0xB7, 0xB7}, {0x0300, 0x036F}, {0x203F, 0x2040}
    }, character_ranges_comparator);
    character_ranges.merge(additional_character_ranges);
    return character_ranges;
}();
// Returns true if a characteer is a valid name start character.
bool valid_name_start_character(Char);
// Returns true if a character is a valid name character.
bool valid_name_character(Char);
// Returns true if a name is valid (optionally checking all characters if not already).
bool valid_name(const String&, bool check_all_chars = false);
// DRY - validation of Names and Nmtokens very similar.
bool valid_names_or_nmtokens(const String&, bool);
// Returns true if string matches Names.
// Restricted to attribute values only.
bool valid_names(const String&);
// Returns true if string matches Nmtoken (sequence of name characters).
bool valid_nmtoken(const String&);
// Returns true if string matches Nmtokens.
bool valid_nmtokens(const String&);

// Left angle bracket and ampersand are disallowed literal characters in attribute values.
static String INVALID_ATTRIBUTE_VALUE_CHARACTERS {LEFT_ANGLE_BRACKET, AMPERSAND};
static String ATTRIBUTE_NAME_TERMINATORS = []{
    String valid_chars = WHITESPACE;
    valid_chars.push_back(EQUAL);
    return valid_chars;
}();
// Returns true if a character is a valid literal attribute character.
bool valid_attribute_value_character(Char);

// XML declaration handling.
static String XML_DECLARATION_VERSION_NAME = "version";
static String XML_DECLARATION_ENCODING_NAME = "encoding";
static String XML_DECLARATION_STANDALONE_NAME = "standalone";
// Currently supported encodings by THIS XML PARSER (lower-case).
static std::vector<String> SUPPORTED_ENCODINGS {"utf-8"};
// Standalone is either 'yes' (true) or 'no' (false).
static std::map<String, bool> STANDALONE_VALUES {{"yes", true}, {"no", false}};
// Returns true for a valid version specified (1.x).
bool valid_version(const String&);
// Returns true for a valid encoding specified (as recognised by this parser).
bool valid_encoding(const String&);
// Obtains the standalone value from the literal, if possible.
bool get_standalone_value(const String&);

// Tag types: start (opening), end (closing), empty.
enum class TagType {start, end, empty};
// Attributes list - map is suitable and convenient (sorted keys).
typedef std::map<String, String> Attributes;
// Tag class - either start, end or empty tag.
struct Tag {
    String name; // Tag name
    TagType type; // Tag type (start/end/empty)
    Attributes attributes; // Map of tag attribute name/value pairs.
};

// Processing instruction class.
struct ProcessingInstruction {
    String target; // Name of the target application this PI is directed to.
    String instruction; // The instruction.
};
// Characters which may signal end of processing instruction target name.
static String PROCESSING_INSTRUCTION_TARGET_NAME_TERMINATORS = []{
    String valid_chars = WHITESPACE;
    valid_chars.push_back(QUESTION_MARK);
    return valid_chars;
}();
// Returns true if the processing instruction name is valid.
bool valid_processing_instruction_target(const String&);

// Element class - including child elements, text etc.
struct Element {
    String text; // All character data in the element (excluding text in child elements).
    Tag tag; // Underlying tag info: name, type, attributes
    std::vector<Element> children; // Child elements in order of being parsed.
    std::vector<ProcessingInstruction> processing_instructions; // PIs in order of being parsed.
    bool is_empty = true; // Element is EMPTY
    // Only child elements and raw whitespace (no CDATA, characteer references etc).
    bool children_only = true;
};

// External ID types for external entities.
enum class ExternalIDType {system, public_, none};
// External ID class. May include a system ID, public ID, both, or neither.
struct ExternalID {
    ExternalIDType type = ExternalIDType::none; // External ID type.
    // Only should be accessed if external ID is SYSTEM or PUBLIC.
    std::filesystem::path system_id;
    // Only should be accessed if external ID is PUBLIC.
    std::filesystem::path public_id;
};
// Expected literals indicating external IDs.
static std::map<String, ExternalIDType> EXTERNAL_ID_TYPES {
    {"SYSTEM", ExternalIDType::system}, {"PUBLIC", ExternalIDType::public_}
};
// Returns the type of external ID if posssible.
ExternalIDType get_external_id_type(const String&);
// Valid public ID characters.
static CharacterRanges PUBLIC_ID_CHARACTER_RANGES({
    {'a', 'z'}, {'A', 'Z'}, {'0', '9'}
}, character_ranges_comparator);
// Too cumbersome to create separate range per valid public ID character.
static String PUBLIC_ID_CHARACTERS = "-'()+,./:=?;!*#@$_% \u000d\u000a";
// Returns true if a character is a valid public ID character.
bool valid_public_id_character(Char);

// Element type info.
enum class ElementType {empty, any, mixed, children};
// Element appearance expectation for an element content model.
enum class ElementContentCount {one, zero_or_one, zero_or_more, one_or_more};
// Base case: Specification of the expected child elements within an element.
// Also used recursively for parenthesised parts of a ECM.
struct ElementContentModel {
    // Number of matches expected (1, 0 or 1, 0+ or 1+)
    ElementContentCount count = ElementContentCount::one;
    // Specifies element appearance ORDER if true, otherwise
    // specifies that one or more parts must match.
    bool is_sequence = true;
    std::vector<ElementContentModel> parts; // Sub ECMs (possibly element names only).
    // For leaves of the ECM only (standalone names with possible count).
    bool is_name = false;
    String name; // Name of the element to match (leaves only).
};
// Specification of names of child elements allowed within an element.
struct MixedContentModel {
    std::set<String> choices; // Accepted child tag names. Does not contain #PCDATA (implicit).
};
// Element declaration as per <!ELEMENT ...>
struct ElementDeclaration {
    ElementType type; // Type of element.
    String name; // Name of element.
    // Only for element type 'children'.
    ElementContentModel element_content; 
    // Only for element type 'mixed'.
    MixedContentModel mixed_content;
};
// Symbols mapping to element counts in ECM.
// ? -> 0 or 1, * -> 0 or more, + -> 1 or more
static std::map<Char, ElementContentCount> ELEMENT_CONTENT_COUNT_SYMBOLS {
    {QUESTION_MARK, ElementContentCount::zero_or_one},
    {ASTERISK, ElementContentCount::zero_or_more}, {PLUS, ElementContentCount::one_or_more}
};
// Min number of occurrences of an element given the count type.
static std::map<ElementContentCount, int> ELEMENT_CONTENT_COUNT_MINIMA {
    {ElementContentCount::one, 1}, {ElementContentCount::one_or_more, 1},
    {ElementContentCount::zero_or_one, 0}, {ElementContentCount::zero_or_more, 0}
};
// Max number of occurrences of an element given the count type.
static std::map<ElementContentCount, int> ELEMENT_CONTENT_COUNT_MAXIMA {
    {ElementContentCount::one, 1}, {ElementContentCount::one_or_more, INT_MAX},
    {ElementContentCount::zero_or_one, 1}, {ElementContentCount::zero_or_more, INT_MAX}
};
// Possible characters that can precede an element name in an ECM.
static String ELEMENT_CONTENT_NAME_TERMINATORS = []{
    String valid_chars = WHITESPACE;
    valid_chars.insert(valid_chars.end(), {
        QUESTION_MARK, ASTERISK, PLUS, COMMA, VERTICAL_BAR, RIGHT_PARENTHESIS});
    return valid_chars;
}();
static String PCDATA = "PCDATA";
// Possible characters that can precede an element name in a MCM.
static String MIXED_CONTENT_NAME_TERMINATORS = []{
    String valid_chars = WHITESPACE;
    valid_chars.insert(valid_chars.end(), {VERTICAL_BAR, RIGHT_PARENTHESIS});
    return valid_chars;
}();

// All types of attributes that exist in XML.
enum class AttributeType {
    cdata, id, idref, idrefs, entity, entities, nmtoken, nmtokens, notation, enumeration
};
// Literals mapping to a certain attribute type.
// Note, enumerations will be detected from opening '(' so are not in this map.
static std::map<String, AttributeType> ATTRIBUTE_TYPES {
    {"CDATA", AttributeType::cdata}, {"ID", AttributeType::id},
    {"IDREF", AttributeType::idref}, {"IDREFS", AttributeType::idrefs},
    {"ENTITY", AttributeType::entity}, {"ENTITIES", AttributeType::entities},
    {"NMTOKEN", AttributeType::nmtoken}, {"NMTOKENS", AttributeType::nmtokens},
    {"NOTATION", AttributeType::notation}
};
// Required -> mandatory - no default, Implied -> optional - no default, 
// Fixed -> constant value (default), Relaxed -> with default, can override
enum class AttributePresence {required, implied, fixed, relaxed};
// Literals denoting a demanded attribute presence (after #).
static std::map<String, AttributePresence> ATTRIBUTE_PRESENCES {
    {"REQUIRED", AttributePresence::required}, {"IMPLIED", AttributePresence::implied},
    {"FIXED", AttributePresence::fixed}
};

// Represents an attribute declaration.
struct AttributeDeclaration {
    String name; // Name of attribute.
    AttributeType type; // Type of attribute.
    AttributePresence presence = AttributePresence::relaxed; // Attribute presence requirements.
    std::set<String> notations; // Notation names: Only for notation attributes.
    std::set<String> enumeration; // Enumeration values: only for enumeration attributes.
    bool has_default_value = false; // Default value has been provided.
    String default_value; // Only access if default value has been provided.
    bool from_external = false; // Attribute is declared in external DTD or parameter entity.
};
// Retrieves the attribute type from the given literal string if possible.
AttributeType get_attribute_type(const String&);
// Retrieves the attribute presence from the given literal string if possible.
AttributePresence get_attribute_presence(const String&);
// Attribute names mapping to their corresponding declarations.
typedef std::map<String, AttributeDeclaration> AttributeListDeclaration;
// Possible characters preceding declaring the list of possible notation/enum attribute values.
static String ENUMERATED_ATTRIBUTE_NAME_TERMINATORS = []{
    String valid_chars = WHITESPACE;
    valid_chars.insert(valid_chars.end(), {VERTICAL_BAR, RIGHT_PARENTHESIS});
    return valid_chars;
}();
// The xml:space special attribute allows specification of whitespace handling.
// It is ignored by this parser, but still may exist and thus must be acknowleged.
static String XML_SPACE = "xml:space";
// Note, xml:lang meant to be valid langauge specifier as per
// https://datatracker.ietf.org/doc/html/rfc4646, but this is beyond the scope of this project.
// The user is trusted that the language is correct.
// The value of the xml:lang attribute does not affect parsing behaviour.
static String XML_LANG = "xml:lang";
// Special built-in attribute names that would be rejected otherwise.
static std::set<String> SPECIAL_ATTRIBUTE_NAMES {XML_SPACE, XML_LANG};
// For xml:space attributes, one or both of default/preserve must be in the enumeration.
static std::set<std::set<String>> XML_SPACE_ENUMS {
    {"default", "preserve"}, {"default"}, {"preserve"}
};

// Base entity class containing common attributes between general and parameter entities.
struct Entity {
    String name; // Name of entity.
    String value; // Value of entity (empty if external).
    bool is_external = false; // Entity points to external resource with XML data.
    ExternalID external_id; // External ID, for external entities only.
    bool from_external = false; // Entity is declared inside external subset/parameter entity.
};

// Parse a character reference and returns the corresponding Unicode character.
Char parse_character_reference(const String&);
// Expand all character entities in a given string, returning the resulting string.
String expand_character_references(const String&);

// Represents a general entity (one for use in the main data).
struct GeneralEntity : public Entity {
    bool is_unparsed = false; // General entities can also be unparsed (points to non-XML data).
    String notation_name; // The name of the notation (only if unparsed).
    GeneralEntity() = default;
    GeneralEntity(const String&);
};

// Represents a parameter entity (one for use in the DTD).
struct ParameterEntity : public Entity {};

// Notation handling.
struct NotationDeclaration {
    String name; // Name of notation.
    bool has_system_id; // Contains a system ID.
    bool has_public_id; // Contains a public ID.
    std::filesystem::path system_id; // Corresponding system ID if available.
    std::filesystem::path public_id; // Corresponding public ID if available.
};

// Map of element declarations (element name to declaration).
typedef std::map<String, ElementDeclaration> ElementDeclarations;
// Map of attribute list declarations (element name to declaration).
typedef std::map<String, AttributeListDeclaration> AttributeListDeclarations;
// Map of general entities (name to object).
typedef std::map<String, GeneralEntity> GeneralEntities;
// Map of parameter entities (name to object).
typedef std::map<String, ParameterEntity> ParameterEntities;
// Map of notation declaration (name to declaration).
typedef std::map<String, NotationDeclaration> NotationDeclarations;
// Name, value pairs for built-in general entities as per the standard.
static GeneralEntities BUILT_IN_GENERAL_ENTITIES {
    {"lt", String("&#60;")}, {"gt", String("&#62;")}, {"amp", String("&#38;")},
    {"apos", String("&#39;")}, {"quot", String("&#34;")}
};
// The built-in entities that MUST be double escaped if explicitly declared.
static std::set<String> BUILT_IN_GENERAL_ENTITIES_MANDATORY_DOUBLE_ESCAPE {"lt", "amp"};
// Stores info about the DOCTYPE declaration, if any.
// A DTD is optional - if not provided, assume zero restrictions on actual elements.
struct DoctypeDeclaration {
    bool exists = false; // Indicates whether DOCTYPE declaration exists in the document.
    String root_name; // Expected name of the root element.
    ExternalID external_id; // External ID to external DTD.
    std::vector<ProcessingInstruction> processing_instructions; // PIs in parse order.
    ElementDeclarations element_declarations; // Element declarations map (name->decl).
    AttributeListDeclarations attribute_list_declarations; // Attlist declarations map (element->decls).
    // General entities (name->object). Includes built-in entities upon construction.
    GeneralEntities general_entities = BUILT_IN_GENERAL_ENTITIES;
    ParameterEntities parameter_entities; // Parameter entities (name->object).
    NotationDeclarations notation_declarations; // Notation declarations map (name->decl).
};
// Characters which may signal end of root name in DTD.
static String DOCTYPE_DECLARATION_ROOT_NAME_TERMINATORS = []{
    String valid_chars = WHITESPACE;
    valid_chars.insert(valid_chars.end(), {LEFT_SQUARE_BRACKET, RIGHT_ANGLE_BRACKET});
    return valid_chars;
}();
// Characters which may signal end of a conditional section type name (INCLUDE/IGNORE).
static String CONDITIONAL_TYPE_NAME_TERMINATORS = []{
    String valid_chars = WHITESPACE;
    valid_chars.push_back(LEFT_SQUARE_BRACKET);
    return valid_chars;
}();

// Basic handling of URL resources instead of file paths.
// For some reason, URLs are detected as relative paths which is problematic.
// Therefore detecting URLs explicitly allows for special treatment.
// This XML parser does not support fetching resources over the network, only file system.
static std::set<String> RECOGNISED_PROTOCOLS {"http://", "https://"};
// Path starts with one of the recognised URL protocols.
bool is_url_resource(const std::string&);

// Ultimate document class - contains all information about the XML document.
struct Document {
    String version = "1.0"; // Document version as per XML declaration (1.0 otherwise).
    String encoding = "utf-8"; // Encoding as per XML declaration, in lower-case.
    bool standalone = false; // Indicates whether document is 'standalone' (false by default).
    DoctypeDeclaration doctype_declaration; // Corresponding doctype declaration of document.
    Element root; // The root (overarching) element.
    std::vector<ProcessingInstruction> processing_instructions; // PIs that appear in toplevel scope.
};

}