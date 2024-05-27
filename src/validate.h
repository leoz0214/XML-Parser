// Element content validation and attributes validation etc.
#pragma once
#include "utils.h"
#include <set>

namespace xml {

// Validates a given document (may have already done some validation during parsing
// but the rest will be completed here).
void validate_document(const Document&, bool, bool);

// Validates an element, ensuring it meets the content requirements
// as declared in the DTD.
void validate_element(const Element&, const ElementDeclarations&, bool);

// Recursive Helper for element content validation.
// Returns true if match.
bool valid_element_content_helper(const Element&, const ElementContentModel&, std::size_t&);
// Validates element content (child elements separated by optional whitespace only).
void validate_element_content(const Element&, const ElementContentModel&, bool);

// Validates mixed content (any number of certain child elements, cdata allowed).
void validate_mixed_content(const Element&, const MixedContentModel&);

// Validates attribute list declarations after parsing DTD.
void validate_attribute_list_declarations(const DoctypeDeclaration&);

// Validates a default attribute value. Less restrictions than actual attributes.
void validate_default_attribute_value(
    const AttributeDeclaration&, const DoctypeDeclaration&, const String* = nullptr);

// Validates the attributes of an element, and then attributes of child elements recursively.
void validate_attributes(const Element&, const DoctypeDeclaration&, const std::set<String>&);

// Parse and validate all ID values, ensuring no duplicates. Before
// general attributes validation. Does not do any other validation.
void parse_and_validate_ids(const Element&, const DoctypeDeclaration&, std::set<String>&);

}