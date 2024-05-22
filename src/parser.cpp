#include "parser.h"
#include <fstream>


namespace xml {

EntityStream::EntityStream(const String& text) {
    this->text = text;
    this->is_external = false;
}

EntityStream::EntityStream(const String& file_path, bool) {
    // TO IMPROVE - PROPER String -> std::string conversion.
    std::string file_path_literal(file_path.begin(), file_path.end());
    std::istream* stream_ptr = new std::ifstream(file_path_literal);
    this->stream = unique_istream_ptr(stream_ptr);
    Parser* parser_ptr = new Parser(*this->stream);
    this->parser = std::unique_ptr<Parser>(parser_ptr);
    this->is_external = true;
}

Char EntityStream::get() {
    return this->is_external ? this->parser->get() : this->text.at(this->pos);
}

void EntityStream::operator++() {
    this->pos++;
    if (is_external) {
        this->parser->operator++();
    }
}

bool EntityStream::eof() {
    return this->is_external ? this->parser->eof() : this->pos >= this->text.size();
}


Parser::Parser(const std::string& string) {
    this->string_buffer = std::make_unique<StringBuffer>(StringBuffer(string));
    std::istream* istream_ptr = new std::istream(this->string_buffer.get());
    this->stream = std::unique_ptr<std::istream>(istream_ptr);
}

Parser::Parser(std::istream& istream) {
    this->stream = &istream;
}

Char Parser::get() {
    if (this->previous_char != -1) {
        return this->previous_char;
    }
    this->just_parsed_character_reference = false;
    bool just_parsed_carriage_return = this->just_parsed_carriage_return;
    this->just_parsed_carriage_return = false;
    if (this->general_entity_active) {
        return this->general_entity_stack.top().get();
    }
    if (this->parameter_entity_active) {
        return this->parameter_entity_stack.top().get();
    }
    if (this->eof()) {
        throw;
    }
    Char c;
    if (std::holds_alternative<unique_istream_ptr>(this->stream)) {
        c = parse_utf8(*std::get<unique_istream_ptr>(this->stream));
    } else {
        c = parse_utf8(*std::get<std::istream*>(this->stream));
    }
    if (c == LINE_FEED && just_parsed_carriage_return) {
        operator++();
        return this->get();
    }
    this->just_parsed_carriage_return = c == CARRIAGE_RETURN;
    if (this->just_parsed_carriage_return) {
        return LINE_FEED;
    }
    previous_char = c;
    return c;
}

Char Parser::get(const GeneralEntities& general_entities) {
    Char c = this->get();
    if (c == AMPERSAND && !this->just_parsed_character_reference) {
        operator++();
        if (this->get() == OCTOTHORPE) {
            operator++();
            return this->parse_character_entity();
        }
        this->parse_general_entity(general_entities);
        return this->general_entity_stack.top().get();
    }
    return c;
}

Char Parser::get(const ParameterEntities& parameter_entities) {
    Char c = this->get();
    if (c == PERCENT_SIGN && !this->just_parsed_character_reference) {
        operator++();
        this->parse_parameter_entity(parameter_entities);
        return this->parameter_entity_stack.top().get();
    }
    return c;
}

void Parser::operator++() {
    if (this->general_entity_active) {
        ++this->general_entity_stack.top();
        if (this->general_entity_stack.top().eof() && this->general_entity_stack.size() > 1) {
            this->general_entity_stack.pop();
        }
        return;
    }
    if (this->parameter_entity_active) {
        ++this->parameter_entity_stack.top();
        if (this->parameter_entity_stack.top().eof() && this->parameter_entity_stack.size() > 1) {
            this->parameter_entity_stack.pop();
        }
        return;
    }
    if (!this->previous_char) {
        operator++();
    }
    this->previous_char = -1;
}

bool Parser::eof() {
    if (std::holds_alternative<unique_istream_ptr>(this->stream)) {
        return std::get<unique_istream_ptr>(this->stream)->peek() == EOF;
    }
    return std::get<std::istream*>(this->stream)->peek() == EOF;
}

bool Parser::general_entity_eof() {
    return this->general_entity_active && this->general_entity_stack.size() == 1
        && this->general_entity_stack.top().eof();
}

bool Parser::parameter_entity_eof() {
    return this->parameter_entity_active && this->parameter_entity_stack.size() == 1
        && this->parameter_entity_stack.top().eof();
}

void Parser::ignore_whitespace() {
    while (is_whitespace(this->get())) {
        operator++();
    }
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

String Parser::parse_nmtoken(const String& until) {
    String nmtoken;
    while (true) {
        Char c = this->get();
        if (std::find(until.cbegin(), until.cend(), c) != until.cend()) {
            break;
        }
        nmtoken.push_back(c);
        operator++();
    }
    return nmtoken;
}

String Parser::parse_attribute_value(const DoctypeDeclaration& dtd, bool references_active) {
    // Ensure opening quote (double or single accepted).
    Char quote = this->get();
    if (quote != SINGLE_QUOTE && quote != DOUBLE_QUOTE) {
        throw;
    }
    operator++();
    String value;
    int general_entity_stack_size_before = this->general_entity_stack.size();
    while (true) {
        Char c = this->get(dtd.general_entities);
        if (this->general_entity_stack.size() > general_entity_stack_size_before) {
            if (!references_active) {
                throw;
            }
            if (this->general_entity_stack.top().is_external) {
                // No external general entities in attribute values.
                throw;
            }
            // Parse all general entity text and normalise whitespace.
            this->parse_general_entity_text(dtd.general_entities, [&value, this](Char c) {
                // Included in literal - ignore quotes but disallow < still for example.
                if (!this->just_parsed_character_reference && !valid_attribute_value_character(c)) {
                    throw;
                }
                if (is_whitespace(c)) {
                    value.push_back(SPACE);
                } else {
                    value.push_back(c);
                }             
            }, general_entity_stack_size_before);
            continue;
        }
        if (!this->just_parsed_character_reference) {
            operator++();
            if (c == quote) {
                break;
            }
            if (!valid_attribute_value_character(c)) {
                throw;
            }
        } else if (!references_active) {
            throw;
        }
        if (is_whitespace(c) && !this->just_parsed_character_reference) {
            // All literal whitespace that is not char reference becomes space.
            value.push_back(SPACE);
        } else {
            value.push_back(c);
        }
    }
    return value;
}

String Parser::parse_entity_value(const DoctypeDeclaration& dtd) {
    Char quote = this->get();
    if (quote != SINGLE_QUOTE && quote != DOUBLE_QUOTE) {
        throw;
    }
    operator++();
    String value;
    while (true) {
        int before_parameter_entity_stack_size = this->parameter_entity_stack.size();
        Char c = this->get(dtd.parameter_entities);
        int after_parameter_entity_stack_size = this->parameter_entity_stack.size();
        if (after_parameter_entity_stack_size > before_parameter_entity_stack_size) {
            // Parse all parameter entity text.
            while (true) {
                value.push_back(c);
                operator++();
                if (before_parameter_entity_stack_size == 0) {
                    if (this->parameter_entity_eof()) {
                        this->end_parameter_entity();
                        break;
                    }
                } else {
                    if (this->parameter_entity_stack.size() == before_parameter_entity_stack_size) {
                        break;
                    }
                }
                c = this->get();
            }
            continue;
        }
        operator++();
        if (c == AMPERSAND && !this->just_parsed_character_reference) {
            // General/character entity.
            if (this->get() == OCTOTHORPE) {
                // Character entity: &#...
                operator++();
                value.push_back(this->parse_character_entity());
            } else {
                // General entity (bypass - store actual entity ref
                // in the value of current entity - not replacement text).
                value.push_back(AMPERSAND);
                for (Char c : this->parse_general_entity_name(dtd.general_entities)) {
                    value.push_back(c);
                }
                value.push_back(SEMI_COLON);
            }
        } else {
            if (c == quote) {
                break;
            }
            value.push_back(c);
        }
    }
    return value;
}

Char Parser::parse_character_entity() {
    String string;
    while (true) {
        Char c = this->get();
        operator++();
        string.push_back(c);
        if (c == SEMI_COLON) {
            break;
        }
    }
    this->just_parsed_character_reference = true;
    return xml::parse_character_entity(string);
}

String Parser::parse_general_entity_name(const GeneralEntities& general_entities) {
    String name = "";
    while (true) {
        Char c = this->get();
        operator++();
        if (c == SEMI_COLON) {
            break;
        }
        name.push_back(c);
    }
    if (!general_entities.count(name)) {
        // General entity name is not declared.
        throw;
    }
    return name;
}

void Parser::parse_general_entity(const GeneralEntities& general_entities) {
    String name = this->parse_general_entity_name(general_entities);
    if (!general_entities.count(name)) {
        throw;
    }
    if (general_entities.at(name).is_unparsed) {
        throw;
    }
    if (general_entities.at(name).is_external) {
        this->general_entity_stack.push(
            {general_entities.at(name).external_id.system_id, true});
    } else {
        this->general_entity_stack.push({general_entities.at(name).value});
    }
    this->general_entity_active = true;
}

String Parser::parse_general_entity_text(
    const GeneralEntities& general_entities, std::function<void(Char)> func,
    int original_depth
) {
    String text;
    while (true) {
        // Ampersand means recursive entity reference.
        // Only if not ampersand should character be added to value.
        Char c = this->get(general_entities);
        if (c != AMPERSAND || this->just_parsed_character_reference) {
            func(c);
            if (!this->just_parsed_character_reference) {
                operator++();
            }
        }
        if (
            this->general_entity_stack.size() == original_depth + 1
            && this->general_entity_stack.top().eof()
        ) {
            if (this->general_entity_eof()) {
                this->end_general_entity();
            }
            break;
        }
    }
    return text;
}

void Parser::end_general_entity() {
    this->general_entity_stack = {};
    this->general_entity_active = false;
}

void Parser::parse_parameter_entity(const ParameterEntities& parameter_entities) {
    String name = "";
    while (true) {
        Char c = this->get();
        operator++();
        if (c == SEMI_COLON) {
            break;
        }
        name.push_back(c);
    }
    if (!parameter_entities.count(name)) {
        // Parameter entity name is not declared.
        throw;
    }
    this->parameter_entity_stack.push({parameter_entities.at(name).value});
    this->parameter_entity_active = true;
}

void Parser::end_parameter_entity() {
    this->parameter_entity_stack = {};
    this->parameter_entity_active = false;
}

std::pair<String, String> Parser::parse_attribute(const DoctypeDeclaration& dtd, bool references_active) {
    String name = this->parse_name(ATTRIBUTE_NAME_TERMINATORS);
    // Ignore whitespace until '=' is reached.
    while (this->get() != EQUAL) {
        operator++();
    }
    // Increment the '='
    operator++();
    this->ignore_whitespace();
    String value = this->parse_attribute_value(dtd, references_active);
    return {name, value};
}

Tag Parser::parse_tag(const DoctypeDeclaration& dtd) {
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
            std::pair<String, String> attribute = this->parse_attribute(dtd);
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

ProcessingInstruction Parser::parse_processing_instruction(bool detect_xml_declaration) {
    ProcessingInstruction pi;
    pi.target = parse_name(PROCESSING_INSTRUCTION_TARGET_NAME_TERMINATORS, false);
    if (!valid_processing_instruction_target(pi.target)) {
        // Special case - name is exactly 'xml' and xml declaration currently being searched for.
        // In that case, processing instruction returned and further specialist processing
        // will take place for handling the XML declaration in particular.
        if (detect_xml_declaration && pi.target == String("xml")) {
            return pi;
        }
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

Element Parser::parse_element(const DoctypeDeclaration& dtd, bool allow_end) {
    Tag tag = this->parse_tag(dtd);
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
    int general_entity_stack_size_before = this->general_entity_stack.size();
    while (true) {
        if (this->general_entity_eof()) {
            this->end_general_entity();
        }
        Char c = this->get(dtd.general_entities);
        if (this->general_entity_active) {
            while (c == AMPERSAND && !this->just_parsed_character_reference) {
                c = this->get(dtd.general_entities);
            }
        }
        if (this->just_parsed_character_reference) {
            // Escaped character - part of character data.
            element.text.reserve(element.text.size() + char_data.size());
            element.text.insert(element.text.end(), char_data.begin(), char_data.end());
            element.text.push_back(c);
            char_data.clear();
            continue;
        }
        switch (c) {
            case LEFT_ANGLE_BRACKET: {
                // Flush current character data to overall text.
                element.text.reserve(element.text.size() + char_data.size());
                element.text.insert(element.text.end(), char_data.begin(), char_data.end());
                char_data.clear();
                operator++();
                switch (this->get()) {
                    case EXCLAMATION_MARK:
                        // Comment or CDATA section.
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
                        element.processing_instructions.push_back(this->parse_processing_instruction());
                        break;
                    default:
                        // Must be child element or erroneous.
                        Element child = this->parse_element(dtd, true);
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
    // General entities not properly closed.
    if (this->general_entity_stack.size() != general_entity_stack_size_before) {
        throw;
    }
    return element;
}

Element Parser::parse_element() {
    return this->parse_element({});
}

void Parser::parse_xml_declaration(Document& document) {
    // Version must be specified.
    bool version_info_parsed = false;
    // Tracks which attributes can still be seen.
    bool version_info_possible = true;
    bool encoding_declaration_possible = true;
    bool standalone_declaration_possible = true;
    while (true) {
        Char c = this->get();
        if (is_whitespace(c)) {
            operator++();
            continue;
        }
        if (c == QUESTION_MARK) {
            operator++();
            if (this->get() != RIGHT_ANGLE_BRACKET) {
                throw;
            }
            operator++();
            break;
        }
        std::pair<String, String> attribute = this->parse_attribute(document.doctype_declaration, false);
        if (attribute.first == XML_DECLARATION_VERSION_NAME) {
            String& version = attribute.second;
            if (!version_info_possible || !valid_version(version)) {
                throw;
            }
            version_info_parsed = true;
            version_info_possible = false;
            document.version = version;
        } else if (attribute.first == XML_DECLARATION_ENCODING_NAME) {
            String& encoding = attribute.second;
            if (!encoding_declaration_possible) {
                throw;
            }
            // case-insensitive
            std::transform(encoding.begin(), encoding.end(), encoding.begin(), [](Char c) {
                return std::tolower(c);
            });
            if (!valid_encoding(encoding)) {
                throw;
            }
            version_info_possible = false;
            encoding_declaration_possible = false;
            document.encoding = encoding;
        } else if (attribute.first == XML_DECLARATION_STANDALONE_NAME) {
            if (!standalone_declaration_possible) {
                throw;
            }
            version_info_possible = false;
            encoding_declaration_possible = false;
            standalone_declaration_possible = false;
            document.standalone = get_standalone_value(attribute.second);
        } else {
            // Unknown XML declaration attribute.
            throw;
        }
    }
    if (!version_info_parsed) {
        throw;
    }
}

String Parser::parse_public_id() {
    Char quote = this->get();
    operator++();
    if (quote != SINGLE_QUOTE && quote != DOUBLE_QUOTE) {
        throw;
    }
    String public_id;
    while (true) {
        Char c = this->get();
        operator++();
        if (c == quote) {
            break;
        }
        if (!valid_public_id_character(c)) {
            throw;
        }
        public_id.push_back(c);
    }
    return public_id;
}

String Parser::parse_system_id() {
    Char quote = this->get();
    operator++();
    if (quote != SINGLE_QUOTE && quote != DOUBLE_QUOTE) {
        throw;
    }
    String system_id;
    while (true) {
        Char c = this->get();
        operator++();
        if (c == quote) {
            break;
        }
        if (!valid_character(c)) {
            throw;
        }
        system_id.push_back(c);
    }
    return system_id;
}

ExternalID Parser::parse_external_id() {
    ExternalID external_id;
    // Looking for PUBLIC / SYSTEM - parsing like a name is just fine.
    String type_string = this->parse_name(WHITESPACE, false);
    external_id.type = get_external_id_type(type_string);
    this->ignore_whitespace();
    if (external_id.type == ExternalIDType::public_) {
        external_id.public_id = this->parse_public_id();
        if (!is_whitespace(this->get())) {
            // Min 1 whitespace between public/system ID.
            throw;
        }
        this->ignore_whitespace();
    }
    external_id.system_id = this->parse_system_id();
    return external_id;
}

void Parser::parse_internal_dtd_subset(DoctypeDeclaration& dtd) {
    while (true) {
        if (this->parameter_entity_eof()) {
            this->end_parameter_entity();
        }
        Char c = this->get(dtd.parameter_entities);
        while (c == PERCENT_SIGN) {
            c = this->get(dtd.parameter_entities);
        }
        operator++();
        if (c == RIGHT_SQUARE_BRACKET) {
            if (this->parameter_entity_active) {
                throw;
            }
            return;
        }
        if (is_whitespace(c)) {
            continue;
        } else if (c == LEFT_ANGLE_BRACKET) {
            // Must be exclamation mark or processing instruction or invalid.
            c = this->get();
            if (c != EXCLAMATION_MARK) {
                if (c == QUESTION_MARK && !this->parameter_entity_active) {
                    operator++();
                    dtd.processing_instructions.push_back(this->parse_processing_instruction());
                    continue;
                }
                // Not processing instruction, or parameter entity active.
                throw;
            }
            operator++();
            if (this->get() == HYPHEN) {
                if (this->parameter_entity_active) {
                    // Comment cannot be in parameter entity text.
                    throw;
                }
                // Comment or invalid.
                operator++();
                if (this->get() != HYPHEN) {
                    throw;
                }
                operator++();
                this->parse_comment();
            } else {
                // Markup declaration or invalid.
                this->parse_markup_declaration(dtd);
            }
        } else {
            throw;
        }
    }
}

void Parser::parse_markup_declaration(DoctypeDeclaration& dtd) {
    String markup_declaration_type = this->parse_name(WHITESPACE, false);
    if (markup_declaration_type == String("ELEMENT")) {
        this->parse_element_declaration(dtd);
    } else if (markup_declaration_type == String("ATTLIST")) {
        this->parse_attribute_list_declaration(dtd);
    } else if (markup_declaration_type == String("ENTITY")) {
        this->parse_entity_declaration(dtd);
    } else if (markup_declaration_type == String("NOTATION")) {
       this->parse_notation_declaration(dtd);
    } else {
        // Unknown markup declaration type.
        throw;
    }
}

void Parser::parse_element_declaration(DoctypeDeclaration& dtd) {
    this->ignore_whitespace();
    ElementDeclaration element_declaration;
    element_declaration.name = this->parse_name(WHITESPACE);
    // Duplicate declarations for same element name illegal.
    if (dtd.element_declarations.count(element_declaration.name)) {
        throw;
    }
    this->ignore_whitespace();
    // Empty, any, children, mixed OR invalid.
    if (this->get() == LEFT_PARENTHESIS) {
        // Mixed/element content.
        operator++();
        this->ignore_whitespace();
        if (this->get() == OCTOTHORPE) {
            // Indicates mixed content or invalid.
            element_declaration.type = ElementType::mixed;
            element_declaration.mixed_content = this->parse_mixed_content_model();
        } else {
            // Indicates element content or invalid.
            element_declaration.type = ElementType::children;
            element_declaration.element_content = this->parse_element_content_model();
        }
    } else {
        // Must be EMPTY or ANY otherwise.
        String element_type = this->parse_name(WHITESPACE_AND_RIGHT_ANGLE_BRACKET, false);
        if (element_type == String("EMPTY")) {
            element_declaration.type = ElementType::empty;
        } else if (element_type == String("ANY")) {
            element_declaration.type = ElementType::any;
        } else {
            throw;
        }
    }
    this->ignore_whitespace();
    if (this->get() != RIGHT_ANGLE_BRACKET) {
        throw;
    }
    operator++();
    dtd.element_declarations[element_declaration.name] = element_declaration;
}

ElementContentModel Parser::parse_element_content_model() {
    ElementContentModel ecm;
    bool separator_seen = false;
    bool separator_next = false;
    while (true) {
        Char c = this->get();
        if (is_whitespace(c)) {
            operator++();
            continue;
        }
        if (c == RIGHT_PARENTHESIS) {
            operator++();
            // Cannot end on separator - ensure this is not case (also cannot be empty).
            if (!separator_next) {
                throw;
            }
            if (ELEMENT_CONTENT_COUNT_SYMBOLS.count(this->get())) {
                ecm.count = ELEMENT_CONTENT_COUNT_SYMBOLS.at(this->get());
                operator++();
            }
            break;
        } else if (c == COMMA || c == VERTICAL_BAR) {
            operator++();
            // Separator - ensure appropriate.
            if (!separator_next) {
                // Not expecting separator at this time - invalid.
                throw;
            }
            if (separator_seen) {
                // If not first separator, ensure consistent with previous seps.
                // If it is sequence, expecting ',' else if choice, expecting '|'
                if ((c == COMMA) != ecm.is_sequence) {
                    throw;
                }
            } else {
                // First separator.
                separator_seen = true;
                ecm.is_sequence = (c == COMMA);
            }
            separator_next = false;
        } else {
            // Element content part - sub-content-model or name.
            if (separator_next) {
                // Expecting separator at this time, but not seen.
                throw;
            }
            if (c == LEFT_PARENTHESIS) {
                operator++();
                while (is_whitespace(this->get())) {
                    operator++();
                }
                ecm.parts.push_back(this->parse_element_content_model());
            } else {
                ElementContentModel sub_ecm;
                sub_ecm.is_sequence = false;
                sub_ecm.is_name = true;
                sub_ecm.name = this->parse_name(ELEMENT_CONTENT_NAME_TERMINATORS);
                if (ELEMENT_CONTENT_COUNT_SYMBOLS.count(this->get())) {
                    sub_ecm.count = ELEMENT_CONTENT_COUNT_SYMBOLS.at(this->get());
                    operator++();
                }
                ecm.parts.push_back(sub_ecm);
            }
            separator_next = true;
        }
    }
    return ecm;
}

MixedContentModel Parser::parse_mixed_content_model() {
    MixedContentModel mcm;
    bool first = true;
    bool separator_next = false;
    // #
    operator++();
    while (true) {
        Char c = this->get();
        if (is_whitespace(c)) {
            operator++();
            continue;
        }
        if (c == RIGHT_PARENTHESIS) {
            operator++();
            if (!separator_next) {
                // Disallow empty MCM without even #PCDATA, or one ending in separator.
                throw;
            }
            if (!mcm.choices.empty()) {
                if (this->get() != ASTERISK) {
                    // MCM actually ends in )* if not empty.
                    throw;
                }
                operator++();
            }
            break;
        } else if (c == VERTICAL_BAR) {
            operator++();
            if (!separator_next) {
                throw;
            }
            separator_next = false;
        } else {
            if (separator_next) {
                throw;
            }
            String name = this->parse_name(MIXED_CONTENT_NAME_TERMINATORS);
            if (first) {
                // Must be PCDATA to begin with (excluding octothorpe).
                if (name != PCDATA) {
                    throw;
                }
                first = false;
            } else {
                // Duplicate element names are prohibited.
                if (mcm.choices.count(name)) {
                    throw;
                }
                mcm.choices.insert(name);
            }
            separator_next = true;
        }
    }
    return mcm;
}

void Parser::parse_attribute_list_declaration(DoctypeDeclaration& dtd) {
    this->ignore_whitespace();
    String element_name = this->parse_name(WHITESPACE);
    this->ignore_whitespace();
    while (true) {
        Char c = this->get();
        if (is_whitespace(c)) {
            operator++();
            continue;
        }
        if (c == RIGHT_ANGLE_BRACKET) {
            operator++();
            return;
        }
        this->parse_attribute_declaration(dtd, dtd.attribute_list_declarations[element_name]);
    }
}

void Parser::parse_attribute_declaration(DoctypeDeclaration& dtd, AttributeListDeclaration& attlist) {
    AttributeDeclaration ad;
    ad.name = this->parse_name(WHITESPACE);
    this->ignore_whitespace();
    if (this->get() == LEFT_PARENTHESIS) {
        // Can only be an enumeration.
        ad.type = AttributeType::enumeration;
        operator++();
        ad.enumeration = this->parse_enumeration();
    } else {
        ad.type = get_attribute_type(this->parse_name(WHITESPACE));
    }
    this->ignore_whitespace();
    if (ad.type == AttributeType::notation) {
        operator++();
        ad.notations = this->parse_notations();
        this->ignore_whitespace();
    }
    if (this->get() == OCTOTHORPE) {
        // Presence indication.
        operator++();
        String presence = this->parse_name(WHITESPACE_AND_RIGHT_ANGLE_BRACKET);
        ad.presence = get_attribute_presence(presence);
    }
    // Only #FIXED or no presence indication permits a default value.
    if (ad.presence == AttributePresence::fixed || ad.presence == AttributePresence::relaxed) {
        this->ignore_whitespace();
        ad.has_default_value = true;
        ad.default_value = this->parse_attribute_value(dtd);
    }
    // Perform suitable validation possible right now. Uses fallback switch technique.
    // Any attribute type not in the switch does not need validation at this time.
    switch (ad.type) {
        case AttributeType::id:
            // Must be #REQUIRED or #IMPLIED.
            if (ad.presence != AttributePresence::required && ad.presence != AttributePresence::implied) {
                throw;
            }
            break;
        case AttributeType::idref:
        case AttributeType::entity:
            // Default value must match Name.
            if (ad.has_default_value && !valid_name(ad.default_value, true)) {
                throw;
            }
            break;
        case AttributeType::idrefs:
        case AttributeType::entities:
            // Default value must match Names.
            if (ad.has_default_value && !valid_names(ad.default_value)) {
                throw;
            }
            break;
        case AttributeType::nmtoken:
            // Default value must match Nmtoken.
            if (ad.has_default_value && !valid_nmtoken(ad.default_value)) {
                throw;
            }
            break;
        case AttributeType::nmtokens:
            // Default value must match Nmtokens.
            if (ad.has_default_value && !valid_nmtokens(ad.default_value)) {
                throw;
            }
            break;
        case AttributeType::notation:
            // Default value must be one of the notations in the enumeration list.
            if (ad.has_default_value && !ad.notations.count(ad.default_value)) {
                throw;
            }
            break;
        case AttributeType::enumeration:
            // Default value must be one of the enumeration values.
            if (ad.has_default_value && !ad.enumeration.count(ad.default_value)) {
                throw;
            }
            break;
    }
    // If the same attribute name is declared multiple times, only the first one counts.
    if (!attlist.count(ad.name)) {
        attlist[ad.name] = ad;
    }
}

std::set<String> Parser::parse_enumerated_attribute(AttributeType att_type) {
    std::set<String> values;
    bool separator_next = false;
    while (true) {
        Char c = this->get();
        if (is_whitespace(c)) {
            operator++();
            continue;
        }
        if (c == RIGHT_PARENTHESIS) {
            if (!separator_next) {
                // Cannot end on separator.
                throw;
            }
            operator++();
            break;
        } else if (c == VERTICAL_BAR) {
            if (!separator_next) {
                throw;
            }
            operator++();
            separator_next = false;
        } else {
            if (separator_next) {
                throw;
            }
            separator_next = true;
            // Notation -> Name, Enumeration -> NmToken
            String value;
            if (att_type == AttributeType::notation) {
                value = this->parse_name(ENUMERATED_ATTRIBUTE_NAME_TERMINATORS);
            } else {
                value = this->parse_nmtoken(ENUMERATED_ATTRIBUTE_NAME_TERMINATORS);
            }
            if (values.count(value)) {
                // Duplicate enumeration list values prohibited.
                throw;
            }
            values.insert(value);
        }
    }
    // An enumerated attribute list must not be empty.
    if (values.empty()) {
        throw;
    }
    return values;
}

std::set<String> Parser::parse_notations() {
    return this->parse_enumerated_attribute(AttributeType::notation);
}

std::set<String> Parser::parse_enumeration() {
    return this->parse_enumerated_attribute(AttributeType::enumeration);
}

void Parser::parse_entity_declaration(DoctypeDeclaration& dtd) {
    this->ignore_whitespace();
    if (this->get() == PERCENT_SIGN) {
        // Parameter entity. At least 1 whitespace must follow '%'.
        operator++();
        if (!is_whitespace(this->get())) {
            throw;
        }
        this->ignore_whitespace();
        this->parse_parameter_entity_declaration(dtd);
    } else {
        // General entity
        this->parse_general_entity_declaration(dtd);
    }
    this->ignore_whitespace();
    if (this->get() != RIGHT_ANGLE_BRACKET) {
        throw;
    }
    operator++();
}

void Parser::parse_general_entity_declaration(DoctypeDeclaration& dtd) {
    GeneralEntity ge;
    ge.name = this->parse_name(WHITESPACE);
    this->ignore_whitespace();
    if (this->get() == SINGLE_QUOTE || this->get() == DOUBLE_QUOTE) {
        // Has a literal value.
        ge.value = this->parse_entity_value(dtd);
    } else {
        // External entity.
        ge.is_external = true;
        ge.external_id = this->parse_external_id();
        this->ignore_whitespace();
        if (this->get() != RIGHT_ANGLE_BRACKET) {
            // Not done - must be NDATA indication otherwise invalid.
            if (this->parse_name(WHITESPACE) != String("NDATA")) {
                throw;
            }
            this->ignore_whitespace();
            ge.is_unparsed = true;
            ge.notation_name = this->parse_name(WHITESPACE_AND_RIGHT_ANGLE_BRACKET);
        }
    }
    if (BUILT_IN_GENERAL_ENTITIES.count(ge.name)) {
        // If built-in entities are declared explictly, must match their actual value.
        String expected = expand_character_entities(BUILT_IN_GENERAL_ENTITIES.at(ge.name).value);
        String expansion = expand_character_entities(ge.value);
        if (BUILT_IN_GENERAL_ENTITIES_MANDATORY_DOUBLE_ESCAPE.count(ge.name)) {
            if (ge.value == expected || expansion != expected) {
                throw;
            }
        } else {
            if (ge.value != expected && expansion != expected) {
                throw;
            }
        }
    }
    if (!dtd.general_entities.count(ge.name)) {
        // Only count first instance of general entity declaration.
        dtd.general_entities[ge.name] = ge;
    }
}

void Parser::parse_parameter_entity_declaration(DoctypeDeclaration& dtd) {
    ParameterEntity pe;
    pe.name = this->parse_name(WHITESPACE);
    this->ignore_whitespace();
    if (this->get() == SINGLE_QUOTE || this->get() == DOUBLE_QUOTE) {
        // Has a literal value.
        pe.value = this->parse_entity_value(dtd);
    } else {
        // External entity.
        pe.is_external = true;
        pe.external_id = this->parse_external_id();
    }
    if (!dtd.parameter_entities.count(pe.name)) {
        // Only count first instance of parameter entity declaration.
        dtd.parameter_entities[pe.name] = pe;
    }
}

void Parser::parse_notation_declaration(DoctypeDeclaration& dtd) {
    this->ignore_whitespace();
    NotationDeclaration nd;
    nd.name = this->parse_name(WHITESPACE);
    if (dtd.notation_declarations.count(nd.name)) {
        // Cannot have duplicate notation names.
        throw;
    }
    this->ignore_whitespace();
    String type = this->parse_name(WHITESPACE);
    this->ignore_whitespace();
    if (type == String("SYSTEM")) {
        nd.has_public_id = false;
        nd.has_system_id = true;
        nd.system_id = this->parse_system_id();
    } else if (type == String("PUBLIC")) {
        nd.has_public_id = true;
        nd.public_id = this->parse_public_id();
        bool whitespace_seen = false;
        while (is_whitespace(this->get())) {
            operator++();
            whitespace_seen = true;
        }
        nd.has_system_id = (this->get() != RIGHT_ANGLE_BRACKET);
        if (nd.has_system_id) {
            if (!whitespace_seen) {
                // At least 1 whitespace between public/system ID.
                throw;
            }
            // System ID is optional but does appear to exist here.
            nd.system_id = this->parse_system_id();
        }
    } else {
        // Not SYSTEM or PUBLIC -> invalid.
        throw;
    }
    this->ignore_whitespace();
    if (this->get() != RIGHT_ANGLE_BRACKET) {
        throw;
    }
    operator++();
    dtd.notation_declarations[nd.name] = nd;
}

DoctypeDeclaration Parser::parse_doctype_declaration() {
    DoctypeDeclaration dtd;
    dtd.exists = true;
    while (is_whitespace(this->get())) {
        operator++();
    }
    dtd.root_name = this->parse_name(DOCTYPE_DECLARATION_ROOT_NAME_TERMINATORS);
    bool can_parse_external_id = true;
    bool can_parse_internal_subset = true;
    while (true) {
        Char c = this->get();
        if (c == RIGHT_ANGLE_BRACKET) {
            operator++();
            break;
        } else if (is_whitespace(c)) {
            operator++();
        } else if (c == LEFT_SQUARE_BRACKET && can_parse_internal_subset) {
            operator++();
            can_parse_external_id = false;
            can_parse_internal_subset = false;
            this->parse_internal_dtd_subset(dtd);
        } else {
            // External ID or invalid.
            if (!can_parse_external_id) {
                throw;
            }
            can_parse_external_id = false;
            dtd.external_id = this->parse_external_id();
        }
    }
    return dtd;
}

Document Parser::parse_document() {
    Document document;
    // IMPORTANT - XML declaration MUST be the very first thing in the document
    // or else cannot be included. NOT EVEN WHITESPACE BEFORE IT.
    bool xml_declaration_possible = true;
    bool doctype_declaration_seen = false;
    bool root_seen = false;
    while (!this->eof()) {
        Char c = this->get();
        if (is_whitespace(c)) {
            operator++();
            xml_declaration_possible = false;
            continue;
        }
        // Surprisingly, only '<' valid other than whitespace in starting
        // something new. If not, then throw. If so, deduce what is coming up.
        if (c != LEFT_ANGLE_BRACKET) {
            throw;
        }
        operator++();
        switch (this->get()) {
            case QUESTION_MARK: {
                // XML declaration or processing instruction.
                operator++();
                ProcessingInstruction pi = this->parse_processing_instruction(xml_declaration_possible);
                if (pi.target == String("xml")) {
                    // Indeed XML declaration.
                    this->parse_xml_declaration(document);
                } else {
                    document.processing_instructions.push_back(pi);
                }
                break;
            }
            case EXCLAMATION_MARK:
                // Document type definition or comment.
                operator++();
                if (this->get() == '-') {
                    // Hints a comment - need to check further: <!--.
                    operator++();
                    if (this->get() != '-') {
                        throw;
                    }
                    operator++();
                    this->parse_comment();
                } else {
                    // Must be DOCTYPE declaration if not a comment - confirm.
                    String required_string = "DOCTYPE";
                    for (Char required : required_string) {
                        if (this->get() != required) {
                            throw;
                        }
                        operator++();
                    }
                    if (doctype_declaration_seen || root_seen) {
                        // Only one DOCTYPE declaration permitted, before root.
                        throw;
                    }
                    doctype_declaration_seen = true;
                    document.doctype_declaration.exists = true;
                    document.doctype_declaration = this->parse_doctype_declaration();
                    break;
                }
                break;
            default:
                // Must be the root element - cannot be anything else.
                if (root_seen) {
                    // Only 1 root element permitted.
                    throw;
                }
                root_seen = true;
                document.root = this->parse_element(document.doctype_declaration);
        }
        xml_declaration_possible = false;
    }
    if (!root_seen) {
        // No root element seen - invalid doc.
        throw;
    }
    return document;
}

}
