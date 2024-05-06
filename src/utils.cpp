#include "utils.h"
#include <string>


namespace xml {

bool is_whitespace(Char c) {
    return std::find(WHITESPACE.cbegin(), WHITESPACE.cend(), c) != WHITESPACE.cend();
}

bool valid_character(Char c) {
    auto possible_range_it = CHARACTER_RANGES.lower_bound({c, c});
    return possible_range_it != CHARACTER_RANGES.end()
        && c >= possible_range_it->first && c <= possible_range_it->second;
}

bool valid_name_start_character(Char c) {
    auto possible_range_it = NAME_START_CHARACTER_RANGES.lower_bound({c, c});
    return possible_range_it != NAME_START_CHARACTER_RANGES.end()
        && c >= possible_range_it->first && c <= possible_range_it->second;
}

bool valid_name_character(Char c) {
    auto possible_range_it = NAME_CHARACTER_RANGES.lower_bound({c, c});
    return possible_range_it != NAME_CHARACTER_RANGES.end()
        && c >= possible_range_it->first && c <= possible_range_it->second;
}

bool valid_name(const String& name) {
    if (name.empty()) {
        return false;
    }
    if (name.size() < 3) {
        return true;
    }
    // Names starting with xml (case-insensitive) are reserved. Disallow.
    return !(std::tolower(name[0]) == 'x'
            && std::tolower(name[1]) == 'm' && std::tolower(name[2]) == 'l');
}

bool valid_attribute_value_character(Char c) {
    return valid_character(c) && std::find(
        INVALID_ATTRIBUTE_VALUE_CHARACTERS.cbegin(),
        INVALID_ATTRIBUTE_VALUE_CHARACTERS.cend(), c
    ) == INVALID_ATTRIBUTE_VALUE_CHARACTERS.cend();
}

}