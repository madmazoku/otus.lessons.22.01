#define BOOST_TEST_MODULE TestMain
#include <boost/test/unit_test.hpp>

#include "../bin/version.h"

#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <algorithm> 
#include <queue>
#include <fstream>

#include <boost/timer/timer.hpp>

BOOST_AUTO_TEST_SUITE( test_suite )

BOOST_AUTO_TEST_CASE( test_version )
{
    BOOST_CHECK_GT(build_version(), 0);
}

void eat_cpu(size_t power) {
    std::vector<size_t> v(power);
    for(size_t p = 0; p < power; ++p)
        v[p] = p;
    while(std::next_permutation(v.begin(), v.end()))
        ;
}

#define __THREAD_TEST__

BOOST_AUTO_TEST_CASE( test_threads )
{
#ifdef __THREAD_TEST__

    size_t hardware_concurrency = std::thread::hardware_concurrency();
    std::cout << "hardware concurrency: " << hardware_concurrency << std::endl;

    std::mutex tm;
    std::condition_variable tcv;

    std::ofstream out("test.csv", std::ios::app);
    size_t max_thread_count = hardware_concurrency * 3;
    for(size_t thread_count = 1; thread_count < max_thread_count + 1; ++thread_count) {
        std::unique_lock<std::mutex> tl(tm);

        boost::timer::cpu_timer cpu;
        boost::timer::cpu_times start = cpu.elapsed();

        std::queue<std::thread> threads;
        for(size_t n = 0; n < thread_count; ++n)
            threads.push(std::thread([](){
                eat_cpu(11);
            }));

        while(!threads.empty()) {
            threads.front().join();
            threads.pop();
        }

        boost::timer::cpu_times end = cpu.elapsed();
        boost::timer::cpu_times duration;

        boost::timer::nanosecond_type wall = (end.wall - start.wall);
        boost::timer::nanosecond_type user = (end.user - start.user);
        boost::timer::nanosecond_type sys = (end.system - start.system);

        std::cout << "threads: " << thread_count << std::endl;
        std::cout << "real: " << (wall * 1e-9) << std::endl;
        std::cout << "user: " << (user * 1e-9) << std::endl;
        std::cout << "system: " << (sys * 1e-9) << std::endl;
        std::cout << "u/r ratio: " << (1.0 * user / wall) << std::endl;
        std::cout << std::endl;

        out << thread_count << std::fixed
            << '\t' << (1.0 * user / wall)
            << '\t' << (wall * 1e-9)
            << '\t' << (user * 1e-9)
            << '\t' << (sys * 1e-9)
            << std::endl;
    }
    out.close();

#endif

    BOOST_CHECK(true);
}

/*
2 core x 2 hyperthreading
threads <\t> u/r ratio <\t> real <\t> user <\t> system
1   0.999812    60.401351   60.390000   0.000000
2   1.996001    63.061099   125.870000  0.010000
3   2.765745    87.770195   242.750000  0.190000
4   3.818145    98.825476   377.330000  0.120000
5   3.737951    126.034288  471.110000  0.290000
6   3.895955    145.807634  568.060000  0.090000
7   3.872968    171.514473  664.270000  0.120000
8   3.860902    196.658704  759.280000  0.270000
9   3.858632    220.689609  851.560000  0.210000
10  3.741460    252.086102  943.170000  0.520000
11  3.754773    276.077914  1036.610000 1.520000
12  3.716745    303.773860  1129.050000 1.200000

no meaning to use more then 4

*/

BOOST_AUTO_TEST_SUITE_END()

