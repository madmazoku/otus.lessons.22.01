
#include <iostream>
#include <boost/lexical_cast.hpp>

#include "../bin/version.h"

#include "metrics.h"
#include "async.h"

int main(int argc, char** argv)
{
    size_t N = argc > 1 ? boost::lexical_cast<size_t>(argv[1]) : 0;

    async::handle_t handle = async::connect(N);
    char buffer[3];
    while(!std::cin.eof()) {
        std::cin.read(buffer, 3);
        async::receive(handle, buffer, std::cin.gcount());
    }
    async::disconnect(handle);

    Metrics::get_metrics().dump();

    return 0;
}
