# XML Parser

## Introduction

**XML** (Extensible Markup Language) is a powerful data format allowing data to be stored in a structured way. Custom tags can be used, and elements can contain text, attributes and indeed, child elements. There are also many more complex constructs and concepts such as entities, with various types of entities too.

The target of this project is to implement a **validating parser** for **XML v1.0** that meets the following standard as closely as reasonable for this educational project: https://www.w3.org/TR/xml. There is a huge number of edge cases to consider, making it a non-trivial task to parse XML robustly. 

Nonetheless, the project has succeeded, with rigorous testing to the point where the vast majority of the standard has proven to be adhered to. The parser is not 100% conforming, but around 95%. There are some aspects of the standard that are too demanding to properly implement in this parser (discussed later in this document), but the omission of features has been minimised such that this parser is still generally usable.

## Features
This XML parser is implemented in **C++** and is designed to be used in C++ projects that do not demand fast XML parsing performance. Performance is reasonable, but not amazing.

Features of XML that are supported by this parser include but are far from limited to:
- XML data input through a **string or stream**.
- Handling start tags, end tags, empty tags
- **Attribute** name/value pairs.
- Character data, child elements, comments, processing instructions.
- The **DOCTYPE declaration**
    - Internal/external subset
    - Element, attribute list, entity, notation declarations.
    - Conditional sections
- **Character references, general entities, parameter entities**.
- **Error** detection and handling
- **Validation**
- Upon successful parsing, access to element text, attributes and child elements, alongside any other collected data during parsing.

Note: this project only focuses on XML parsing, not XML generation.

## Limitations
Whilst this is a solid XML parser implementation, there are known limitations that must be acknowledged if you decide to use this parser seriously. Ensure these issues do not involve the XML data you decide to input into the parse, otherwise do not use the parser if these limitations will cause problems.

Perhaps a future version of the parser may improve standards conformance, but for now, note the existence of problems.

These issues include, but may not be limited to:
- **Only UTF-8 is supported by the parser**, only expecting 'UTF-8' in any encoding declaration (case-insensitive). As a result, UTF-8 encoding is assumed. Note that ASCII is a subset of UTF-8, so ASCII encoded XML files are just fine. No special characters are recognised, such as byte order marks. A sequence of valid UTF-8 characters is expected. UTF-16 is unsupported despite being mandated by the standard, for it is an archaic and poor encoding.
- Public and system IDs can be present. **Only system IDs are used by the parser to open external resources**. Importantly, this parser **only supports files as external resources, not URLs**, and for any relative paths being opened, these paths will be opened relative to the **current working directory** of the program. This is a significant limitation - be careful when considering how external resources are handled.
- **Namespaces are not supported by the parser** (the XML Namespaces specification is separated from the main specification and not implemented).

## Disclaimer
This XML parser can be included freely in any project and the source code can be freely read and modified without restriction.

However, there is NO LIABILITY for any DAMAGE caused by use of this parser. Use at your own risk.

One possible advantage of this parser is that the XML data can be input as a string or stream and parsed by passing the data into a single function that returns all data about the document. This makes the parser simple to use.

However, performance is not great, alongside possibly security risks and unresolved bugs. Remember the limitations already mentioned. **If in doubt, please do not use this parser because as already mentioned, this is an educational project for serious parsing practice.**

## What Next
If you are interested in using this parser or learning more about it, please refer to the [guide](GUIDE.md). This guide will provide setup information and how to use the parser and access the data after successful parsing. 