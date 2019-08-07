#include <cstdio>
#include <string>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <numeric>
#include <random>
#include <map>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <args.hxx>

#include "timer.h"

using namespace std;

auto logger = spdlog::basic_logger_st("logger", "index_lookup.log");
int nbrOfIndices(0);
int nbrOfRuns(0);
int nrOfLookups(0);

using Weeksecs = vector<double>;
using Indices = vector<int64_t>;

void generate_test_data(Weeksecs& weeksecs, Indices& indices, int k = 10)
{
    random_device rnd;
    default_random_engine eng(rnd());
    uniform_int_distribution<> uid(0, k - 1);

    indices.resize(nbrOfIndices);
    weeksecs.resize(nbrOfIndices);

    int n(0);
    generate(indices.begin(), indices.end(), [&]() { return (k + uid(eng)) * n++; });
    transform(indices.begin(), indices.end(), begin(weeksecs),
        [&](int64_t index) -> double { return (double)(index + uid(eng) / 10.0002); });
}

//typedef unordered_map<TPoint3D, int, hashFunc, equalsFunc> TPoint3DMap;

double write_map(Weeksecs& weeksecs, Indices& indices)
{
    using Map = map<double, uint64_t>;

    Timer timer;
    for (auto c(0); c != nbrOfRuns; ++c)
    {
        generate_test_data(weeksecs, indices);

        Map map;
        int i(0);
        for_each(begin(weeksecs), end(weeksecs), [&](double ws)
        {
            timer.start();
            map[ws] = indices[i];
            timer.stop();
            ++i;
        });
        cout << '#';
    }
    cout << endl;

    return timer.elapsedSeconds();
}

double read_map()
{
    using Map = map<double, uint64_t>;

    random_device rnd;
    default_random_engine eng(rnd());
    uniform_int_distribution<> uid(0, nbrOfIndices - 1);

    Weeksecs weeksecs;
    Indices indices;

    Timer timer;
    for (auto c(0); c != nbrOfRuns; ++c)
    {
        generate_test_data(weeksecs, indices);

        Map map;
        int i(0);
        for_each(begin(weeksecs), end(weeksecs), [&](double ws)
        {
            map[ws] = indices[i++];
        });

        vector<int> lookups(nbrOfIndices);
        generate_n(begin(lookups), nbrOfIndices, [&]() { return uid(eng); });

        for_each(begin(lookups), end(lookups), [&](int i)
        {
            double const timestamp = weeksecs[i];
            timer.start();
            auto iter = map.lower_bound(timestamp);
            if (iter == map.end())
            {
                cout << "Failure read_map" << endl;
                spdlog::error("Failure read_map");
            }
            timer.stop();
        });

        cout << '#';
    }

    cout << endl;

    return timer.elapsedSeconds();
}

void print_result(double secs, string const& msg)
{
    spdlog::info("{:.3f} [s], {:.1f} [items/s] :{}", secs, nbrOfRuns * nbrOfIndices / secs, msg);
}

int main(int argc, char* argv[])
{
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::info);
    spdlog::flush_on(spdlog::level::info);
    spdlog::set_pattern("[%D %H:%M:%S] %v");

    args::ArgumentParser parser("This is a io performance test program.");
    args::HelpFlag help(parser, "help", "Display this help menu", { 'h', "help" });
    args::CompletionFlag completion(parser, { "complete" });
    args::ValueFlag<int> count(parser, "count", "Nbr of runs", { 'c' }, 100);
    args::ValueFlag<int> size(parser, "size", "Nbr of indices", { 's' }, 10000);
    args::ValueFlag<int> procent(parser, "procent", "Lookups procent", { 'p' }, 20);

    ostringstream cmdLine;
    cmdLine << "Args: ";
    for (auto i(1); i != argc; ++i)
    {
        cmdLine << string(argv[i]) + " ";
    }

    try
    {
        parser.ParseCLI(argc, argv);
    }
    catch (const args::Completion& e)
    {
        cout << e.what();
        return 0;
    }
    catch (const args::Help&)
    {
        cout << parser;
        return 0;
    }
    catch (const args::ParseError& e)
    {
        cerr << e.what() << std::endl;
        cerr << parser;
        return 1;
    }

    nbrOfIndices = args::get(size);
    nbrOfRuns = args::get(count);
    nrOfLookups = (int)(args::get(procent) / 100.0 * nbrOfIndices);

    spdlog::info("===== Start test ============");
    spdlog::info(cmdLine.str());

    Weeksecs weeksecs;
    Indices indices;

    generate_test_data(weeksecs, indices);

    //copy(indices.begin(), indices.end(),
    //    std::ostream_iterator<uint64_t>(std::cout, " "));
    //copy(weeksecs.begin(), weeksecs.end(),
    //    std::ostream_iterator<double>(std::cout, " "));

    double secs(0);

    cout << "Running write_map ..." << endl;
    secs = write_map(weeksecs, indices);
    print_result(secs, "write_map");

    cout << "Running read_map ..." << endl;
    secs = read_map();
    print_result(secs, "read_map");

    Timer timer;
    timer.start();

    timer.stop();
    cout << endl;

    spdlog::info("Total time: {}s", timer.elapsedSeconds());

    return 0;
}