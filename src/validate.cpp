#include "validate.h"
#include <algorithm>
#include <iostream>


namespace xml {

void validate_document(const Document& document, bool validate_elements) {
    if (document.root.tag.name != document.doctype_declaration.root_name) {
        // Root name must match the root name in the DTD.
        throw;
    }
    if (validate_elements) {
            // Validate root element and all its children (recursively).
        validate_element(document.root, document.doctype_declaration.element_declarations);
    }
}

void validate_element(const Element& element, const ElementDeclarations& element_declarations) {
    if (!element_declarations.count(element.tag.name)) {
        // Element not declared at all.
        throw;
    }
    ElementDeclaration ed = element_declarations.at(element.tag.name);
    switch (ed.type) {
        case ElementType::any:
            // No content restriction whatsoever.
            break;
        case ElementType::empty:
            // Element must simply have no content.
            if (!element.is_empty) {
                throw;
            }
            break;
        case ElementType::children:
            validate_element_content(element, ed.element_content);
            break;
        case ElementType::mixed:
            validate_mixed_content(element, ed.mixed_content);
            break;
    }
    // Validate all child elements in the same way.
    for (const Element& child : element.children) {
        validate_element(child, element_declarations);
    }
}

bool valid_element_content_helper(
    const Element& element, const ElementContentModel& ecm, std::size_t& pos
) {
    int min_count = ELEMENT_CONTENT_COUNT_MINIMA.at(ecm.count);
    int max_count = ELEMENT_CONTENT_COUNT_MAXIMA.at(ecm.count);
    int count = 0;
    if (ecm.is_name) {
        // Element name matching, in some way the base case.
        while (
            count < max_count && pos < element.children.size() 
            && element.children.at(pos).tag.name == ecm.name
        ) {
            count++;
            pos++;
        }
    } else {
        std::size_t previous_pos = pos;
        auto recursive_call = [&](const ElementContentModel& child_ecm) {
            return valid_element_content_helper(element, child_ecm, pos);
        };
        if (ecm.is_sequence) {
            // Sequence - full matches required.
            while (
                count < max_count && std::all_of(ecm.parts.begin(), ecm.parts.end(), recursive_call)
            ) {
                if (pos == previous_pos) {
                    // No progress - infinite count (DEADLOCK).
                    count = INT_MAX;
                    break;
                }
                previous_pos = pos;
                count++;
            }
        } else {
            // Choice - any (or first) match can be accepted.
            while (
                count < max_count && std::any_of(ecm.parts.begin(), ecm.parts.end(), recursive_call)
            ) {
                if (pos == previous_pos) {
                    count = INT_MAX;
                    break;
                }
                previous_pos = pos;
                count++;
            }
        }
    }
    return count >= min_count && count <= max_count;
}

void validate_element_content(const Element& element, const ElementContentModel& ecm) {
    // Ensure only interspersed whitespace.
    if (!std::all_of(element.text.begin(), element.text.end(), is_whitespace)) {
        throw;
    }
    std::size_t pos = 0;
    // Note: must also ensure all elements covered (hence pos must equal child count in the end).
    if (!valid_element_content_helper(element, ecm, pos) || pos < element.children.size()) {
        throw 0.0;
    }
}

void validate_mixed_content(const Element& element, const MixedContentModel& mcm) {
    // Simply ensure all child elements have accepted name.
    if (!std::all_of(element.children.begin(), element.children.end(), [&](const Element& child) {
        return mcm.choices.count(child.tag.name);
    })) {
        throw 0.0;
    }
}

}