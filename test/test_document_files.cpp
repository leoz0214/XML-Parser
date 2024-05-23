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
}
