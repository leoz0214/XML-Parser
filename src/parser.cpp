#include "parser.h"
#include <iostream>


namespace xml {

Parser::Parser(const std::string& data) {
    this->data = &data;
}

Char Parser::get() {
    if (this->eof()) {
        throw;
    }
    char current_char = this->data->at(pos);
    if ((current_char & 0b10000000) == 0) {
        // Just ASCII
        this->increment = 1;
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
        throw;
    }
    // First byte has (8-n-1) bytes of interest where n is number of leading 1s
    Char char_value = current_char & (0b11111111 >> sig_1s);
    // Each subsequent byte has 10xxxxxx = 6 bytes of interest.
    for (int offset = 1; offset < sig_1s; ++offset) {
        char byte = this->data->at(pos + offset);
        // Ensure byte starts with 10......
        if ((byte & 0b10000000) == 0 || (byte & 0b01000000) == 1)  {
            throw;
        }
        char_value <<= 6;
        char_value += byte & 0b00111111;
    }
    this->increment = sig_1s;
    return char_value;
}

void Parser::operator++() {
    if (!this->increment) {
        this->get();
    }
    this->pos += this->increment;
}

bool Parser::eof() {
    return this->pos == this->data->size();
}

String Parser::parse_name(const String& until) {
    String name;
    while (true) {
        Char c = this->get();
        if (std::find(until.cbegin(), until.cend(), c) != until.cend()) {
            break;
        }
        if (name.empty() && !valid_name_start_character(c)) {
            throw;
        }
        if (!name.empty() && !valid_name_character(c)) {
            throw;
        }
        name.push_back(c);
        operator++();
    }
    if (!valid_name(name)) {
        throw;
    }
    return name;
}

std::pair<String, String> Parser::parse_attribute() {
    String name = this->parse_name({EQUAL});
    // Increment the '='
    operator++();
    // Ensure opening quote (double or single accepted).
    Char quote = this->get();
    if (quote != SINGLE_QUOTE && quote != DOUBLE_QUOTE) {
        throw;
    }
    operator++();
    String value;
    while (true) {
        Char c = this->get();
        operator++();
        if (c == quote) {
            break;
        }
        if (!valid_attribute_value_character(c)) {
            throw;
        }
        value.push_back(c);
    }
    return {name, value};
}

Tag Parser::parse_tag() {
    // Increment the '<'
    operator++();
    Tag tag;
    if (this->get() == SOLIDUS) {
        // End tag.
        operator++();
        tag.name = this->parse_name(END_TAG_NAME_TERMINATORS);
        tag.type = TagType::end;
        while (true) {
            Char c = this->get();
            operator++();
            if (c == TAG_CLOSE) {
                break;
            } if (!is_whitespace(c)) {
                throw;
            }
        }
    } else {
        // Start/empty tag.
        tag.name = this->parse_name(START_EMPTY_TAG_NAME_TERMINATORS);
        while (true) {
            Char c = this->get();
            if (c == TAG_CLOSE) {
                operator++();
                tag.type == TagType::start;
                break;
            }
            if (c == SOLIDUS) {
                // Empty tag: must end in /> strictly.
                tag.type = TagType::empty;
                operator++();
                c = this->get();
                operator++();
                if (c != TAG_CLOSE) {
                    throw;
                }
                break;
            }
            if (is_whitespace(c)) {
                operator++();
                continue;
            }
            // Not end or whitespace, so must be an attribute.
            std::pair<String, String> attribute = this->parse_attribute();
            if (tag.attributes.count(attribute.first)) {
                // Duplicate attribute names in same element prohibited.
                throw;
            }
            tag.attributes.insert(attribute);
        }
    }
    return tag;
}

Element Parser::parse_element(bool allow_end) {
    Tag tag = this->parse_tag();
    Element element;
    element.tag = tag;
    switch (tag.type) {
        case TagType::start:
            break;
        case TagType::end:
            if (allow_end) {
                // Only as the end tag of a parent element.
                return element;
            }
            // Cannot have end tag at this time.
            throw;
        case TagType::empty:
            return element;
    }
    // Process normal element after start tag seen.
    String char_data;
    while (true) {
        Char c = this->get();
        switch (c) {
            case AMPERSAND:
                throw;
            case LEFT_ANGLE_BRACKET: {
                // Flush current character data to overall text.
                element.text.reserve(element.text.size() + char_data.size());
                element.text.insert(element.text.end(), char_data.begin(), char_data.end());
                char_data.clear();
                Element child = this->parse_element(true);
                if (child.tag.type == TagType::end) {
                    // If closing tag same name, then element successfully closed.
                    if (child.tag.name != tag.name) {
                        throw;
                    }
                    goto done;
                }
                element.children.push_back(child);
                break;
            }
            case RIGHT_ANGLE_BRACKET:
                // Check ']]>' not formed (strange standard requirement).
                if (
                    char_data.size() >= 2
                    && *(char_data.end() - 1) == RIGHT_SQUARE_BRACKET
                    && *(char_data.end() - 2) == RIGHT_SQUARE_BRACKET
                ) {
                    throw;
                }
                char_data.push_back(c);
                operator++();
                break;
            default:
                // Any other character continues on the character data if valid.
                if (!valid_character(c)) {
                    throw;
                }
                operator++();
                char_data.push_back(c);
        }
    }
    done:;
    return element;
}

}
