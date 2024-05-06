#include "xml.h"


namespace xml {

void parse(Parser& stream) {
    // TODO
}

void parse(const std::string& string) {
    Parser parser(string);
    return parse(parser);
}

}
