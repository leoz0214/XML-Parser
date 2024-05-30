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
        throw XmlError("Root element name does not match declared root element name in DTD");
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
        throw XmlError("Undeclared element: " + std::string(element.tag.name));
    }
    ElementDeclaration ed = element_declarations.at(element.tag.name);
    switch (ed.type) {
        case ElementType::any:
            // No content restriction whatsoever.
            break;
        case ElementType::empty:
            // Element must simply have no content.
            if (!element.is_empty) {
                throw XmlError(
                    "Element declared EMPTY but contains content: " + std::string(element.tag.name));
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
        throw XmlError(
            "Standalone document cannot have whitespace "
            "in element with element content: " + std::string(element.tag.name));
    }
    if (!element.children_only) {
        throw XmlError(
            "Element with element content must have "
            "child elements only: " + std::string(element.tag.name));
    }
    std::size_t pos = 0;
    // Note: must also ensure all elements covered (hence pos must equal child count in the end).
    if (!valid_element_content_helper(element, ecm, pos) || pos < element.children.size()) {
        throw XmlError(
            "Element did not match element content model: " + std::string(element.tag.name));
    }
}

void validate_mixed_content(const Element& element, const MixedContentModel& mcm) {
    // Simply ensure all child elements have accepted name.
    if (!std::all_of(element.children.begin(), element.children.end(), [&](const Element& child) {
        return mcm.choices.count(child.tag.name);
    })) {
        throw XmlError("Element did not match mixed content model: " + std::string(element.tag.name));
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
                    throw XmlError(
                        "ID attribute must be #IMPLIED or #REQUIRED: '"
                        + std::string(ad.name) + "' of element " + std::string(element_name));
                }
                // An element type must not have more than one ID attribute specified.
                if (id_attribute_seen) {
                    throw XmlError("Single ID attribute only: " + std::string(element_name));
                }
                id_attribute_seen = true;
            } else if (ad.type == AttributeType::notation) {
                // An element type must not have more than one NOTATION attribute specified.
                if (notation_attribute_seen) {
                    throw XmlError("Single notation attribute only: " + std::string(element_name));
                }
                notation_attribute_seen = true;
                // An attribute of type NOTATION must not be declared on an element declared EMPTY.
                if (
                    dtd.element_declarations.count(element_name)
                    && dtd.element_declarations.at(element_name).type == ElementType::empty
                ) {
                    throw XmlError(
                        "Notation attribute must not be declared "
                        "on an EMPTY element: " + std::string(element_name));
                }
                // All notation names in the declaration must be declared.
                if (!std::all_of(ad.notations.begin(), ad.notations.end(), [&dtd](const String& notation) {
                    return dtd.notation_declarations.count(notation);
                })) {
                    throw XmlError(
                        "All notation names must be declared: '"
                        + std::string(ad.name) + "' of element " + std::string(element_name));
                }
            }
            if (ad.has_default_value) {
                try {
                    validate_default_attribute_value(ad, dtd);
                } catch (const XmlError& e) {
                    throw XmlError(
                        "Default value error for attribute '" + std::string(ad.name) +
                        "' of element " + std::string(element_name) + ": " + e.what());
                }
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
            if (!valid_name(value, true)) {
                throw XmlError("Attribute value must match Name");
            }
            break;
        case AttributeType::idrefs:
        case AttributeType::entities:
            // Attribute value must match Names.
            if (!valid_names(value)) {
                throw XmlError("Attribute value must match Names");
            }
            break;
        case AttributeType::nmtoken:
            // Attribute value must match Nmtoken.
            if (!valid_nmtoken(value)) {
                throw XmlError("Attribute value must match Nmtoken");
            }
            break;
        case AttributeType::nmtokens:
            // Attribute value must match Nmtokens.
            if (!valid_nmtokens(value)) {
                throw XmlError("Attribute value must match Nmtokens");
            }
            break;
        case AttributeType::notation:
            // Attribute value must be one of the notations in the enumeration list.
            if (!ad.notations.count(value)) {
                throw XmlError("Attribute value must match one of the notation names");
            }
            break;
        case AttributeType::enumeration:
            // Attribute value must be one of the enumeration values.
            if (!ad.enumeration.count(value)) {
                throw XmlError("Attribute value must match one of the enumeration values");
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
            throw XmlError(
                "Element with attribute must have ATTLIST declaration: " + std::string(element.tag.name));
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
                throw XmlError(
                    "REQUIRED attribute '" + std::string(attribute_name) +
                    "' not specified in element: " + std::string(element.tag.name));
            }
            registered++;
            const String& value = attributes.at(attribute_name);
            if (ad.presence == AttributePresence::fixed) {
                // The attribute must always have the default value.
                if (value != ad.default_value) {
                    throw XmlError(
                        "FIXED attribute '" + std::string(attribute_name) +
                        "' does not match default value in element: " + std::string(element.tag.name));
                }
            } else {
                // Need to check basic validity: based on default value validation.
                // Only required if not FIXED (Fixed default value already validated).
                try {
                    validate_default_attribute_value(ad, dtd, &value);
                } catch (const XmlError& e) {
                    throw XmlError(
                        "Value error for attribute '" + std::string(ad.name) +
                        "' of element " + std::string(element.tag.name) + ": " + e.what());
                }
            }
            // Further validation now that this a real attribute value.
            std::string error_details;
            switch (ad.type) {
                case AttributeType::idref:
                    if (!ids.count(value)) {
                        error_details = "IDREF value must match an ID value in the document";
                    }
                    break;
                case AttributeType::idrefs: {
                    String idref;
                    for (Char c : value) {
                        if (c == SPACE) {
                            if (!ids.count(idref)) {
                                error_details =
                                    "All IDREFS values must match an ID value in the document";
                                goto done;
                            }
                            idref.clear();
                        } else {
                            idref.push_back(c);
                        }
                    }
                    if (!ids.count(idref)) {
                        error_details = "All IDREFS values must match an ID value in the document";
                    }
                    break;
                }
                case AttributeType::entity:
                    if (!is_unparsed_entity(value)) {
                        error_details = "ENTITY value must match the name of a declared unparsed entity";
                    }
                    break;
                case AttributeType::entities: {
                    // Must all match the name of declared unparsed entity.
                    String unparsed_entity;
                    for (Char c : value) {
                        if (c == SPACE) {
                            if (!is_unparsed_entity(unparsed_entity)) {
                                error_details =
                                    "All ENTITIES values must match "
                                    "the name of a declared unparsed entity";
                                goto done;
                            }
                            unparsed_entity.clear();
                        } else {
                            unparsed_entity.push_back(c);
                        }
                    }
                    if (!is_unparsed_entity(unparsed_entity)) {
                        error_details =
                            "All ENTITIES values must match the name of a declared unparsed entity";
                    }
                    break;
                }
            }
            done:
            if (!error_details.empty()) {
                throw XmlError(
                    "Value error for attribute '" + std::string(ad.name) +
                    "' of element " + std::string(element.tag.name) + ": " + error_details);
            }
        }
        if (registered < attributes.size()) {
            // Excess attributes that are not declared. Invalid.
            throw XmlError("Undeclared attributes found in element: " + std::string(element.tag.name));
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
                        throw XmlError("Repeated ID value: '" + std::string(id) + "'");
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