# XML Parser - Setup/Usage Guide

## Setup
Before getting started, please note the following requirements about the parser:
- The parser has been written in the C++ programming language. The minimum version of C++ required to use the parser is **C++17** due to the fact that C++17 features have been used, such as structured bindings and the `std::filesystem` library.
- It is assumed you have a suitable C++ compiler installed. This project has compiled and worked successfully on g++ v12.1.0, but should work on any standards compiler supporting C++17.
- Of course, understanding of XML is assumed. Please refer to online resources if you are unsure about how XML works because that is beyond the scope of this guide.

Provided these requirements have been satisfied, here is how this parser can be used in a C++ program:
- Download the files in the `src` folder and keep these files in a suitable location. All header (`.h`) files must be kept in the same folder.
- The only public header that is designed for direct use is `xml.h`.
- When compiling, include all the `.cpp` files of the parser in the compilation process.

That's all you really need to know about setting up the parser to be used in a project. Next up is how to actually use the parser in source code.

## Usage

Before beginning, note that all functions, classes and constants of the library are wrappedin the `xml` namespace, to avoid name collisions.

### Input
The relevant header to include is the `src/xml.h` header, which contains a function called `xml::parse` with two overloads.

There are two ways of inputting XML data into the parser:
1. A `std::string` object that contains all the XML data to parse, where parsing will start from the first character and conclude after the last character of the string.
2. A `std::istream` object that contains XML data, where parsing will start from the initial stream position and conclude when the end of the stream is reached. Note that polymorphism is allowed, for example, a `std::ifstream` (input file stream) can be provided.

The `xml::parse` function accepts 3 parameters, and in position order, these parameters are:
1. Either a `const std::string&` or `std::istream&` object (polymorphism acceptable).
2. A Boolean indicating whether to validate all elements to their ELEMENT declarations (true by default). For more information on what an ELEMENT declaration is, see: https://www.w3.org/TR/xml/#elemdecls
3. A Boolean indicating whether to validate attributes of all elements based on ATTLIST declarations (true by default). For more information on what an ATTLIST declaration is, see: https://www.w3.org/TR/xml/#attdecls

### Process
Once the `xml::parse` function is called, the parsing begins. All parsing will be in accordance with the standard as per https://www.w3.org/TR/xml, except the limitations as seen in the README document.

In terms of validation performed, note the following:
- If there is no DOCTYPE declaration, no validation takes place, since it is impossible to have a 'valid' document with no DTD. However, do not worry about this since validity is not a requirement, only well-formedness. In general, validation is good since it promotes data integrity, but it is simply irrelevant whenever no DTD is available. Indeed, many XML documents do not contain a DTD since it is seen to be unnecessary in many scenarios.
- If a DOCTYPE declaration exists, all validation as seen in the standard will be performed and parsing will fail if such a document is not valid. However, note the following:
    - If element validation is disabled, then elements in the document will not be validated against ELEMENT declarations in the DTD.
    - If attribute validation is disabled, then attributes of elements in the document will not be validated against ATTLIST declarations in the DTD.

### Output
Output takes the form of a single `xml::Document` object. The object has several public attributes (freely modifiable if needed since the object does not need to used later by the parser).

Before explaining each attribute of `xml::Document`, there are several typedefs and utility classes used:
- `xml::Char` is a typedef of type `int`, ensuring all UTF-8 characters can be properly represented.
- `xml::String` is inherited from `std::vector<xml::Char>`, ensuring a UTF-8 string is available. Additionally, it can be cast to a `std::string` object if needed.
- `xml::ExternalID` - an external ID in the document, such as in an external ID declaration:
    - `type` (type `xml::ExternalIDType`) - the type of external ID, one of: `xml::ExternalIDType::system` (SYSTEM), `xml::ExternalIDType::public_` (PUBLIC) or `xml::ExternalIDType::none` (no external ID provided).
    - `system_id` (type `std::filesystem::path`) - the system ID (not relevant if `type` is `xml::ExternalIDType::none`).
    - `public_id` (type `std::filesystem::path`) - the public ID (only relevant if `type` is `xml::ExternalIDType::public_`).
