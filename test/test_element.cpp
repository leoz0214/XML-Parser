// Tests the functionality of simple element parsing.
#include <cassert>
#include <functional>
#include <string>
#include <iostream>
#include "../src/parser.h"


using namespace xml;


typedef std::function<void(const Element&)> TestElement;
unsigned test_number = 0;
void test_element(const std::string& string, TestElement callback) {
    Element element = Parser(string).parse_element();
    callback(element);
    std::cout << "Element Test " << test_number++ << " passed.\n";
}


int main() {
    test_element("<test1>Hello, world</test1>", [](const Element& element) {
        assert((element.tag.name == (String)"test1"));
        assert((element.tag.type == TagType::start));
        assert((element.tag.attributes.empty()));
        assert((element.children.empty()));
        assert((element.text.size() == 12));
    });
    test_element(R"(<:a:  at1='"' at2="ABC"      />)", [](const Element& element) {
        assert((element.tag.name == String{':', 'a', ':'}));
        assert((element.tag.type == TagType::empty));
        assert((element.text.empty()));
        assert((element.tag.attributes.size() == 2));
        assert((element.tag.attributes.at("at1") == String{'"'}));
        assert((element.tag.attributes.at("at2") == String("ABC")));
    });
    test_element(R"(<root level    =     "2">
                    <item price ="2.25">Ruler</item>
               <item price= "10.22"      >Mouse</item   >
            <item    price="244.55">SmartWatch4000</item>
        </root>
    )", [](const Element& element) {
        assert((element.tag.attributes.at("level") == String("2")));
        assert((element.children.size() == 3));
        for (const auto& child : element.children) {
            assert((child.tag.name == String("item")));
            assert((child.tag.attributes.count("price")));
        }
    });
    test_element("<a><b><c><d><e><f><g></g></f></e></d></c></b></a>", [](const Element& element) {
        assert((element.children.at(0).children.at(0).children.at(0).tag.name == String("d")));
    });
    test_element("<x>Test <!--Comment-Comment-Comment-->123</x>", [](const Element& element) {
        assert((element.text == String("Test 123")));
    });
    test_element("<:>XML Parsing is <![CDATA[Fun & Painful!]]>!!!</:>", [](const Element& element) {
        assert((element.text == String("XML Parsing is Fun & Painful!!!!")));
    });
    test_element("<h>abc<?x-spreadsheet a='123?'?>def<?tgt?></h>", [](const Element& element) {
        assert((element.text == String("abcdef")));
        assert((element.processing_instructions[0].target == String("x-spreadsheet")));
        assert((element.processing_instructions[0].instruction == String("a='123?'")));
        assert((element.processing_instructions[1].target == String("tgt")));
        assert((element.processing_instructions[1].instruction.empty()));
    });
    test_element(R"(<t1>
        <!-- This is a test - with lots of stuff together! -->
        <t2 id="123"><!----><![CDATA[1 & 1 = 1]]>!</t2>
                <?ins anything?>
            </t1>
    )", [](const Element& element) {
        Element child = element.children[0];
        assert((child.tag.name == String("t2")));
        assert((child.tag.attributes.at("id") == String("123")));
        assert((child.text == String("1 & 1 = 1!")));
        assert((element.processing_instructions[0].target == String("ins")));
    });
}