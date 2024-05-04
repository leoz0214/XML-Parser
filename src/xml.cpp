#include "xml.h"


namespace xml {

void parse(Stream& stream) {
    // TODO
}

void parse(const std::string& string) {
    Stream stream(string);
    return parse(stream);
}

}