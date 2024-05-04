#include "stream.h"
#include <iostream>


namespace xml {

Stream::Stream(const std::string& data) {
    this->data = &data;
}

Char Stream::get() {
    char current_char = this->data->at(pos);
    if ((current_char & 0b10000000) == 0) {
        // Just ASCII
        this->increment = 1;
        return current_char;
    }
    int sig_1s = 1;
    char mask = 0b01000000;
    // Determine if 2-bytes, 3-bytes or 4-bytes based on number of leading 1s.
    while (current_char & mask) {
        sig_1s++;
        mask >>= 1;
    }
    // Validate indeed 2-4 bytes, and not invalid Unicode byte.
    if (sig_1s < 2 || sig_1s > 4) {
        throw;
    }
    // First byte has (8-n-1) bytes of interest where n is number of leading 1s
    Char char_value = current_char & (0b11111111 >> sig_1s);
    // Each subsequent byte has 10xxxxxx = 6 bytes of interest.
    for (int offset = 1; offset < sig_1s; ++offset) {
        char byte = this->data->at(pos + offset);
        // Ensure byte starts with 10......
        if ((byte & 0b10000000) == 0 || (byte & 0b01000000) == 1)  {
            throw;
        }
        char_value <<= 6;
        char_value += byte & 0b00111111;
    }
    this->increment = sig_1s;
    return char_value;
}

void Stream::operator++() {
    if (!this->increment) {
        this->get();
    }
    this->pos += this->increment;
}

bool Stream::eof() {
    return this->pos == this->data->size();
}

}
