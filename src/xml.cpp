#include "xml.h"


namespace xml {

Document parse(const std::string& string) {
    Parser parser(string);
    return parser.parse_document();
}

}
