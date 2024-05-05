#include "utils.h"


namespace xml {

bool is_whitespace(Char c) {
    return std::find(WHITESPACE.cbegin(), WHITESPACE.cend(), c) != WHITESPACE.cend();
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
    // todo
    return true;
}

}