// Wraps input data into an abstracted stream, returning the current
// Unicode character as an integer value each time it is requested.
#pragma once
#include <string>


namespace xml {

typedef int Char;

class Stream {
    const std::string* data;
    std::size_t pos = 0;
    std::size_t increment;
    public:
        Stream(const std::string&);
        Char get();
        void operator++();
        bool eof();
};

}