- `xml::ProcessingInstruction` - a processing instruction:
    - `target` (type `xml::String`) - name of the target application the instruction is directed to.
    - `instruction` (type `xml::String`) - contents of `the instruction.
- `xml::ElementDeclaration` - an ELEMENT declaration in the DTD:`
    - `name` (type `xml::String`) - the name of the elmeent the element declaration relates to.
    - `type` (type `xml::ElementType`) - the type of element. One of: `xml::ElementType::` `empty`, (no content) `any`, (any content) `mixed`, (character data and certain child elements) `children` (child elements only).
    - `element_content` (type `xml::ElementContentModel`) - the element content model (only relevant if element is of type `xml::ElementType::children`).
        - `count` (type `xml::ElementContentCount`) - allowed consecutive frequency of the element content. One of: `xml::ElementContentCount::` `one`, `zero_or_more` (*), `zero_or_one` (?), `one_more_more` (+).
        - `is_name` (type `bool`) - whether the element content is at a leaf - where a single element name is to be matched.
        - `name` (type `xml::String`) - the name of the element to match (only relevant if `is_name` is `true`).
        - `is_sequence` (type `bool`) - whether sub-contents must all occur in order (comma-separated) if true, or one sub-content must be matched (bar-separated) if false. Only relevant if `is_name` is `false`.
        - `parts` (type `std::vector<xml::ElementContentModel>`) - the sub-element content models only for use if `is_name` is `false`.
    - `mixed_content` (type `xml::MixedContentModel`) - the mixed content model (only relevant if element is of type `xml::ElementType::mixed`):
        - `choices` (type `std::set<xml::String>`) - the allowed child element names.
- `xml::AttributeDeclaration` - declaration info for a single attribute for a given element type:
    - `name` (type `xml::String`) - name of the attribute.
    - `type` (type `xml::AttributeType`) - type of the attribute. One of: `xml::AttributeType::` `cdata`, `id`, `idref`, `idrefs`, `entity`, `entities`, `nmtoken`, `nmtokens`, `notation`, `enumeration`.
    - `presence` (type `xml::AttributePresence`) - presence requirements of the attribute. One of `xml::AttributePresence::` `required` (attribute must be present), `implied` (optional attribute), `fixed` (attribute must take fixed value), `relaxed` (optional attribute with default).
    - `notations` (type `std::set<xml::String>`) - the notation names, only relevant if `type` is `xml::AttributeType::notation`.
    - `enumeration` (type `std::set<xml::String>`) - the enumeration values, only relevant if `type` is `xml::AttributeType::enumeration`.
    - `has_default_value` (type `bool`) - whether a default value has been specified for the attribute.
    - `default_value` (type `xml::String`) - the default value of the attribute, if `has_default_value` -s `true`.
    - `from_external` (type `bool`) - whether the attribute was declared in the external DTD subset or an external parameter entity.
- `xml::AttributeListDeclaration` - a typedef of type `std::map<xml::String, xml::AttributeDeclaration>` - maps attribute name to its corresponding attribute declaration.
- `xml::Entity` - base class containing attributes common to both general and parameter entities:
    bool from_external = false; 
    - `name` (type `xml::String`) - the name of the entity.
    - `value` (type `xml::String`) - the value (text) of the entity (will be empty if the entity is external).
    - `is_external` (type `bool`) - `true` indicates the entity is external, `false` indicates the entity is internal.
    - `external_id` (type `xml::ExternalID`) - the external ID, only for if the entity is external.
    - `from_external` (type `bool`) - whether the entity was declared in the external DTD subset or an external parameter entity.
- `xml::GeneralEntity` - represents a general entity, inherited from `xml::Entity` and contains the followwing additional attributes:
    - `is_unparsed` (type `bool`) - whether the entity is an unparsed entity, in which case `value` will be empty.
    - `notation_name` (type `xml::String`) - the name of the notation (only if the entity is unparsed).
- `xml::ParameterEntity` - represents a parameter entity, inherited from `xml::Entity`, and contains no additional attributes.
- `xml::NotationDeclaration` - a NOTATION declaration in the DTD:
    - `name` (type `xml::String`) - the name of the notation.
    - `has_system_id` (type `bool`) - whether the notation has a system ID.
    - `has_public_id` (type `bool`) - whether the notation has a public ID.
    - `system_id` (type `xml::String`) - the system ID, if available.
    - `public_id` (type `xml::String`) - the public ID, if available.
