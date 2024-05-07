#include "parser.h"


namespace xml {

Parser::Parser(const std::string& data) {
    this->data = &data;
}

Char Parser::get() {
    if (this->eof()) {
        throw;
    }
    return parse_utf8(&this->data->at(pos), pos, this->data->size(), this->increment);
}

Char Parser::peek() {
    std::size_t current_increment = this->increment;
    operator++();
    Char next_char = this->get();
    this->pos -= current_increment;
    this->increment = current_increment;
    return next_char;
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

String Parser::parse_name(const String& until, bool validate) {
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
    if (validate && !valid_name(name)) {
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
                tag.type = TagType::start;
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

void Parser::parse_comment() {
    // This implementation will for now ignore comments.
    Char prev_char = -1;
    while (true) {
        Char c = this->get();
        operator++();
        if (c == HYPHEN && prev_char == HYPHEN) {
            // -- found, needs to be closing part of comment otherwise invalid.
            if (this->get() == RIGHT_ANGLE_BRACKET) {
                operator++();
                return;
            }
            throw;
        }
        if (!valid_character(c)) {
            throw;
        }
        prev_char = c;
    }
}

String Parser::parse_cdata() {
    String cdata;
    Char prev_prev_char = -1;
    Char prev_char = -1;
    while (true) {
        Char c = this->get();
        operator++();
        // Note, CDATA section must end in ]]>
        if (c == RIGHT_ANGLE_BRACKET && prev_char == RIGHT_SQUARE_BRACKET
                && prev_prev_char == RIGHT_SQUARE_BRACKET
        ) {
            // Need to remove ending ]]
            cdata.pop_back();
            cdata.pop_back();
            break;
        }
        if (!valid_character(c)) {
            throw;
        }
        cdata.push_back(c);
        prev_prev_char = prev_char;
        prev_char = c;
    }
    return cdata;
}

ProcessingInstruction Parser::parse_processing_instruction() {
    ProcessingInstruction pi;
    pi.target = parse_name(PROCESSING_INSTRUCTION_TARGET_NAME_TERMINATORS, false);
    if (!valid_processing_instruction_target(pi.target)) {
        throw;
    }
    if (this->get() != QUESTION_MARK) {
        // Increment whitespace.
        operator++();
    }
    Char prev_char = -1;
    while (true) {
        Char c = this->get();
        operator++();
        // Closed with ?>
        if (c == RIGHT_ANGLE_BRACKET && prev_char == QUESTION_MARK) {
            // Need to remove ending ?
            pi.instruction.pop_back();
            break;
        }
        if (!valid_character(c)) {
            throw;
        }
        pi.instruction.push_back(c);
        prev_char = c;
    }
    return pi;
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
                switch (this->peek()) {
                    case EXCLAMATION_MARK:
                        // Comment or CDATA section.
                        operator++();
                        operator++();
                        switch (this->get()) {
                            case HYPHEN:
                                // Further narrowed to comment.
                                operator++();
                                if (this->get() != HYPHEN) {
                                    // Not starting with <!--
                                    throw;
                                }
                                operator++();
                                this->parse_comment();
                                break;
                            case LEFT_SQUARE_BRACKET: {
                                // Further narrowed to CDATA.
                                operator++();
                                String required_chars("CDATA[");
                                for (Char required : required_chars) {
                                    if (this->get() != required) {
                                        throw;
                                    }
                                    operator++();
                                }
                                String cdata = this->parse_cdata();
                                element.text.reserve(element.text.size() + cdata.size());
                                element.text.insert(element.text.end(), cdata.begin(), cdata.end());
                                break;
                            }
                            default:
                                throw;
                        }
                        break;
                    case QUESTION_MARK:
                        // Processing instruction.
                        operator++();
                        operator++();
                        element.processing_instructions.push_back(this->parse_processing_instruction());
                        break;
                    default:
                        // Must be child element or erroneous.
                        Element child = this->parse_element(true);
                        if (child.tag.type == TagType::end) {
                            // If closing tag same name, then element successfully closed.
                            if (child.tag.name != tag.name) {
                                throw;
                            }
                            goto done;
                        }
                        element.children.push_back(child);
                }
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
