// Tests the functionality of entire document parsing.
#include <cassert>
#include <functional>
#include <string>
#include <iostream>
#include "../src/parser.h"


using namespace xml;


typedef std::function<void(const Document&)> TestDocument;
unsigned test_number = 0;
void test_document(
    const std::string& string, TestDocument callback,
    bool validate_elements = true, bool validate_attributes = true
) {
    Document document = Parser(string).parse_document(validate_elements, validate_attributes);
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
        <?abc2 goodluck?><!----><!----><!----><!----><!----><!----><!----><!---->
        <?abc3 greatluck?>
        <root   category="food"  shopid="441">
            <item name="cookie" price="0.59"/>
            <item name="salmon" price="2.25"/>
            <discounted>
                <!-- Going going gone -->
                <item name="cake" disc-price="3.49" price="4.99"/>
                <item price = "0.09" disc-price="0.04"  name= "gum" />    
            </discounted>
        </root >   <!-- END OF DOC --> <?abc4 final?>
    )", [](const Document& document) {
        assert((document.version == String("1.234")));
        assert((document.encoding == String("utf-8")));
        assert((document.standalone == true));
        assert((document.doctype_declaration.exists == false));
        assert((document.processing_instructions.size() == 3));
        assert((document.processing_instructions.at(1).target == String("abc3")));
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
    }, false);
    test_document("<!DOCTYPE x SYSTEM 'y'><x></x>", [](const Document& document) {
        assert((document.doctype_declaration.external_id.type == ExternalIDType::system));
        assert((document.doctype_declaration.external_id.system_id == String("y")));
    }, false);
    test_document("<!DOCTYPE minimal><minimal></minimal>", [](const Document& document) {
        assert((document.doctype_declaration.external_id.type == ExternalIDType::none));
        assert((document.root.text.empty()));
    }, false);
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
    }, false);
    test_document(R"(<?xml version='1.0' encoding='utf-8'?>
        <!DOCTYPE root [
            <!ELEMENT root ANY>
            <!ELEMENT e EMPTY>
            <!ELEMENT    a    ANY    ><!ELEMENT front ANY><!ELEMENT body ANY><!ELEMENT back ANY>
            <!ELEMENT spec (front, body, back?)>
            <!ELEMENT head ANY><!ELEMENT list ANY><!ELEMENT div2 ANY>
            <!ELEMENT div1 ( head, (  p | list+ | note)*, div2*, (no-deadlock?)*)>
            <!ELEMENT  p (#PCDATA|a|ul|b|i|em)*>
            <!ELEMENT b       ( #PCDATA )>
        ]><root>
            <!-- Testing the above entity declarations. -->
            <e/>
            <p><a>
                Can contain char data
                <spec><front/><body/></spec>
                <b>Can contain char data</b>
                <div1>
                    <head/><list/><list/><list/><div2/><div2/></div1>
            </a></p>
        </root>
    )", [](const Document& document) {
        auto element_decls = document.doctype_declaration.element_declarations;
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
        assert((div1.element_content.parts.size() == 4));
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
    }, false);
    test_document(R"(<?xml version='1.11' encoding='UTF-8'?>
        <!--Attribute list declarations sanity check.-->
        <!DOCTYPE root [
            <!ATTLIST termdef
                id      ID      #REQUIRED
                id      ID      #REQUIRED
                name    CDATA   #IMPLIED>
            <!ATTLIST  list type    ( bullets|ordered |glossary )  "    ordered   ">
            <!ATTLIST form method  CDATA   #FIXED 'POST' >
        ]><root><!---->
        <termdef id="id1" name="term"/><form method="POST"/><form/>
        <list type="glossary"/><list/>
        </root>
    )", [](const Document& document) {
        auto attlists = document.doctype_declaration.attribute_list_declarations;
        assert((attlists.size() == 3));
        AttributeListDeclaration termdef = attlists.at("termdef");
        assert((termdef.at("id").type == AttributeType::id));
        assert((termdef.at("id").presence == AttributePresence::required));
        assert((!termdef.at("id").has_default_value));
        assert((termdef.at("name").type == AttributeType::cdata));
        assert((termdef.at("name").presence == AttributePresence::implied));
        assert((!termdef.at("name").has_default_value));
        AttributeListDeclaration list = attlists.at("list");
        assert((list.at("type").type == AttributeType::enumeration));
        assert((list.at("type").enumeration.size() == 3));
        assert((list.at("type").enumeration.count("glossary")));
        assert((list.at("type").presence == AttributePresence::relaxed));
        assert((list.at("type").default_value == String("ordered")));
        AttributeListDeclaration form = attlists.at("form");
        assert((form.at("method").presence == AttributePresence::fixed));
        assert((form.at("method").default_value == String("POST")));
    }, false);
    test_document(R"(<?xml version='1.00' encoding='utf-8'?>
        <!--Attribute list declarations: more diverse checks-->
        <!DOCTYPE root [
            <!ATTLIST id id ID #REQUIRED>
            <!ATTLIST idrefs a IDREF 'idref' b IDREFS " a   b c d   e f g  ">
            <!ATTLIST ents a ENTITY "entity" b ENTITIES '   h i j k l m'>
            <!ATTLIST tokens a NMTOKEN '123' b NMTOKENS "1    2   3   4">
            <!NOTATION a SYSTEM "a"><!NOTATION b SYSTEM "b"><!NOTATION c SYSTEM "c">
            <!ATTLIST nota a NOTATION ( a | b | c ) #FIXED " c ">
        ]><root>
            <id id="a"/><id id="b"/><id id="c"/><id id="d"/><id id="e"/><id id="f"/><id id="g"/>
            <id id="idref"/>
            <idrefs></idrefs><nota a="c"/>
        </root>
    )", [](const Document& document) {
        auto attlists = document.doctype_declaration.attribute_list_declarations;
        AttributeListDeclaration idrefs = attlists.at("idrefs");
        assert((idrefs.at("a").type == AttributeType::idref));
        assert((idrefs.at("a").default_value == String("idref")));
        assert((idrefs.at("b").type == AttributeType::idrefs));
        assert((idrefs.at("b").default_value == String("a b c d e f g")));
        AttributeListDeclaration ents = attlists.at("ents");
        assert((ents.at("a").type == AttributeType::entity));
        assert((ents.at("a").default_value == String("entity")));
        assert((ents.at("b").type == AttributeType::entities));
        assert((ents.at("b").default_value == String("h i j k l m")));
        AttributeListDeclaration tokens = attlists.at("tokens");
        assert((tokens.at("a").type == AttributeType::nmtoken));
        assert((tokens.at("a").default_value == String("123")));
        assert((tokens.at("b").type == AttributeType::nmtokens));
        assert((tokens.at("b").default_value == String("1 2 3 4")));
        AttributeListDeclaration nota = attlists.at("nota");
        assert((nota.at("a").type == AttributeType::notation));
        assert((nota.at("a").notations.size() == 3));
        assert((nota.at("a").presence == AttributePresence::fixed));
        assert((nota.at("a").default_value == String("c")));
    }, false);
    test_document(R"(<!DOCTYPE root [
        <!-- Testing General Entity Declarations -->
        <!ENTITY g1 "value1">
        <!ENTITY g1 "Dupe">
        <!ENTITY      g2   'value"2"'   >
        <!ENTITY open-hatch
                SYSTEM "http://www.textuality.com/boilerplate/OpenHatch.xml">
        <!ENTITY open-hatch2
                PUBLIC "-//Textuality//TEXT Standard open-hatch boilerplate//EN"
                "http://www.textuality.com/boilerplate/OpenHatch.xml">
        <!ENTITY hatch-pic
                SYSTEM "../grafix/OpenHatch.gif"
                NDATA gif >
    ]><root></root>
    )", [](const Document& document) {
        auto general_entities  = document.doctype_declaration.general_entities;
        assert((general_entities.size() == 5 + BUILT_IN_GENERAL_ENTITIES.size()));
        assert((!general_entities.at("g1").is_external));
        assert((!general_entities.at("g1").is_unparsed));
        assert((general_entities.at("g1").value == String("value1")));
        assert((general_entities.at("g2").value == String("value\"2\"")));
        assert((general_entities.at("open-hatch").is_external));
        assert((general_entities.at("open-hatch").external_id.type == ExternalIDType::system));
        assert((general_entities.at("open-hatch2").is_external));
        assert((general_entities.at("open-hatch2").external_id.type == ExternalIDType::public_));
        assert((general_entities.at("hatch-pic").is_external));
        assert((general_entities.at("hatch-pic").is_unparsed));
        assert((general_entities.at("hatch-pic").notation_name == String("gif")));
    }, false);
    test_document(R"(<!DOCTYPE root [
        <!-- Testing Parameter Entity Declarations -->
        <!ENTITY % p1 "value1">
        <!ENTITY % p1 "Dupe">
        <!ENTITY      %       p2        ''   >
        <!ENTITY % ISOLat2
                SYSTEM "http://www.xml.com/iso/isolat2-xml.entities" >
    ]><root></root>
    )", [](const Document& document) {
        auto param_entities = document.doctype_declaration.parameter_entities;
        assert((param_entities.size() == 3));
        assert((param_entities.at("p1").value == String("value1")));
        assert((param_entities.at("p2").value.empty()));
        assert((param_entities.at("ISOLat2").is_external));
        assert((param_entities.at("ISOLat2").external_id.type == ExternalIDType::system));
        assert((param_entities.at("ISOLat2").external_id.system_id ==
            String("http://www.xml.com/iso/isolat2-xml.entities")));
    }, false);
    test_document(R"(<!DOCTYPE root [
        <!-- Parameter Entity Usage Very Basic Test -->
        <!ENTITY % att1 " <!ATTLIST a b CDATA '123'> ">
        <!-- Test -->
        <!ELEMENT e EMPTY>
            %att1;
        <!ELEMENT f EMPTY>
            %att1;
            %att1;
            %att1;
    ]><root></root>
    )", [](const Document& document) {
        auto dtd = document.doctype_declaration;
        assert((dtd.element_declarations.size() == 2));
        assert((dtd.attribute_list_declarations.size() == 1));
        assert((dtd.attribute_list_declarations.at("a").at("b").default_value == String("123")));
    }, false);
    test_document(R"(<!DOCTYPE root [
        <!-- Parameter Entities in Entity Values TEST -->
        <!ENTITY % a "1'2'3" >
        <!ENTITY % b  "0'%a;'4">
        <!ENTITY %  c '%b;'>
        <!ENTITY a "Counting: %b;!">
    ]><root></root>
    )", [](const Document& document) {
        auto parameter_entities = document.doctype_declaration.parameter_entities;
        auto gen_entities = document.doctype_declaration.general_entities;
        assert((parameter_entities.size() == 3));
        assert((parameter_entities.at("a").value == String("1'2'3")));
        assert((parameter_entities.at("b").value == String("0'1'2'3'4")));
        assert((parameter_entities.at("c").value == String("0'1'2'3'4")));
        assert((gen_entities.at("a").value == String("Counting: 0'1'2'3'4!")));
    }, false);
    test_document(R"(<!DOCTYPE root [
        <!-- Character/General REFERENCES in ENTITY VALUES -->
        <!ENTITY x "a&#98;&#x63;d">
        <!ENTITY % y  "&x;efg&#000104;">
        <!ENTITY z '%y;ij&#x6B;'>
    ]><root></root>
    )", [](const Document& document) {
        auto parameter_entities = document.doctype_declaration.parameter_entities;
        auto gen_entities = document.doctype_declaration.general_entities;
        assert((gen_entities.at("x").value == String("abcd")));
        assert((gen_entities.at("z").value == String("&x;efghijk")));
    }, false);
    test_document(R"(<!DOCTYPE countries [
        <!-- Entities in attributes - test -->
        <!ENTITY eur "E&#117;r">
        <!ENTITY op 'op'>
        <!ENTITY europ "&eur;&op;">
        <!-- Testing recursive behaviour -->
        <!ENTITY europe "&europ;e">
        <!ENTITY asia "&#65;&#115;&#105;&#97;">
        <!ENTITY africa 'Africa'>
        <!ATTLIST countries continents NMTOKENS #REQUIRED>
        <!ATTLIST country name CDATA #REQUIRED continent CDATA #REQUIRED capital CDATA #REQUIRED>
    ]>
    <!-- Not-all continents included, just for demo-->
    <countries continents="&europe; &asia; &africa;">
        <country name="Ireland" continent="&europe;" capital="Dublin"/>
        <country name="Japan" capital="Tokyo" continent="&asia;"/>
        <country name="Egypt" continent="&africa;" capital="Cairo"/>
    </countries>
    )", [](const Document& document) {
        Element root = document.root;
        auto a = root.tag.attributes.at("continents");
        assert((root.tag.attributes.at("continents") == String("Europe Asia Africa")));
        assert((root.children.at(0).tag.attributes.at("continent") == String("Europe")));
        assert((root.children.at(0).tag.attributes.at("name") == String("Ireland")));
        assert((root.children.at(1).tag.attributes.at("continent") == String("Asia")));
        assert((root.children.at(1).tag.attributes.at("capital") == String("Tokyo")));
        assert((root.children.at(2).tag.attributes.at("continent") == String("Africa")));
    }, false);
    test_document(R"(<!DOCTYPE root [
        <!-- Entity reference ABUSE -->
        <!ENTITY a "a">
        <!ENTITY b "&a;&a;">
        <!ENTITY c "&b;&b;&b;">
        <!ENTITY d "&c;&c;&c;&c;">
        <!ENTITY e "&d;&d;&d;&d;&d;">
        <!ENTITY f "&e;&e;&e;&e;&e;&e;">
        <!ENTITY g "&f;&f;&f;&f;&f;&f;&f;">
        <!ENTITY h "&g;&g;&g;&g;&g;&g;&g;&g;">
        <!ATTLIST root att CDATA #IMPLIED>
    ]><root att="&a;&b;&c;&d;&e;&f;&g;&h;"></root>)", [](const Document& document) {
        assert((document.root.tag.attributes.at("att").size() ==
            1 + 2 + 6 + 24 + 120 + 720 + 5040 + 40320));
    }, false);
    std::cout << "Checkpoint\n";
    test_document(R"(<!DOCTYPE root [
        <!-- Taken straight from the standard. -->
        <!ENTITY example "[&#38;#38;][&#38;#38;#38;][&amp;amp;]" >
    ]><root att="&example;"></root>
    )", [](const Document& document) {
        assert((document.doctype_declaration.general_entities.at("example").value 
            == String("[&#38;][&#38;#38;][&amp;amp;]")));
        assert((document.root.tag.attributes.at("att") == String("[&][&#38;][&amp;]")));
    }, false, false);
    test_document(R"(<!DOCTYPE root [
        <!-- Testing explicit built-in entity declarations! -->
        <!ENTITY lt "&#x26;#x03C;">
        <!ENTITY gt '>'>
        <!ENTITY amp "&#38;#38;">
        <!ENTITY apos "&#39;">
        <!ENTITY quot "&#000000000000000000034;">
    ]><root all="&lt;&gt;&amp;&apos;&quot;"></root>)", [](const Document& document) {
        assert((document.root.tag.attributes.at("all") == String("<>&'\"")));
    }, false, false);
    test_document(R"(<?xml version='1.0'?>
        <!DOCTYPE test [
        <!ELEMENT test (#PCDATA) >
        <!ENTITY % xx '&#37;zz;'>
        <!ENTITY % zz '&#60;!ENTITY tricky "error-prone" >' >
    %xx;]><test att="This sample shows a &tricky; method."></test>
    )", [](const Document& document) {
        assert((document.root.tag.attributes.at("att")
            == String("This sample shows a error-prone method.")));
    }, false, false);
    test_document(R"(
        <!DOCTYPE root [
            <!ENTITY d "&#xD;">
            <!ENTITY a "&#xA;">
            <!ENTITY da "&#xD;&#xA;">
            <!ENTITY t '&#9;'>
        ]><root a="

xyz" b="&d;&d;A&a;&#x20;&a;B&da;" c="&#xd;&#xd;A&#xa;&#xa;B&#xd;&#xa;&t;&t;"></root>
    )", [](const Document& document) {
        auto& attributes = document.root.tag.attributes;
        assert((attributes.at("a") == String("  xyz")));
        assert((attributes.at("b") == String("  A   B  ")));
        assert((attributes.at("c") == String("\r\rA\n\nB\r\n  ")));
    }, false, false);
    test_document(R"(<?xml version="1.0" encoding="utf-8"?>
        <!DOCTYPE activities [
            <!-- If this document is correctly parsed,
                most 'normal' XML docs should pass with this parser! -->
            <!ENTITY morning 'Morning'><!ENTITY afternoon "Aftern&#111;&#111;n">
            <!ENTITY evening "Evening"><!ENTITY night "Night"><!ENTITY run "Run">
        ]>
        <activities count='4' userId="123456" settings="43&amp;54&amp;&amp;25">
            <activity id="8888" distance="5.44">&afternoon; <!-- -->&run;</activity>
            <activity id="1234" distance="6.46">&night;&#x020;&#0074;og</activity>
            <activity id="0000" distance="7.77">Wet &amp; Fun &run;&lt;</activity>
            <activity id="2323" distance="9.99">Dry<![CDATA[ & Boring Run<]]>></activity>
        </activities>
    )", [](const Document& document) {
        Tag root = document.root.tag;
        assert((root.attributes.at("count") == String("4")));
        assert((root.attributes.at("userId") == String("123456")));
        assert((root.attributes.at("settings") == String("43&54&&25")));
        std::vector<Element> activities = document.root.children;
        std::vector<String> ids {"8888", "1234", "0000", "2323"};
        std::vector<String> distances {"5.44", "6.46", "7.77", "9.99"};
        std::vector<String> titles {"Afternoon Run", "Night Jog", "Wet & Fun Run<", "Dry & Boring Run<>"};
        assert((activities.size() == 4));
        for (int i = 0; i < 4; ++i) {
            Element activity = activities[i];
            assert((activity.tag.attributes.at("id") == ids[i]));
            assert((activity.tag.attributes.at("distance") == distances[i]));
            assert((activity.text == titles[i]));
        }
    }, false,false);
    test_document(R"(
        <!DOCTYPE root [
            <!-- It's not looking great now - markup in entity references! -->
            <!ENTITY example '<example a="b" c = &#39;d&#x27;   /><!--Oh no-->'>
            <!ENTITY example2 '&example;<![CDATA[A]]>&example;B&example;C'>
        ]><root>Extremely bad situation here!&example2;</root>
    )", [](const Document& document) {
        assert((document.root.text == String("Extremely bad situation here!ABC")));
        assert((document.root.children.size() == 3));
        assert((document.root.children.at(1).tag.attributes.at("c") == String("d")));
    }, false, false);
}