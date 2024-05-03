# XML Parser

XML (Extensible Markup Language) is a powerful data format allowing data to be stored in a structured way. Custom tags can be used, and elements can contain text, attributes and indeed, child elements. There are also many more complex constructs and concepts such as entities, with various types of entities too.

The target of this project is to implement a parser for XML v1.0 that meets the following standard as closely as reasonable for this educational project: https://www.w3.org/TR/xml. There is a huge number of edge cases to consider, making it a non-trivial task to parse XML.

Nonetheless, it is worth a try. Features/considerations include but are far from limited to:
- XML data input through a string or stream.
- Start tags, end tags, empty tags
- Attributes
- Character data, child elements, comments
- Various declarations
- Entities
- Error handling
- Upon successful parsing, access to element text, attributes and child elements.

Hopefully, a solid parser can be developed in 3-4 weeks, given XML is difficult but not impossible to successfully parse.