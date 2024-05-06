// General utilities for the parser.
#pragma once
#include <algorithm>
#include <initializer_list>
#include <map>
#include <set>
#include <utility>
#include <vector>


namespace xml {

typedef int Char;
typedef std::vector<Char> String;

// Character constants.
constexpr char LEFT_ANGLE_BRACKET = '<';
constexpr char RIGHT_ANGLE_BRACKET = '>';
constexpr char RIGHT_SQUARE_BRACKET = ']';
constexpr char TAG_OPEN = LEFT_ANGLE_BRACKET;
constexpr char TAG_CLOSE = RIGHT_ANGLE_BRACKET;
constexpr char SOLIDUS = '/';
constexpr char EQUAL = '=';
constexpr char SINGLE_QUOTE = '\'';
constexpr char DOUBLE_QUOTE = '"';
constexpr char AMPERSAND = '&';


// Whitespace characters as per standard.
static String WHITESPACE {0x20, 0x09, 0x0D, 0x0A};
// Returns true if character is indeed whitespace.
bool is_whitespace(Char);

// Characters which may signal end of tag name.
static String START_EMPTY_TAG_NAME_TERMINATORS = []() {
    String valid_chars(WHITESPACE.begin(), WHITESPACE.end());
    valid_chars.push_back(TAG_CLOSE);
    valid_chars.push_back(SOLIDUS);
    return valid_chars;
}();
// End tag name cannot end with solidus (solidus at back of other vector - exclude it).
static String END_TAG_NAME_TERMINATORS(
    START_EMPTY_TAG_NAME_TERMINATORS.begin(), START_EMPTY_TAG_NAME_TERMINATORS.end() - 1
);

static auto character_ranges_comparator = [](const std::pair<Char, Char>& a, const std::pair<Char, Char>& b) {
    return a.second < b.second;
};
typedef std::set<std::pair<Char, Char>, decltype(character_ranges_comparator)> CharacterRanges;

// Accepted character data as per standard.
static CharacterRanges CHARACTER_RANGES({
    {0x09, 0x0A}, {0x0D, 0x0D}, {0x20, 0xD7FF}, {0xE000, 0xFFFD}, {0x10000, 0x10FFFF}
}, character_ranges_comparator);
bool valid_character(Char);

// Valid (start) name character ranges (all inclusive).
static CharacterRanges NAME_START_CHARACTER_RANGES({
    {':', ':'}, {'A', 'Z'}, {'_', '_'}, {'a', 'z'}, {0xC0, 0xD6}, {0xD8, 0xF6}, {0xF8, 0x2FF},
    {0x370, 0x37D}, {0x37F, 0x1FFF}, {0x200C, 0x200D}, {0x2070, 0x218F}, {0x2C00, 0x2FEF},
    {0x3001, 0xD7F}, {0xF900, 0xFDCF}, {0xFDF0, 0xFFFD}, {0x10000, 0xEFFFF}
}, character_ranges_comparator);
static CharacterRanges NAME_CHARACTER_RANGES = []() {
    CharacterRanges character_ranges = NAME_START_CHARACTER_RANGES;
    CharacterRanges additional_character_ranges ({
        {'-', '.'}, {'0', '9'}, {0xB7, 0xB7}, {0x0300, 0x036F}, {0x203F, 0x2040}
    }, character_ranges_comparator);
    character_ranges.merge(additional_character_ranges);
    return character_ranges;
}();
bool valid_name_start_character(Char);
bool valid_name_character(Char);
bool valid_name(const String&);

// Ensures attribute character is valid.
static String INVALID_ATTRIBUTE_VALUE_CHARACTERS {TAG_OPEN, AMPERSAND};
bool valid_attribute_value_character(Char);

// Tag types: start, end, empty.
enum class TagType {start, end, empty};
// Attributes list - map is suitable and convenient (sorted keys).
typedef std::map<String, String> Attributes;
// Tag class - either start, end or empty tag.
struct Tag {
    String name;
    TagType type;
    Attributes attributes;
};

// Element class - including child elements, text etc.
struct Element {
    String text;
    Tag tag;
    std::vector<Element> children;
};

}