#include "utils.h"
#include <string>
#include <cstring>
#include <sstream>


namespace xml {

String::String(const char* string) {
    std::string str(string, string + std::strlen(string));
    StringBuffer string_buffer(str);
    std::istream istream(&string_buffer);
    while (istream.peek() != EOF) {
        this->push_back(parse_utf8(istream));
    }
}

String::String(const std::string& string) : String::String(string.c_str()) {}

std::ostream& operator<<(std::ostream& output, const String& string) {
    return output << std::string(string);
}

void fill_utf8_byte(char& byte, Char& value_left, int bits) {
    for (int n = 0; n < bits; ++n) {
        if (value_left & 1) {
            byte |= (1 << n);
        }   
        value_left >>= 1;
    }
}

String::operator std::string() const {
    std::string string;
    string.reserve(this->size() * 4);
    // Converts each Unicode value to sequence of correct Unicode bytes.
    for (Char c : *this) {
        if (c < 0) {
            // Invalid character - cannot be negative.
            throw XmlError("Invalid character in String");
        }
        if (c <= UTF8_BYTE_LIMITS[0]) {
            // Just ASCII - nothing special - can add character directly.
            string.push_back(c);
            continue;
        } else if (c >= UTF8_BYTE_LIMITS[3]) {
            // Too large - invalid.
            throw XmlError("Invalid character in String");
        }
        // Deduce number of bytes required based on numeric value.
        int bytes = c <= UTF8_BYTE_LIMITS[1] ? 2 : c <= UTF8_BYTE_LIMITS[2] ? 3 : 4;
        // First character byte depends on number of bytes to represent character.
        char byte1 = bytes == 2 ? 0b11000000 : bytes == 3 ? 0b11100000 : 0b11110000;
        // All additional bytes are in the form 10xxxxxx
        std::vector<char> extra_bytes(bytes - 1, 0b10000000);
        for (int i = bytes - 2; i >= 0; --i) {
            fill_utf8_byte(extra_bytes[i], c, 6);
        }
        fill_utf8_byte(byte1, c, 7 - bytes);
        string.push_back(byte1);
        string.insert(string.end(), extra_bytes.begin(), extra_bytes.end());
    }
    string.shrink_to_fit();
    return string;
}

Char parse_utf8(std::istream& istream) {
    char current_char = istream.get();
    if ((current_char & 0b10000000) == 0) {
        return current_char;
    }
    int sig_1s = 1;
    char mask = 0b01000000;
    // Determine if 2-bytes, 3-bytes or 4-bytes based on number of leading 1s.
    while (current_char & mask) {
        sig_1s++;
        mask >>= 1;
    }
    // Validate indeed 2-4 bytes, and not invalid Unicode byte.
    if (sig_1s < 2 || sig_1s > 4) {
        throw XmlError("Invalid UTF-8 byte");
    }
    // First byte has (8-n-1) bytes of interest where n is number of leading 1s
    Char char_value = current_char & (0b11111111 >> sig_1s);
    // Each subsequent byte has 10xxxxxx = 6 bytes of interest.
    for (int offset = 1; offset < sig_1s; ++offset) {
        char byte = istream.get();
        if (byte == EOF) {
            throw XmlError("Incomplete UTF-8 character");
        }
        // Ensure byte starts with 10......
        if ((byte & 0b10000000) == 0 || (byte & 0b01000000) == 1)  {
            throw XmlError("Invalid UTF-8 byte");
        }
        char_value <<= 6;
        char_value += byte & 0b00111111;
    }
    return char_value;
}

StringBuffer::StringBuffer(const std::string& string) {
    this->setg(const_cast<char*>(string.data()), const_cast<char*>(string.data()),
        const_cast<char*>(string.data()) + string.size());
}

GeneralEntity::GeneralEntity(const String& value) {
    this->value = value;
}

bool is_whitespace(Char c) {
    return std::find(WHITESPACE.cbegin(), WHITESPACE.cend(), c) != WHITESPACE.cend();
}

bool in_any_character_range(Char c, const CharacterRanges& character_ranges) {
    // Identifies a possible range the character can be in,
    // and confirms it is indeed in range. If not within any range, this will be deduced.
    auto possible_range_it = character_ranges.lower_bound({c, c});
    return possible_range_it != character_ranges.cend()
        && c >= possible_range_it->first && c <= possible_range_it->second;
}

bool valid_character(Char c) {
    return in_any_character_range(c, CHARACTER_RANGES);
}

bool valid_name_start_character(Char c) {
    return in_any_character_range(c, NAME_START_CHARACTER_RANGES);
}

bool valid_name_character(Char c) {
    return in_any_character_range(c, NAME_CHARACTER_RANGES);
}

bool valid_name(const String& name, bool check_all_chars) {
    if (name.empty()) {
        return false;
    }
    if (check_all_chars) {
        // Only when not checking on-the-go should validating all characters occur.
        // Such as an attribute value needing to be a valid Name.
        if (!valid_name_start_character(name.front())) {
            return false;
        }
        if (!std::all_of(name.cbegin() + 1, name.cend(), valid_name_character)) {
            return false;
        }
    }
    if (name.size() < 3) {
        return true;
    }
    // Names starting with xml (case-insensitive) are reserved. Disallow.
    return !(std::tolower(name[0]) == 'x'
            && std::tolower(name[1]) == 'm' && std::tolower(name[2]) == 'l');
}

bool valid_names_or_nmtokens(const String& string, bool is_nmtokens) {
    if (string.empty()) {
        return false;
    }
    // Names/nmtokens separated by a space without any leading/trailing spaces.
    String value;
    for (Char c : string) {
        if (c == SPACE) {
            if ((!is_nmtokens && !valid_name(value, true)) || (is_nmtokens && !valid_nmtoken(value))) {
                return false;
            }
            value.clear();
        } else {
            value.push_back(c);
        }
    }
    // Must not end on whitespace.
    return !value.empty();
}

bool valid_names(const String& names) {
    return valid_names_or_nmtokens(names, false);
}

bool valid_nmtoken(const String& nmtoken) {
    return !nmtoken.empty() && std::all_of(nmtoken.cbegin(), nmtoken.cend(), valid_name_character);
}

bool valid_nmtokens(const String& nmtokens) {
    return valid_names_or_nmtokens(nmtokens, true);
}

bool valid_attribute_value_character(Char c) {
    return valid_character(c) && std::find(
        INVALID_ATTRIBUTE_VALUE_CHARACTERS.cbegin(),
        INVALID_ATTRIBUTE_VALUE_CHARACTERS.cend(), c
    ) == INVALID_ATTRIBUTE_VALUE_CHARACTERS.cend();
}

bool valid_processing_instruction_target(const String& target) {
    // Not empty, not beginning with xml (case-insensitive).
    return valid_name(target);
}

bool valid_version(const String& version) {
    // Must begin with 1. and subsequent characters are digits.
    return version.size() > 2 && version[0] == '1' && version[1] == '.'
        && std::all_of(version.cbegin() + 2, version.cend(), [](Char c) {
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

ExternalIDType get_external_id_type(const String& string) {
    return EXTERNAL_ID_TYPES.at(string);
}

bool valid_public_id_character(Char c) {
    auto possible_range_it = PUBLIC_ID_CHARACTER_RANGES.lower_bound({c, c});
    if (
        possible_range_it != PUBLIC_ID_CHARACTER_RANGES.end()
        && c >= possible_range_it->first && c <= possible_range_it->second
    ) {
        return true;
    }
    return std::find(
        PUBLIC_ID_CHARACTERS.cbegin(), PUBLIC_ID_CHARACTERS.cend(), c
    ) != PUBLIC_ID_CHARACTERS.cend();
}

AttributeType get_attribute_type(const String& string) {
    return ATTRIBUTE_TYPES.at(string);
}

AttributePresence get_attribute_presence(const String& string) {
    return ATTRIBUTE_PRESENCES.at(string);
}

Char parse_character_reference(const String& string) {
    Char c = string.front();
    std::size_t i = 0;
    bool is_hex = c == 'x';
    if (is_hex) {
        c = string[++i];
    }
    if (c == SEMI_COLON) {
        // Cannot be empty.
        throw XmlError("Character reference must contain at least one digit");
    }
    Char char_value = 0;
    while (true) {
        Char c = std::tolower(string[i++]);
        if (c == SEMI_COLON) {
            break;
        }
        if (is_hex) {
            if (!std::isxdigit(c)) {
                throw XmlError("Invalid hexadecimal (0-F) digit");
            }
            // Converts letter to hex value if needed.
            int digit_value = c >= 'a' ? c - 'a' + 10 : c - '0';
            char_value = char_value * 16 + digit_value;
        } else {
            if (!std::isdigit(c)) {
                throw XmlError("Invalid denary (0-9) digit");
            }
            char_value = char_value * 10 + (c - '0');
        }
        // Sanity range check, avoiding overflow.
        if (char_value > UTF8_BYTE_LIMITS[3]) {
            throw XmlError("Invalid character");
        }
    }
    if (!valid_character(char_value)) {
        throw XmlError("Invalid character");
    }
    return char_value;
}

String expand_character_references(const String& string) {
    String result;
    for (std::size_t i = 0; i < string.size(); ++i) {
        Char c = string.at(i);
        if (c == AMPERSAND && string.at(i+1) == OCTOTHORPE) {
            String char_ref;
            i += 2;
            while (true) {
                c = string.at(i++);
                char_ref.push_back(c);
                if (c == SEMI_COLON) {
                    break;
                }
            }
            result.push_back(parse_character_reference(char_ref));
        } else {
            result.push_back(c);
        }
    }
    return result;
}

bool is_url_resource(const std::string& file_path) {
    String url;
    for (char c : file_path) {
        url.push_back(std::tolower(c));
        if (RECOGNISED_PROTOCOLS.count(url)) {
            return true;
        }
    }
    return false;
}

}