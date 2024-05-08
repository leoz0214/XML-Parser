#include "utils.h"
#include <string>
#include <cstring>


namespace xml {

String::String(const char* string) {
    std::size_t pos = 0;
    std::size_t len = std::strlen(string);
    std::size_t bytes_read;
    while (pos != len) {
        this->push_back(parse_utf8(string + pos, pos, len, bytes_read));
        pos += bytes_read;
    }
}

Char parse_utf8(const char* current_char, std::size_t pos, std::size_t len, std::size_t& bytes_read) {
    if ((*current_char & 0b10000000) == 0) {
        bytes_read = 1;
        return *current_char;
    }
    int sig_1s = 1;
    char mask = 0b01000000;
    // Determine if 2-bytes, 3-bytes or 4-bytes based on number of leading 1s.
    while (*current_char & mask) {
        sig_1s++;
        mask >>= 1;
    }
    // Validate indeed 2-4 bytes, and not invalid Unicode byte.
    if (sig_1s < 2 || sig_1s > 4) {
        throw;
    }
    // First byte has (8-n-1) bytes of interest where n is number of leading 1s
    Char char_value = *current_char & (0b11111111 >> sig_1s);
    // Each subsequent byte has 10xxxxxx = 6 bytes of interest.
    for (int offset = 1; offset < sig_1s; ++offset) {
        if (pos + offset == len) {
            throw;
        }
        char byte = *(current_char + offset);
        // Ensure byte starts with 10......
        if ((byte & 0b10000000) == 0 || (byte & 0b01000000) == 1)  {
            throw;
        }
        char_value <<= 6;
        char_value += byte & 0b00111111;
    }
    bytes_read = sig_1s;
    return char_value;
}

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

bool valid_processing_instruction_target(const String& target) {
    // Not empty, not xml (case-insensitive).
    return !target.empty() && (target.size() != 3 || valid_name(target));
}

bool valid_version(const String& version) {
    // Must begin with 1. and subsequent characters are digits.
    return version.size() > 2 && version[0] == '1' && version[1] == '.'
        && std::all_of(version.begin() + 2, version.end(), [](Char c) {
            return std::isdigit(c);
        });
}

bool valid_encoding(const String& encoding) {
    return std::find(
        SUPPORTED_ENCODINGS.cbegin(), SUPPORTED_ENCODINGS.cend(), encoding
    ) != SUPPORTED_ENCODINGS.cend();
}

bool get_standalone_value(const String& string) {
    return STANDALONE_VALUES.at(string);
}

}