- `xml::ElementDeclarations` - a typedef of type `std::map<xml::String, xml::ElementDeclaration>`, mapping each element name to its corresponding declaration.
- `xml::AttributeListDeclarations` - a typedef of type `std::map<xml::String, xml::AttributeListDeclaration>`, mapping each element name to its corresponding attribute list.
- `xml::GeneralEntities` - a typedef of type `std::map<xml::String, xml::GeneralEntity>`, mapping each general entity name to its corresponding object.
- `xml::ParameterEntities` - a typedef of type `std::map<xml::String, xml::ParameterEntity>`, mapping each parameter entity name to its corresponding object.
- `xml::NotationDeclarations` - a typedef of type `std::map<xml::String, xml::NotationDeclaration>`, mapping each notation name to its corresponding declaration.
- `xml::Element` - represents an element in the document.
    - `text` (type `xml::String`) - all character data in the element, excluding text in child elements.
    - `tag` (type `xml::Tag`) - info about the element seen in the start tag of the element, or empty tag.
        - `name` (type `xml::String`) - the tag/element name.
        - `attributes` (type `xml::Attribute` which is typedef of `std::map<xml::String, xml::String>`) - attribute names mapped to corresponding values.
        - `tag_type` (type `xml::TagType`) - the tag type (either `xml::TagType::start` or `xml::TagType::empty` in this context).
    - `children` (type `std::vector<xml::Element>`) - list of child elements in order of occurrence.
    - `processing_instructions` (type `std::vector<xml::ProcessingInstruction>`) - list of processing instructions within the element in order of occurrence.
    - `is_empty` (type `bool`) - whether the element contains no content.
    - `children_only` (type `bool`) - whether the element contains child elements only (except interspersed whitespace).

And so, `xml::Document` consists of the following attributes:
- `version` (type `xml::String`) - the document version specified in the XML declaration. If there is no XML declaration, this attribute will have a value of `"1.0"`. Note, regardless of specified version, the document will have been parsed as per XML v1.0.
- `encoding` (type `xml::String`) - the document encoding in lower-case. Since only UTF-8 is currently supported by the parser, this attribute will always have a value of `"utf-8"`.
- `standalone` (type `bool`) - indicates whether the document has been specified as `standalone="yes"` in the XML declaration. By default, a document is not considered standalone, hence will have a value of `false` by default. For further information, see: https://www.w3.org/TR/xml/#sec-rmd
- `doctype_declaration` (type `DoctypeDeclaration`) - information about the DOCTYPE declaration of the document.
    - `exists` (type `bool`) - whether a DTD exists in the document. If `false`, all other attributes are irrelevant.
    - `root_name` (type `xml::String`) - the name of the root element.
    - `external_id` (type `xml::ExternalID`) - the external ID to the external DTD subset.
    - `processing_instructions` (type `std::vector<xml::ProcesssingInstruction>`) - a list of processing instructions in parse-order.
    - `element_declarations` (type `xml::ElementDeclarations`)
    - `attribute_list_declarations` (type `xml::AttributeListDeclarations`)
    - `general_entities` (type `xml::GeneralEntities`)
    - `parameter_entities` (type `xml::ParameterEntities`)
    - `notation_declarations` (type `xml::NotationDeclarations`)
- `root` (type `xml::Element`) - the root element of the document.
- `processing_instructions` (type `std::vector<xml::ProcessingInstruction>`) - a list of processing instructions that occur in the toplevel of the document.

### Errors
Whenever an error occurs and is thrown by the parser, it will be of the type `xml::XmlError`, which is inherited from `std::runtime_error`.

A suitable error message will be generated, with information on the line number and position where appropriate and possible.

Note, if an error other than type `xml::XmlError` occurs, then it was not raised explicitly by the parser and can be considered a bug.

Finally, if a `std::istream` is passed in for parsing, then if no error occurs during parsing other than post-validation failure, the stream will be at the end. However, if an error occurs during the actual parsing of characters (reading), the stream will be at an arbitrary position, so be careful.

### Examples
Examples of the XML parser in action can be seen by looking at some of the tests available in the `test/test_document.cpp` and `test/test_document_files.cpp`. Overall, play around with the parser and hopefully it should be fairly intuitive to use.