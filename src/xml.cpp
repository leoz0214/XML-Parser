#include "xml.h"


namespace xml {

Document parse(const std::string& string, bool validate_elements, bool validate_attributes) {
    Parser parser(string);
    return parser.parse_document(validate_elements, validate_attributes);
}

Document parse(std::istream& istream, bool validate_elements, bool validate_attributes) {
    Parser parser(istream);
    return parser.parse_document(validate_elements, validate_attributes);
}

}
