// Testing document, but loaded from XML files.
/*
!!!MUST!!! BE
RUN FROM THE ROOT OF THE PROJECT.
*/
#include <cassert>
#include <functional>
#include <string>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <stdexcept>
#include "../src/parser.h"


using namespace xml;


const std::string FOLDER = "test/test_files";


typedef std::function<void(const Document&)> TestDocument;
unsigned test_number = 0;
void test_document_file(const std::string& filename, TestDocument callback) {
    std::string file_path = FOLDER + "/" + filename;
    std::ifstream file(file_path);
    if (!file.is_open()) {
        throw std::runtime_error(filename + " not found");
    }
    Document document = Parser(file).parse_document();
    callback(document);
    std::cout << "Document File Test " << test_number++ << " passed.\n";
}


int main() {
    test_document_file("sanity.xml", [](const Document& document) {
        assert((document.root.tag.name == String("body")));
        assert((document.root.tag.attributes.at("title") == String("XML File Test")));
        auto children = document.root.children;
        assert((children.size() == 3));
        assert((children.at(0).text == String("XML FILE TEST")));
        assert((children.at(1).text == String("XML is powerful")));
        assert((children.at(2).tag.name == String("metadata")));
        assert((children.at(2).tag.type == TagType::empty));
    });
    test_document_file("external.xml", [](const Document& document) {
        assert((document.doctype_declaration.general_entities.at("external").is_external));
        assert((document.root.children.size() == 4));
        assert((document.root.children.at(0).tag.name == String("example")));
        assert((document.root.children.at(1).tag.attributes.at("att") == String("yes")));
        assert((document.root.children.at(2).text == String("Embed external entity!!!")));
        Element child = document.root.children.at(3);
        assert((child.children.size() == 1));
        assert((child.children.at(0).text == String("&&&")));
    });
    test_document_file("parameter.xml", [](const Document& document) {
        assert((document.root.text.size() == 24));
        assert((document.doctype_declaration.processing_instructions.at(0).target
            == String("param-pi")));
    });
    test_document_file("element_decls.xml", [](const Document& document) {
        auto elements = document.doctype_declaration.element_declarations;
        assert((elements.at("my-element").element_content.parts.size() == 3));
        assert((elements.at("my-element").element_content.parts.at(2).count
            == ElementContentCount::zero_or_one));
        assert((elements.at("ex2").element_content.is_sequence == false));
        assert((elements.at("ex2").element_content.parts.at(0).name == String("abc")));
        assert((elements.at("widgets").element_content.count == ElementContentCount::zero_or_more));
        assert((elements.at("p").mixed_content.choices.size() == 5));
        assert((elements.at("metadata").type == ElementType::empty));
    });
    test_document_file("attlist_decl.xml", [](const Document& document) {
        AttributeListDeclaration decl = 
            document.doctype_declaration.attribute_list_declarations.at("my-element");
        assert((decl.at("title").type == AttributeType::cdata));
        assert((decl.at("title").presence == AttributePresence::required));
        assert((decl.at("identifier").type == AttributeType::id));
        assert((decl.at("atts").default_value == String("abc def ghi")));
        assert((decl.at("level").enumeration.size() == 3));
    });
    test_document_file("entity_decls.xml", [](const Document& document) {
        auto p = document.doctype_declaration.parameter_entities;
        auto g = document.doctype_declaration.general_entities;
        assert((p.at("ccc").value == String("'This is ccc BTW")));
        assert((g.at("x").value == String("This is ccc BTW !!!!")));
        assert((g.at("abc").external_id.public_id == String(" ccc ")));
    });
    test_document_file("notation_decls.xml", [](const Document& document) {
        auto decls = document.doctype_declaration.notation_declarations;
        assert((decls.at("my-notation").public_id == String("Notation")));
        assert((decls.at("Example").system_id == String(" PUBLIC ")));
    });
}
