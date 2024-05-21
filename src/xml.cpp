#include "xml.h"


namespace xml {

Document parse(const std::string& string) {
    Parser parser(string);
    return parser.parse_document();
}

Document parse(std::istream& istream) {
    // Parser parser(istream);
    // return parser.parse_document();
}

}
