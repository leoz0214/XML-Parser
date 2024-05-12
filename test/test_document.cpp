// Tests the functionality of entire document parsing.
#include <cassert>
#include <functional>
#include <string>
#include <iostream>
#include "../src/parser.h"


using namespace xml;


typedef std::function<void(const Document&)> TestDocument;
unsigned test_number = 0;
void test_document(const std::string& string, TestDocument callback) {
    Document document = Parser(string).parse_document();
    callback(document);
    std::cout << "Document Test " << test_number++ << " passed.\n";
}


int main() {
    test_document("<?xml version='1.0'?><a>Sanity Check</a>", [](const Document& document) {
        assert((document.version == String("1.0")));
        assert((document.root.tag.name == String("a")));
        assert((document.root.text == String("Sanity Check")));
    });
    test_document(R"(<?xml    version = "1.234"  encoding="UtF-8" standalone = 'yes' ?>
        <!-- If this document passes - it is a good sign overall ! ! ! -->
        <?xml2 goodluck?><!----><!----><!----><!----><!----><!----><!----><!---->
        <?xml3 greatluck?>
        <root   category="food"  shopid="441">
            <item name="cookie" price="0.59"/>
            <item name="salmon" price="2.25"/>
            <discounted>
                <!-- Going going gone -->
                <item name="cake" disc-price="3.49" price="4.99"/>
                <item price = "0.09" disc-price="0.04"  name= "gum" />    
            </discounted>
        </root >   <!-- END OF DOC --> <?xml4 final?>
    )", [](const Document& document) {
        assert((document.version == String("1.234")));
        assert((document.encoding == String("utf-8")));
        assert((document.standalone == true));
        assert((document.doctype_declaration.exists == false));
        assert((document.processing_instructions.size() == 3));
        assert((document.processing_instructions.at(1).target == String("xml3")));
        assert((document.root.tag.attributes.at("shopid") == String("441")));
        const auto& children = document.root.children;
        assert((children.size() == 3));
        assert((children.at(0).tag.name == String("item")));
        assert((children.at(1).tag.attributes.at("name") == String("salmon")));
        assert((children.at(2).children.size() == 2));
        assert((children.at(2).children.at(1).tag.attributes.at("disc-price") == String("0.04")));
    });
    test_document(R"(<?xml version='1.0' encoding="utf-8"?>
        <!DOCTYPE     root  PUBLIC   "'public id 123!'.xml" 'systemid321.lmx' >
        <root >Hi</root><!-- DOCTYPE-SANITY-CHECK -->
    )", [](const Document& document) {
        DoctypeDeclaration dtd = document.doctype_declaration;
        assert((dtd.exists));
        assert((dtd.root_name == String("root")));
        assert((dtd.external_id.type == ExternalIDType::public_));
        assert((dtd.external_id.public_id == String("'public id 123!'.xml")));
        assert((dtd.external_id.system_id == String("systemid321.lmx")));
    });
    test_document("<!DOCTYPE x SYSTEM 'y'><x></x>", [](const Document& document) {
        assert((document.doctype_declaration.external_id.type == ExternalIDType::system));
        assert((document.doctype_declaration.external_id.system_id == String("y")));
    });
    test_document("<!DOCTYPE minimal><minimal></minimal>", [](const Document& document) {
        assert((document.doctype_declaration.external_id.type == ExternalIDType::none));
        assert((document.root.text.empty()));
    });
    test_document(R"(<!DOCTYPE r PUBLIC 'p' 's' [
        <!-- Internal DTD basic checking... this should pass -->
            <?doc-pi DoctypePI?>
        <!--END OF DOCUMENT TYPE DECLARATION SECTION-->
    ]>
        <r>Internal DTD Subset Sanity Check.</r>
    )", [](const Document& document) {
        auto dtd = document.doctype_declaration;
        assert((dtd.external_id.system_id == String("s")));
        assert((dtd.processing_instructions[0].target == String("doc-pi")));
        assert((document.root.tag.name == String("r")));
    });
    test_document(R"(<?xml version='1.0' encoding='utf-8'?>
        <!DOCTYPE root [
            <!ELEMENT e EMPTY>
            <!ELEMENT    a    ANY    >
            <!ELEMENT spec (front, body, back?)>
            <!ELEMENT div1 ( head, (  p | list+ | note)*, div2*)>
            <!ELEMENT  p (#PCDATA|a|ul|b|i|em)*>
            <!ELEMENT b       ( #PCDATA )>
        ]><root>Testing element entity declarations</root>
    )", [](const Document& document) {
        auto element_decls = document.doctype_declaration.element_declarations;
        assert((element_decls.size() == 6));
        assert((element_decls.at("e").type == ElementType::empty));
        assert((element_decls.at("a").type == ElementType::any));
        ElementDeclaration spec = element_decls.at("spec");
        assert((spec.type == ElementType::children));
        assert((spec.element_content.is_sequence));
        assert((spec.element_content.parts.size() == 3));
        assert((spec.element_content.parts.at(0).is_name));
        assert((spec.element_content.parts.at(0).name == String("front")));
        assert((spec.element_content.parts.at(0).count == ElementContentCount::one));
        assert((spec.element_content.parts.at(2).count == ElementContentCount::zero_or_one));
        ElementDeclaration div1 = element_decls.at("div1");
        assert((div1.element_content.parts.size() == 3));
        ElementContentModel div1_1 = div1.element_content.parts.at(1);
        assert((div1_1.count == ElementContentCount::zero_or_more));
        assert((div1_1.parts.size() == 3));
        assert((div1_1.parts.at(1).name == String("list")));
        assert((div1_1.parts.at(1).count == ElementContentCount::one_or_more));
        ElementDeclaration p = element_decls.at("p");
        assert((p.type == ElementType::mixed));
        assert((p.mixed_content.choices.size() == 5));
        assert((element_decls.at("b").mixed_content.choices.empty()));
    });
    test_document(R"(<!DOCTYPE root SYSTEM "sys/a" [
        <!NOTATION n1    SYSTEM "Notation1">
        <!NOTATION n2 PUBLIC "Notation2" 'N2'>
        <!NOTATION n3 PUBLIC "Notation3">]><root> </root>
    )", [](const Document& document) {
        auto notations = document.doctype_declaration.notation_declarations;
        assert((notations.size() == 3));
        assert((notations.at("n1").has_system_id));
        assert((!notations.at("n1").has_public_id));
        assert((notations.at("n1").system_id == String("Notation1")));
        assert((notations.at("n2").has_public_id));
        assert((notations.at("n2").has_system_id));
        assert((notations.at("n2").public_id == String("Notation2")));
        assert((notations.at("n2").system_id == String("N2")));
        assert((!notations.at("n3").has_system_id));
        assert((notations.at("n3").public_id == String("Notation3")));
    });
}