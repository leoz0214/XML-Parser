#include "parser.h"
#include "validate.h"
#include <fstream>
#include <iostream>

namespace xml {

EntityStream::EntityStream(const String& text, const String& name) {
    this->text = text;
    this->name = name;
    this->pos = 0;
    this->is_external = false;
}

EntityStream::EntityStream(const std::filesystem::path& file_path, const String& name) {
    this->file_path = file_path;
    std::ifstream* stream_ptr = new std::ifstream(this->file_path);
    this->stream = unique_istream_ptr(stream_ptr);
    this->name = name;
    Parser* parser_ptr = new Parser(*this->stream);
    this->parser = std::unique_ptr<Parser>(parser_ptr);
    this->is_external = true;
    // Seek ahead, checking for text declaration.
    bool has_text_declaration = true;
    for (Char c : String("<?xml")) {
        if (this->eof() || this->get() != c) {
            has_text_declaration = false;
            break;
        }
        operator++();
    }
    if (has_text_declaration) {
        // At least one whitespace after.
        has_text_declaration = !this->eof() && is_whitespace(this->get());
    }
    if (!has_text_declaration) {
        // Reset stream to start.
        this->stream->seekg(std::ios::beg);
        this->parser->previous_char = -1;
        this->parser->just_parsed_carriage_return = false;
        this->leading_parameter_space_done = false;
        this->trailing_parameter_space_done = false;
        return;
    }
    this->parse_text_declaration();
}

Char EntityStream::get() {
    if (this->is_external && this->parser->previous_char != -1) {
        return this->parser->previous_char;
    }
    // Handle special case where parameter entity replacement
    // text must be wrapped with a leading and trailing space if not in entity value.
    if (this->is_parameter && !this->in_entity_value) {
        if (!this->leading_parameter_space_done) {
            return SPACE;
        }
        if (
            !this->trailing_parameter_space_done &&
            this->is_external ? this->parser->eof() : this->pos >= this->text.size()
        ) {
            return SPACE;
        };
    }
    return this->is_external ? this->parser->get() : this->text.at(this->pos);
}

void EntityStream::operator++() {
    if (this->is_parameter && !this->in_entity_value) {
        if (!this->leading_parameter_space_done) {
            this->leading_parameter_space_done = true;
            return;
        }
        if (
            !this->trailing_parameter_space_done &&
            this->is_external ? this->parser->eof() : this->pos >= this->text.size()
        ) {
            this->trailing_parameter_space_done = true;
            return;
        };
    }
    this->pos++;
    if (is_external) {
        this->parser->operator++();
    }
}

bool EntityStream::eof() {
    if (this->is_parameter && !this->in_entity_value) {
        return this->trailing_parameter_space_done;
    }
    return this->is_external ? this->parser->eof() : this->pos >= this->text.size();
}

void EntityStream::parse_text_declaration() {
    bool version_parsed = false;
    bool encoding_parsed = false;
    while (true) {
        this->parser->ignore_whitespace();
        Char c = this->get();
        if (c == QUESTION_MARK) {
            operator++();
            if (this->get() != TAG_CLOSE) {
                throw;
            }
            operator++();
            break;
        }
        DoctypeDeclaration dummy;
        std::pair attribute = this->parser->parse_attribute(dummy, false);
        if (attribute.first == XML_DECLARATION_VERSION_NAME) {
            if (version_parsed || encoding_parsed) {
                throw;
            }
            if (!valid_version(attribute.second)) {
                throw;
            }
            this->version = attribute.second;
            version_parsed = true;
        } else if (attribute.first == XML_DECLARATION_ENCODING_NAME) {
            if (encoding_parsed) {
                throw;
            }
            String& encoding = attribute.second;
            std::transform(encoding.begin(), encoding.end(), encoding.begin(), [](Char c) {
                return std::tolower(c);
            });
            if (!valid_encoding(encoding)) {
                throw;
            }
            this->version = encoding;
            encoding_parsed = true;
        } else {
            throw;
        }
    }
    if (!encoding_parsed) {
        // Encoding is mandatory in external text declaration.
        throw;
    }
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
    if (this->general_entity_stack.size()) {
        return this->general_entity_stack.top().get();
    }
    if (this->parameter_entity_stack.size()) {
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
    this->previous_char = c;
    return c;
}

Char Parser::get(const GeneralEntities& general_entities, bool in_attribute_value) {
    Char c = this->get();
    if (c == AMPERSAND && !this->just_parsed_character_reference) {
        operator++();
        if (this->get() == OCTOTHORPE) {
            operator++();
            return this->parse_character_entity();
        }
        this->parse_general_entity(general_entities, in_attribute_value);
        return this->general_entity_stack.top().get();
    }
    return c;
}

Char Parser::get(
    const ParameterEntities& parameter_entities,
    bool in_markup, bool in_entity_value, bool ignore_whitespace_after_percent_sign
) {
    Char c = this->get();
    if (c == PERCENT_SIGN && !this->just_parsed_character_reference) {
        operator++();
        Char next_char = this->get(parameter_entities, in_markup, in_entity_value);
        if (is_whitespace(next_char) && ignore_whitespace_after_percent_sign) {
            return PERCENT_SIGN;
        }
        if (in_markup && !this->external_dtd_content_active) {
            // Can only have PEs inside markup when parsing external DTD or parameter entity.
            throw;
        }
        this->parse_parameter_entity(parameter_entities, in_entity_value);
        c = this->parameter_entity_stack.top().get();
        if (c == PERCENT_SIGN) {
            return this->get(
                parameter_entities, in_markup, in_entity_value,
                ignore_whitespace_after_percent_sign);
        }
    }
    return c;
}

void Parser::operator++() {
    if (this->general_entity_active) {
        ++this->general_entity_stack.top();
        if (this->general_entity_stack.top().eof() && this->general_entity_stack.size() > 1) {
            this->general_entity_names.erase(this->general_entity_stack.top().name);
            if (this->general_entity_stack.top().is_external) {
                this->file_paths.pop();
            }
            this->general_entity_stack.pop();
        }
        return;
    }
    if (this->parameter_entity_active) {
        ++this->parameter_entity_stack.top();
        if (this->parameter_entity_stack.top().eof() && this->parameter_entity_stack.size() > 1) {
            this->parameter_entity_names.erase(this->parameter_entity_stack.top().name);
            if (this->parameter_entity_stack.top().is_external) {
                this->file_paths.pop();
            }
            this->parameter_entity_stack.pop();
        }
        return;
    }
    if (this->previous_char == -1) {
        this->get();
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

void Parser::ignore_whitespace(const ParameterEntities& parameter_entities) {
    while (is_whitespace(this->get(parameter_entities))) {
        operator++();
    }
}

String Parser::parse_name(
    const String& until, bool validate,
    const ParameterEntities* parameter_entities, const std::set<String>* validation_exemptions
) {
    String name;
    while (true) {
        Char c = parameter_entities != nullptr ? this->get(*parameter_entities) : this->get();
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
    if (
        validate && !valid_name(name) &&
        (validation_exemptions == nullptr || !validation_exemptions->count(name))
    ) {
        throw;
    }
    return name;
}

String Parser::parse_nmtoken(const String& until, const ParameterEntities& parameter_entities) {
    String nmtoken;
    while (true) {
        Char c = this->get(parameter_entities);
        if (std::find(until.cbegin(), until.cend(), c) != until.cend()) {
            break;
        }
        nmtoken.push_back(c);
        operator++();
    }
    return nmtoken;
}

String Parser::parse_attribute_value(const DoctypeDeclaration& dtd, bool references_active, bool is_cdata) {
    // Ensure opening quote (double or single accepted).
    Char quote = this->get();
    if (quote != SINGLE_QUOTE && quote != DOUBLE_QUOTE) {
        throw;
    }
    operator++();
    String value;
    int general_entity_stack_size_before = this->general_entity_stack.size();
    while (true) {
        Char c = this->get(dtd.general_entities, true);
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
                if (is_whitespace(c) && !this->just_parsed_character_reference) {
                    value.push_back(SPACE);
                } else {
                    value.push_back(c);
                }             
            }, general_entity_stack_size_before, true);
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
    if (!is_cdata) {
        // If not cdata, further normalisation is required.
        // Discard leading/trailing spaces and ensure no spaces are adjacent.
        auto non_space_begin = std::find_if(value.begin(), value.end(), [](Char c) {
            return c != SPACE;
        });
        auto non_space_end = std::find_if(value.rbegin(), value.rend(), [](Char c) {
            return c != SPACE;
        }).base();
        String normalised;
        normalised.reserve(non_space_end - non_space_begin);
        for (auto it = non_space_begin; it != non_space_end; ++it) {
            if (*it != SPACE || *(it - 1) != SPACE) {
                normalised.push_back(*it);
            }
        }
        value = std::move(normalised);
    }
    return value;
}

String Parser::parse_entity_value(const DoctypeDeclaration& dtd) {
    Char quote = this->get(dtd.parameter_entities);
    if (quote != SINGLE_QUOTE && quote != DOUBLE_QUOTE) {
        throw;
    }
    operator++();
    String value;
    int parameter_entity_stack_size_before = this->parameter_entity_stack.size();
    while (true) {
        Char c = this->get(dtd.parameter_entities, false, true);
        operator++();
        if (this->parameter_entity_eof()) {
            this->end_parameter_entity();
        }
        if (c == AMPERSAND && !this->just_parsed_character_reference) {
            // General/character entity.
            if (this->get(dtd.parameter_entities) == OCTOTHORPE) {
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
            if (c == quote && this->parameter_entity_stack.size() <= parameter_entity_stack_size_before) {
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
        if (c == SPACE) {
            throw;
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
        if (c == SPACE) {
            throw;
        }
        name.push_back(c);
    }
    if (!general_entities.count(name)) {
        // General entity name is not declared.
        throw;
    }
    return name;
}

void Parser::parse_general_entity(const GeneralEntities& general_entities, bool in_attribute_value) {
    String name = this->parse_general_entity_name(general_entities);
    if (!general_entities.count(name)) {
        throw;
    }
    if (general_entities.at(name).is_unparsed) {
        throw;
    }
    if (
        !BUILT_IN_GENERAL_ENTITIES.count(name)
        && general_entities.at(name).from_external && this->standalone
    ) {
        // Entities DECLARED INSIDE external files prohibited if standalone.
        throw;
    }
    if (general_entity_names.count(name)) {
        // Recursive self-reference - illegal.
        throw;
    }
    general_entity_names.insert(name);
    if (general_entities.at(name).is_external) {
        if (in_attribute_value) {
            // Attribute values must not contain entity references to external entities.
            throw;
        }
        this->general_entity_stack.push({general_entities.at(name).external_id.system_id, name});
        this->file_paths.push(this->general_entity_stack.top().file_path);
    } else {
        this->general_entity_stack.push({general_entities.at(name).value, name});
    }
    this->general_entity_active = true;
}

String Parser::parse_general_entity_text(
    const GeneralEntities& general_entities, std::function<void(Char)> func,
    int original_depth, bool in_attribute_value
) {
    String text;
    while (true) {
        // Ampersand means recursive entity reference.
        // Only if not ampersand should character be added to value.
        Char c = this->get(general_entities, in_attribute_value);
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
    if (this->general_entity_stack.top().is_external) {
        this->file_paths.pop();
    }
    this->general_entity_stack = {};
    this->general_entity_names = {};
    this->general_entity_active = false;
}

void Parser::parse_parameter_entity(const ParameterEntities& parameter_entities, bool in_entity_value) {
    String name = "";
    while (true) {
        Char c = this->get();
        operator++();
        if (c == SEMI_COLON) {
            break;
        }
        if (this->parameter_entity_eof()) {
            throw;
        }
        name.push_back(c);
    }
    if (!parameter_entities.count(name)) {
        // Parameter entity name is not declared.
        throw;
    }
    if (parameter_entity_names.count(name)) {
        // Recursive self-reference - illegal.
        throw;
    }
    parameter_entity_names.insert(name);
    EntityStream parameter_entity = parameter_entities.at(name).is_external
        ? EntityStream(parameter_entities.at(name).external_id.system_id, name)
        : EntityStream(parameter_entities.at(name).value, name);
    parameter_entity.is_parameter = true;
    parameter_entity.in_entity_value = in_entity_value;
    if (this->parameter_entity_stack.empty()) {
        this->external_dtd_content_active = parameter_entity.is_external;
    }
    if (parameter_entity.is_external) {
        this->file_paths.push(parameter_entity.file_path);
    }
    this->parameter_entity_stack.push(std::move(parameter_entity));
    this->parameter_entity_active = true;
}

std::filesystem::path Parser::get_file_path() {
    if (this->file_paths.empty()) {
        return std::filesystem::current_path();
    }
    return this->file_paths.top().parent_path();
}

void Parser::end_parameter_entity() {
    if (this->parameter_entity_stack.top().is_external) {
        this->file_paths.pop();
    }
    this->parameter_entity_stack = {};
    this->parameter_entity_names = {};
    this->parameter_entity_active = false;
    this->external_dtd_content_active = false;
}

std::pair<String, String> Parser::parse_attribute(
    const DoctypeDeclaration& dtd, bool references_active, bool is_cdata, const String* tag_name
) {
    String name = this->parse_name(ATTRIBUTE_NAME_TERMINATORS, true, nullptr, &SPECIAL_ATTRIBUTE_NAMES);
    // Ignore whitespace until '=' is reached.
    this->ignore_whitespace();
    if (this->get() != EQUAL) {
        throw;
    }
    // Increment the '='
    operator++();
    this->ignore_whitespace();
    if (!is_cdata) {
        // CDATA not forced - only CDATA if attribute not declared
        // or attribute is declared as CDATA, for the given element.
        is_cdata = tag_name != nullptr && (
            !dtd.attribute_list_declarations.count(*tag_name)
            || (
                dtd.attribute_list_declarations.at(*tag_name).count(name)
                && dtd.attribute_list_declarations.at(*tag_name).at(name).type == AttributeType::cdata)
        );
    }
    String value = this->parse_attribute_value(dtd, references_active, is_cdata);
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
            std::pair<String, String> attribute = this->parse_attribute(dtd, true, false, &tag.name);
            if (tag.attributes.count(attribute.first)) {
                // Duplicate attribute names in same element prohibited.
                throw;
            }
            tag.attributes.insert(attribute);
        }
        if (dtd.attribute_list_declarations.count(tag.name)) {
            // Add default values if available. No validation here at all. That is for later.
            for (const auto& [attribute_name, attribute] : dtd.attribute_list_declarations.at(tag.name)) {
                if (!tag.attributes.count(attribute_name) && attribute.has_default_value) {
                    if (attribute.from_external && this->standalone) {
                        // Default attribute values from external cannot be used in standalone doc.
                        throw;
                    }
                    tag.attributes[attribute_name] = attribute.default_value;
                }
            }
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
        if (
            c == RIGHT_ANGLE_BRACKET && prev_char == RIGHT_SQUARE_BRACKET
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
            element.is_empty = false;
            element.children_only = false;
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
                                element.children_only = false;
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
                        element.children_only = false;
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
                element.children_only = false;
                break;
            default:
                // Any other character continues on the character data if valid.
                if (!valid_character(c)) {
                    throw;
                }
                operator++();
                char_data.push_back(c);
                if (!is_whitespace(c)) {
                    element.children_only = false;
                }
        }
        element.is_empty = false;
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

std::filesystem::path Parser::parse_public_id(const ParameterEntities* parameter_entities) {
    Char quote = parameter_entities != nullptr ? this->get(*parameter_entities) : this->get();
    operator++();
    if (quote != SINGLE_QUOTE && quote != DOUBLE_QUOTE) {
        throw;
    }
    String public_id;
    while (true) {
        Char c = parameter_entities != nullptr ? this->get(*parameter_entities) : this->get();
        operator++();
        if (c == quote) {
            break;
        }
        if (!valid_public_id_character(c)) {
            throw;
        }
        public_id.push_back(c);
    }
    std::filesystem::path result = std::string(public_id);
    if (result.is_relative()) {
        result = (this->get_file_path() / result).lexically_normal();
    }
    return result;
}

std::filesystem::path Parser::parse_system_id(const ParameterEntities* parameter_entities) {
    Char quote = parameter_entities != nullptr ? this->get(*parameter_entities) : this->get();
    operator++();
    if (quote != SINGLE_QUOTE && quote != DOUBLE_QUOTE) {
        throw;
    }
    String system_id;
    while (true) {
        Char c = parameter_entities != nullptr ? this->get(*parameter_entities) : this->get();
        operator++();
        if (c == quote) {
            break;
        }
        if (!valid_character(c)) {
            throw;
        }
        system_id.push_back(c);
    }
    std::filesystem::path result = std::string(system_id);
    if (result.is_relative()) {
        result = (this->get_file_path() / result).lexically_normal();
    }
    return result;
}

ExternalID Parser::parse_external_id(const ParameterEntities* parameter_entities) {
    ExternalID external_id;
    // Looking for PUBLIC / SYSTEM - parsing like a name is just fine.
    String type_string = this->parse_name(WHITESPACE, false, parameter_entities);
    external_id.type = get_external_id_type(type_string);
    if (parameter_entities) {
        this->ignore_whitespace(*parameter_entities);
    } else {
        this->ignore_whitespace();
    }
    if (external_id.type == ExternalIDType::public_) {
        external_id.public_id = this->parse_public_id(parameter_entities);
        if (!is_whitespace(parameter_entities != nullptr ? this->get(*parameter_entities) : this->get())) {
            // Min 1 whitespace between public/system ID.
            throw;
        }
        if (parameter_entities) {
            this->ignore_whitespace(*parameter_entities);
        } else {
            this->ignore_whitespace();
        }
    }
    external_id.system_id = this->parse_system_id(parameter_entities);
    return external_id;
}

void Parser::parse_dtd_subsets(DoctypeDeclaration& dtd, bool external_subset_started, bool in_include) {
    int initial_parameter_entity_stack_size = this->parameter_entity_stack.size();
    // If true, starting straight from external subset (no internal subset).
    // Otherwise, parse internal subset first, followed by external subset (if exists).
    if (external_subset_started) {
        this->start_external_subset(dtd.external_id.system_id);
    }
    while (true) {
        if (this->parameter_entity_eof()) {
            this->end_parameter_entity();
            if (external_subset_started) {
                // End of external subset reached.
                return;
            }
        }
        Char c = this->get(dtd.parameter_entities, false);
        operator++();
        if (c == RIGHT_SQUARE_BRACKET) {
            if (this->parameter_entity_stack.size() != initial_parameter_entity_stack_size) {
                throw;
            }
            if (
                !external_subset_started && dtd.external_id.type != ExternalIDType::none 
                && !in_include
            ) {
                initial_parameter_entity_stack_size = this->parameter_entity_stack.size();
                // Create dummy parameter entity - treat External Subset like one.
                this->start_external_subset(dtd.external_id.system_id);
                external_subset_started = true;
                continue;
            }
            // End of internal (subset & no external subset) OR (end of include subset), done.
            return;
        }
        int parameter_entity_stack_size_before = this->parameter_entity_stack.size();
        if (is_whitespace(c)) {
            continue;
        } else if (c == LEFT_ANGLE_BRACKET) {
            // Must be exclamation mark or processing instruction or invalid.
            c = this->get();
            if (c != EXCLAMATION_MARK) {
                if (c == QUESTION_MARK) {
                    operator++();
                    dtd.processing_instructions.push_back(this->parse_processing_instruction());
                    continue;
                }
                // Not processing instruction.
                throw;
            }
            operator++();
            if (this->get() == HYPHEN) {
                // Comment or invalid.
                operator++();
                if (this->get() != HYPHEN) {
                    throw;
                }
                operator++();
                this->parse_comment();
            } else if (this->get() == LEFT_SQUARE_BRACKET) {
                // Conditional section or invalid.
                if (!external_subset_started && !in_include) {
                    // In internal dtd, cannot have conditional section.
                    throw;
                }
                operator++();
                this->parse_conditional_section(dtd, parameter_entity_stack_size_before);
            } else {
                // Markup declaration or invalid.
                this->parse_markup_declaration(dtd);
            }
        } else {
            throw;
        }
        int parameter_entity_stack_size_after = this->parameter_entity_stack.size();
        if (parameter_entity_stack_size_after != parameter_entity_stack_size_before) {
            throw;
        }
    }
}

void Parser::start_external_subset(const std::filesystem::path& system_id) {
    // Treat external subset like a special external parameter entity.
    // External subset and external param entities are actually quite similar,
    // except external subset MUST be full markup (part of DTD after all).
    // DUMMY NAME i.e. empty string (no actual parameter entity can share the name).
    EntityStream parameter_entity(system_id,  "");
    parameter_entity.is_parameter = true;
    parameter_entity.in_entity_value = false;
    this->external_dtd_content_active = true;
    this->file_paths.push(parameter_entity.file_path);
    this->parameter_entity_stack.push(std::move(parameter_entity));
    this->parameter_entity_active = true;
}

void Parser::parse_markup_declaration(DoctypeDeclaration& dtd) {
    String markup_declaration_type = this->parse_name(WHITESPACE, false, &dtd.parameter_entities);
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

void Parser::parse_conditional_section(DoctypeDeclaration& dtd, int parameter_entity_stack_size_before) {
    this->ignore_whitespace(dtd.parameter_entities);
    String type = this->parse_name(CONDITIONAL_TYPE_NAME_TERMINATORS, true, &dtd.parameter_entities);
    this->ignore_whitespace(dtd.parameter_entities);
    if (
        this->get() != LEFT_SQUARE_BRACKET ||
        this->parameter_entity_stack.size() != parameter_entity_stack_size_before
    ) {
        throw;
    }
    operator++();
    if (type == String("INCLUDE")) {
        this->parse_include_section(dtd);
    } else if (type == String("IGNORE")) {
        this->parse_ignore_section();
    } else {
        // Neither INCLUDE nor IGNORE => Invalid.
        throw;
    }
}

void Parser::parse_include_section(DoctypeDeclaration& dtd) {
    this->parse_dtd_subsets(dtd, false, true);
    // Must end on ]]> (first closing right square bracket already registered).
    if (this->get(dtd.parameter_entities) != RIGHT_SQUARE_BRACKET) {
        throw;
    }
    operator++();
    if (this->get(dtd.parameter_entities) != RIGHT_ANGLE_BRACKET) {
        throw;
    }
    operator++();
}

void Parser::parse_ignore_section() {
    Char prev2 = -1;
    Char prev = -1;
    int remaining_closures = 1;
    // Loop until matching closing part of ignore section reached.
    while (remaining_closures) {
        Char c = this->get();
        operator++();
        // <!- indicates opening (+1 to count stack).
        if (c == LEFT_SQUARE_BRACKET && prev == EXCLAMATION_MARK && prev2 == LEFT_ANGLE_BRACKET) {
            remaining_closures++;
        }
        // ]]> indicates closure (-1 to count stack).
        if (c == RIGHT_ANGLE_BRACKET && prev == RIGHT_SQUARE_BRACKET && prev2 == RIGHT_SQUARE_BRACKET) {
            remaining_closures--;
        }
        if (!valid_character(c)) {
            throw;
        }
        prev2 = prev;
        prev = c;
    }
}

void Parser::parse_element_declaration(DoctypeDeclaration& dtd) {
    this->ignore_whitespace(dtd.parameter_entities);
    ElementDeclaration element_declaration;
    element_declaration.name = this->parse_name(WHITESPACE, true, &dtd.parameter_entities);
    // Duplicate declarations for same element name illegal.
    if (dtd.element_declarations.count(element_declaration.name)) {
        throw;
    }
    this->ignore_whitespace(dtd.parameter_entities);
    // Empty, any, children, mixed OR invalid.
    if (this->get(dtd.parameter_entities) == LEFT_PARENTHESIS) {
        int parameter_entity_stack_size = this->parameter_entity_stack.size();
        // Mixed/element content.
        operator++();
        this->ignore_whitespace(dtd.parameter_entities);
        if (this->get(dtd.parameter_entities) == OCTOTHORPE) {
            // Indicates mixed content or invalid.
            element_declaration.type = ElementType::mixed;
            element_declaration.mixed_content = this->parse_mixed_content_model(
                dtd.parameter_entities, parameter_entity_stack_size);
        } else {
            // Indicates element content or invalid.
            element_declaration.type = ElementType::children;
            element_declaration.element_content = this->parse_element_content_model(
                dtd.parameter_entities, parameter_entity_stack_size);
        }
    } else {
        // Must be EMPTY or ANY otherwise.
        String element_type = this->parse_name(
            WHITESPACE_AND_RIGHT_ANGLE_BRACKET, false, &dtd.parameter_entities);
        if (element_type == String("EMPTY")) {
            element_declaration.type = ElementType::empty;
        } else if (element_type == String("ANY")) {
            element_declaration.type = ElementType::any;
        } else {
            throw;
        }
    }
    this->ignore_whitespace(dtd.parameter_entities);
    if (this->get(dtd.parameter_entities) != RIGHT_ANGLE_BRACKET) {
        throw;
    }
    operator++();
    dtd.element_declarations[element_declaration.name] = element_declaration;
}

ElementContentModel Parser::parse_element_content_model(
    const ParameterEntities& parameter_entities, int parameter_entity_stack_size_before
) {
    ElementContentModel ecm;
    bool separator_seen = false;
    bool separator_next = false;
    while (true) {
        Char c = this->get(parameter_entities);
        if (this->parameter_entity_stack.size() < parameter_entity_stack_size_before) {
            // Opening and closing parentheses must be in same PE replacement text.
            throw;
        }
        if (is_whitespace(c)) {
            operator++();
            continue;
        }
        if (c == RIGHT_PARENTHESIS) {
            if (this->parameter_entity_stack.size() != parameter_entity_stack_size_before) {
                throw;
            }
            operator++();
            // Cannot end on separator - ensure this is not case (also cannot be empty).
            if (!separator_next) {
                throw;
            }
            if (ELEMENT_CONTENT_COUNT_SYMBOLS.count(this->get(parameter_entities))) {
                ecm.count = ELEMENT_CONTENT_COUNT_SYMBOLS.at(this->get(parameter_entities));
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
                int parameter_entity_stack_size = this->parameter_entity_stack.size();
                operator++();
                this->ignore_whitespace(parameter_entities);
                ecm.parts.push_back(
                    this->parse_element_content_model(parameter_entities, parameter_entity_stack_size));
            } else {
                ElementContentModel sub_ecm;
                sub_ecm.is_sequence = false;
                sub_ecm.is_name = true;
                sub_ecm.name = this->parse_name(
                    ELEMENT_CONTENT_NAME_TERMINATORS, true, &parameter_entities);
                if (ELEMENT_CONTENT_COUNT_SYMBOLS.count(this->get(parameter_entities))) {
                    sub_ecm.count = ELEMENT_CONTENT_COUNT_SYMBOLS.at(this->get(parameter_entities));
                    operator++();
                }
                ecm.parts.push_back(sub_ecm);
            }
            separator_next = true;
        }
    }
    return ecm;
}

MixedContentModel Parser::parse_mixed_content_model(
    const ParameterEntities& parameter_entities, int parameter_entity_stack_size_before
) {
    MixedContentModel mcm;
    bool first = true;
    bool separator_next = false;
    // #
    operator++();
    while (true) {
        Char c = this->get(parameter_entities);
        if (this->parameter_entity_stack.size() < parameter_entity_stack_size_before) {
            // Opening and closing parentheses must be in same PE replacement text.
            throw;
        }
        if (is_whitespace(c)) {
            operator++();
            continue;
        }
        if (c == RIGHT_PARENTHESIS) {
            if (this->parameter_entity_stack.size() != parameter_entity_stack_size_before) {
                throw;
            }
            operator++();
            if (!separator_next) {
                // Disallow empty MCM without even #PCDATA, or one ending in separator.
                throw;
            }
            if (this->get(parameter_entities) != ASTERISK) {
                if (!mcm.choices.empty()) {
                    // MCM MUST end in )* if not empty.
                    throw;
                }
            } else {
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
            String name = this->parse_name(
                MIXED_CONTENT_NAME_TERMINATORS, true, &parameter_entities);
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
    this->ignore_whitespace(dtd.parameter_entities);
    String element_name = this->parse_name(WHITESPACE, true, &dtd.parameter_entities);
    this->ignore_whitespace(dtd.parameter_entities);
    while (true) {
        Char c = this->get(dtd.parameter_entities);
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
    ad.name = this->parse_name(WHITESPACE, true, &dtd.parameter_entities, &SPECIAL_ATTRIBUTE_NAMES);
    this->ignore_whitespace(dtd.parameter_entities);
    if (this->get(dtd.parameter_entities) == LEFT_PARENTHESIS) {
        // Can only be an enumeration.
        ad.type = AttributeType::enumeration;
        operator++();
        ad.enumeration = this->parse_enumeration(dtd.parameter_entities);
    } else {
        ad.type = get_attribute_type(this->parse_name(WHITESPACE, true, &dtd.parameter_entities));
    }
    this->ignore_whitespace(dtd.parameter_entities);
    if (ad.type == AttributeType::notation) {
        operator++();
        ad.notations = this->parse_notations(dtd.parameter_entities);
        this->ignore_whitespace(dtd.parameter_entities);
    }
    if (this->get(dtd.parameter_entities) == OCTOTHORPE) {
        // Presence indication.
        operator++();
        String presence = this->parse_name(
            WHITESPACE_AND_RIGHT_ANGLE_BRACKET, true, &dtd.parameter_entities);
        ad.presence = get_attribute_presence(presence);
    }
    // Only #FIXED or no presence indication permits a default value.
    if (ad.presence == AttributePresence::fixed || ad.presence == AttributePresence::relaxed) {
        this->ignore_whitespace(dtd.parameter_entities);
        ad.has_default_value = true;
        ad.default_value = this->parse_attribute_value(dtd, true, false);
    }
    // If the same attribute name is declared multiple times, only the first one counts.
    // Ignore repeat declaration.
    if (attlist.count(ad.name)) {
        return;
    }
    // Special attributes validation.
    if (ad.name == XML_SPACE) {
        if (
            ad.type != AttributeType::enumeration || !XML_SPACE_ENUMS.count(ad.enumeration)
        ) {
            throw;
        }
    }
    ad.from_external = this->external_dtd_content_active;
    // Register for now, validate later.
    attlist[ad.name] = ad;
}

std::set<String> Parser::parse_enumerated_attribute(
    AttributeType att_type, const ParameterEntities& parameter_entities
) {
    std::set<String> values;
    bool separator_next = false;
    while (true) {
        Char c = this->get(parameter_entities);
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
                value = this->parse_name(
                    ENUMERATED_ATTRIBUTE_NAME_TERMINATORS, true, &parameter_entities);
            } else {
                value = this->parse_nmtoken(ENUMERATED_ATTRIBUTE_NAME_TERMINATORS, parameter_entities);
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

std::set<String> Parser::parse_notations(const ParameterEntities& parameter_entities) {
    return this->parse_enumerated_attribute(AttributeType::notation, parameter_entities);
}

std::set<String> Parser::parse_enumeration(const ParameterEntities& parameter_entities) {
    return this->parse_enumerated_attribute(AttributeType::enumeration, parameter_entities);
}

void Parser::parse_entity_declaration(DoctypeDeclaration& dtd) {
    this->ignore_whitespace();
    if (this->get(dtd.parameter_entities, true, false, true) == PERCENT_SIGN) {
        // Parameter entity. 1 whitespace followed '%' (see get method).
        this->ignore_whitespace(dtd.parameter_entities);
        this->parse_parameter_entity_declaration(dtd);
    } else {
        // General entity
        this->parse_general_entity_declaration(dtd);
    }
    this->ignore_whitespace(dtd.parameter_entities);
    if (this->get(dtd.parameter_entities) != RIGHT_ANGLE_BRACKET) {
        throw;
    }
    operator++();
}

void Parser::parse_general_entity_declaration(DoctypeDeclaration& dtd) {
    GeneralEntity ge;
    ge.name = this->parse_name(WHITESPACE, true, &dtd.parameter_entities);
    this->ignore_whitespace(dtd.parameter_entities);
    Char quote = this->get(dtd.parameter_entities);
    if (quote == SINGLE_QUOTE || quote == DOUBLE_QUOTE) {
        // Has a literal value.
        ge.value = this->parse_entity_value(dtd);
    } else {
        // External entity.
        ge.is_external = true;
        ge.external_id = this->parse_external_id(&dtd.parameter_entities);
        this->ignore_whitespace(dtd.parameter_entities);
        if (this->get(dtd.parameter_entities) != RIGHT_ANGLE_BRACKET) {
            // Not done - must be NDATA indication otherwise invalid.
            if (this->parse_name(WHITESPACE, true, &dtd.parameter_entities) != String("NDATA")) {
                throw;
            }
            this->ignore_whitespace(dtd.parameter_entities);
            ge.is_unparsed = true;
            ge.notation_name = this->parse_name(
                WHITESPACE_AND_RIGHT_ANGLE_BRACKET, true, &dtd.parameter_entities);
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
        ge.from_external = this->external_dtd_content_active;
        // Only count first instance of general entity declaration.
        dtd.general_entities[ge.name] = ge;
    }
}

void Parser::parse_parameter_entity_declaration(DoctypeDeclaration& dtd) {
    ParameterEntity pe;
    pe.name = this->parse_name(WHITESPACE,true, &dtd.parameter_entities);
    this->ignore_whitespace(dtd.parameter_entities);
    Char quote = this->get(dtd.parameter_entities);
    if (quote == SINGLE_QUOTE || quote == DOUBLE_QUOTE) {
        // Has a literal value.
        pe.value = this->parse_entity_value(dtd);
    } else {
        // External entity.
        pe.is_external = true;
        pe.external_id = this->parse_external_id(&dtd.parameter_entities);
    }
    if (!dtd.parameter_entities.count(pe.name)) {
        pe.from_external = this->external_dtd_content_active;
        // Only count first instance of parameter entity declaration.
        dtd.parameter_entities[pe.name] = pe;
    }
}

void Parser::parse_notation_declaration(DoctypeDeclaration& dtd) {
    this->ignore_whitespace(dtd.parameter_entities);
    NotationDeclaration nd;
    nd.name = this->parse_name(WHITESPACE, true, &dtd.parameter_entities);
    if (dtd.notation_declarations.count(nd.name)) {
        // Cannot have duplicate notation names.
        throw;
    }
    this->ignore_whitespace(dtd.parameter_entities);
    String type = this->parse_name(WHITESPACE, true, &dtd.parameter_entities);
    this->ignore_whitespace(dtd.parameter_entities);
    if (type == String("SYSTEM")) {
        nd.has_public_id = false;
        nd.has_system_id = true;
        nd.system_id = this->parse_system_id(&dtd.parameter_entities);
    } else if (type == String("PUBLIC")) {
        nd.has_public_id = true;
        nd.public_id = this->parse_public_id(&dtd.parameter_entities);
        bool whitespace_seen = false;
        while (is_whitespace(this->get(dtd.parameter_entities))) {
            operator++();
            whitespace_seen = true;
        }
        nd.has_system_id = (this->get(dtd.parameter_entities) != RIGHT_ANGLE_BRACKET);
        if (nd.has_system_id) {
            if (!whitespace_seen) {
                // At least 1 whitespace between public/system ID.
                throw;
            }
            // System ID is optional but does appear to exist here.
            nd.system_id = this->parse_system_id(&dtd.parameter_entities);
        }
    } else {
        // Not SYSTEM or PUBLIC -> invalid.
        throw;
    }
    this->ignore_whitespace(dtd.parameter_entities);
    if (this->get(dtd.parameter_entities) != RIGHT_ANGLE_BRACKET) {
        throw;
    }
    operator++();
    dtd.notation_declarations[nd.name] = nd;
}

DoctypeDeclaration Parser::parse_doctype_declaration() {
    DoctypeDeclaration dtd;
    dtd.exists = true;
    this->ignore_whitespace();
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
            this->parse_dtd_subsets(dtd);
        } else {
            // External ID or invalid.
            if (!can_parse_external_id) {
                throw;
            }
            can_parse_external_id = false;
            dtd.external_id = this->parse_external_id();
        }
    }
    if (can_parse_internal_subset && dtd.external_id.type != ExternalIDType::none) {
        // Internal subset not provided but parse external subset anyways.
        this->parse_dtd_subsets(dtd, true);
    }
    // Validate attribute list declaration after entire DTD parsed.
    validate_attribute_list_declarations(dtd);
    return dtd;
}

Document Parser::parse_document(bool validate_elements, bool validate_attributes) {
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
                    if (document.standalone) {
                        this->standalone = true;
                    }
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
                document.root = this->parse_element(document.doctype_declaration, false);
        }
        xml_declaration_possible = false;
    }
    if (!root_seen) {
        // No root element seen - invalid doc.
        throw;
    }
    // Only validate document if DTD given - otherwise be lenient.
    if (document.doctype_declaration.exists) {
        validate_document(document, validate_elements, validate_attributes);
    }
    return document;
}

}
