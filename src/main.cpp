
#include <iostream>
#include <boost/lexical_cast.hpp>

#include "../bin/version.h"

// #include "processor.h"
#include "async.h"

int main(int argc, char** argv)
{
    size_t N = argc > 1 ? boost::lexical_cast<size_t>(argv[1]) : 0;

    async::handle_t handle = async::connect(N);
    char buffer[16];
    while(!std::cin.eof()) {
        std::cin.read(buffer, 16);
        async::receive(handle, buffer, std::cin.gcount());
    }
    async::disconnect(handle);

    return 0;

    // {
    //     Reader r(N);
    //     r.subscribe(ConsolePrint::get_singleton());
    //     r.subscribe(FilePrint::get_singleton());

    //     r.read(std::cin);

    // }

    // ConsolePrint::get_singleton()->done();
    // FilePrint::get_singleton()->done();

    // Metrics::get_metrics().dump();

    // return 0;
}
