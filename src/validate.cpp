#include "validate.h"
#include <algorithm>
#include <iostream>
#include <sstream>


namespace xml {

void validate_document(
    const Document& document, bool validate_elements, bool validate_attributes_
) {
    if (document.root.tag.name != document.doctype_declaration.root_name) {
        // Root name must match the root name in the DTD.
        throw;
    }
    if (validate_elements) {
        // Validate root element and all its children (recursively).
        validate_element(
            document.root, document.doctype_declaration.element_declarations, document.standalone);
    }
    if (validate_attributes_) {
        // Validate root element attributes and all attributes of children (recursively).
        std::set<String> ids;
        parse_and_validate_ids(document.root, document.doctype_declaration, ids);
        validate_attributes(document.root, document.doctype_declaration, ids);
    }
}

void validate_element(
    const Element& element, const ElementDeclarations& element_declarations, bool standalone
) {
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
            validate_element_content(element, ed.element_content, standalone);
            break;
        case ElementType::mixed:
            validate_mixed_content(element, ed.mixed_content);
            break;
    }
    // Validate all child elements in the same way.
    for (const Element& child : element.children) {
        validate_element(child, element_declarations, standalone);
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

void validate_element_content(const Element& element, const ElementContentModel& ecm, bool standalone) {
    if (standalone && !element.text.empty()) {
        // Must not be standalone if white space occurs directly within any instance of those types
        throw;
    }
    if (!element.children_only) {
        throw 0.0;
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

void validate_attribute_list_declarations(const DoctypeDeclaration& dtd) {
    for (const auto& [element_name, ald] : dtd.attribute_list_declarations) {
        // Validate attribute list for given element.
        bool id_attribute_seen = false;
        bool notation_attribute_seen = false;
        for (const auto& [_, ad] : ald) {
            if (ad.type == AttributeType::id) {
                // An ID attribute must have a declared default of #IMPLIED or #REQUIRED.
                if (
                    ad.presence != AttributePresence::required && ad.presence != AttributePresence::implied
                ) {
                    throw;
                }
                // An element type must not have more than one ID attribute specified.
                if (id_attribute_seen) {
                    throw;
                }
                id_attribute_seen = true;
            } else if (ad.type == AttributeType::notation) {
                // An element type must not have more than one NOTATION attribute specified.
                if (notation_attribute_seen) {
                    throw;
                }
                notation_attribute_seen = true;
                // An attribute of type NOTATION must not be declared on an element declared EMPTY.
                if (
                    dtd.element_declarations.count(element_name)
                    && dtd.element_declarations.at(element_name).type == ElementType::empty
                ) {
                    throw;
                }
                // All notation names in the declaration must be declared.
                if (!std::all_of(ad.notations.begin(), ad.notations.end(), [&dtd](const String& notation) {
                    return dtd.notation_declarations.count(notation);
                })) {
                    throw;
                }
            }
            if (ad.has_default_value) {
                validate_default_attribute_value(ad, dtd);
            }
        }
    }
}

void validate_default_attribute_value(
    const AttributeDeclaration& ad, const DoctypeDeclaration& dtd, const String* value_ptr
) {
    const String& value = value_ptr != nullptr ? *value_ptr : ad.default_value;
    switch (ad.type) {
        case AttributeType::id:
        case AttributeType::idref:
        case AttributeType::entity:
            // Default value must match Name.
            if (!valid_name(value, true)) {
                throw;
            }
            break;
        case AttributeType::idrefs:
        case AttributeType::entities:
            // Default value must match Names.
            if (!valid_names(value)) {
                throw;
            }
            break;
        case AttributeType::nmtoken:
            // Default value must match Nmtoken.
            if (!valid_nmtoken(value)) {
                throw;
            }
            break;
        case AttributeType::nmtokens:
            // Default value must match Nmtokens.
            if (!valid_nmtokens(value)) {
                throw;
            }
            break;
        case AttributeType::notation:
            // Default value must be one of the notations in the enumeration list.
            if (!ad.notations.count(value)) {
                throw;
            }
            break;
        case AttributeType::enumeration:
            // Default value must be one of the enumeration values.
            if (!ad.enumeration.count(value)) {
                throw;
            }
            break;
    }
}

void validate_attributes(
    const Element& element, const DoctypeDeclaration& dtd, const std::set<String>& ids
) {
    if (!dtd.attribute_list_declarations.count(element.tag.name)) {
        // No attlist declaration - element must have no attributes or else error.
        if (!element.tag.attributes.empty()) {
            throw;
        }
    } else {
        const AttributeListDeclaration& ald = dtd.attribute_list_declarations.at(element.tag.name);
        const Attributes& attributes = element.tag.attributes;
        int registered = 0;
        auto is_unparsed_entity = [&dtd](const String& value) {
            return dtd.general_entities.count(value) && dtd.general_entities.at(value).is_unparsed;
        };
        for (const auto& [attribute_name, ad] : ald) {
            if (!attributes.count(attribute_name)) {
                if (ad.presence == AttributePresence::implied) {
                    // Implied allows attribute to be omitted.
                    continue;
                }
                // Attribute not in element and not implied => REQUIRED failed => invalid.
                throw;
            }
            registered++;
            const String& value = attributes.at(attribute_name);
            if (ad.presence == AttributePresence::fixed) {
                // The attribute must always have the default value.
                if (value != ad.default_value) {
                    throw;
                }
            } else {
                // Need to check basic validity: based on default value validation.
                // Only required if not FIXED (Fixed default value already validated).
                validate_default_attribute_value(ad, dtd, &value);
            }
            // Further validation now that this a real attribute value.
            switch (ad.type) {
                case AttributeType::idref:
                    // IDREF values must match the value of some ID attribute in doc.
                    if (!ids.count(value)) {
                        throw;
                    }
                    break;
                case AttributeType::idrefs: {
                    // All IDREEFs must match the value of some ID attribute in doc.
                    String idref;
                    for (Char c : value) {
                        if (c == SPACE) {
                            if (!ids.count(idref)) {
                                throw;
                            }
                            idref.clear();
                        } else {
                            idref.push_back(c);
                        }
                    }
                    if (!ids.count(idref)) {
                        throw;
                    }
                    break;
                }
                case AttributeType::entity:
                    // Must match the name of an unparsed entity declared in the DTD.
                    if (!is_unparsed_entity(value)) {
                        throw;
                    }
                    break;
                case AttributeType::entities: {
                    // Must all match the name of declared unparsed entity.
                    String unparsed_entity;
                    for (Char c : value) {
                        if (c == SPACE) {
                            if (!is_unparsed_entity(unparsed_entity)) {
                                throw;
                            }
                            unparsed_entity.clear();
                        } else {
                            unparsed_entity.push_back(c);
                        }
                    }
                    if (!is_unparsed_entity(unparsed_entity)) {
                        throw;
                    }
                    break;
                }
            }
        }
        if (registered < ald.size()) {
            // Excess attributes that are not declared. Invalid.
            throw;
        }
    }
    for (const Element& child : element.children) {
        validate_attributes(child, dtd, ids);
    }
}

void parse_and_validate_ids(const Element& element, const DoctypeDeclaration& dtd, std::set<String>& ids) {
    if (dtd.attribute_list_declarations.count(element.tag.name)) {
        for (const auto& [attribute_name, ad] : dtd.attribute_list_declarations.at(element.tag.name)) {
            if (ad.type == AttributeType::id) {
                if (element.tag.attributes.count(attribute_name)) {
                    const String& id = element.tag.attributes.at(attribute_name);
                    if (ids.count(id)) {
                        // Repeated ID values in document forbidden.
                        throw;
                    }
                    ids.insert(id);
                }
                // Only one ID attribute max.
                break;
            }
        }
    }
    for (const Element& child : element.children) {
        parse_and_validate_ids(child, dtd, ids);
    }
}

}