// Element content validation and attributes validation etc.
#pragma once
#include "utils.h"

namespace xml {

// Validates a given document (may have already done some validation during parsing
// but the rest will be completed here).
void validate_document(const Document& document, bool validate_elements);

// Validates an element, ensuring it meets the content requirements
// as declared in the DTD.
void validate_element(const Element& element, const ElementDeclarations&);

// Recursive Helper for element content validation.
// Returns true if match.
bool valid_element_content_helper(const Element& element, const ElementContentModel&, std::size_t&);
// Validates element content (child elements separated by optional whitespace only).
void validate_element_content(const Element& element, const ElementContentModel&);

// Validates mixed content (any number of certain child elements, cdata allowed).
void validate_mixed_content(const Element& element, const MixedContentModel&);

}