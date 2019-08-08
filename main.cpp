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


template<class Container>
auto my_equal_range(Container&& container, double target, double epsilon = 1E-6)
-> decltype(container.equal_range(target))
{
    auto lower = container.lower_bound(target - epsilon);
    auto upper = container.upper_bound(target + epsilon);
    return std::make_pair(lower, upper);
}

template<typename Container>
bool get_index(Container const& container, double target, uint64_t& index, double epsilon = 1E-6) {
    auto range = my_equal_range(container, target, epsilon);
    bool const exists(range.first != range.second);
    if (exists) index = range.first->second;
    return exists;
}

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

template<class Map>
double insert(Weeksecs& weeksecs, Indices& indices)
{
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

    return timer.elapsedSeconds();
}

template<class Map>
double read()
{
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
            double key = weeksecs[i];
            timer.start();
            uint64_t timestamp;
            if (!get_index<Map>(map, key, timestamp))
            {
                cout << "Failure read" << endl;
                spdlog::error("Failure read");
            }
            //auto iter = map.lower_bound(timestamp);
            //if (iter == map.end())
            //{
            //    cout << "Failure read" << endl;
            //    spdlog::error("Failure read");
            //}
            timer.stop();
        });

        cout << '#';
    }

    return timer.elapsedSeconds();
}

double read_vector()
{
    using Vec = vector<double, uint64_t>;

    random_device rnd;
    default_random_engine eng(rnd());
    uniform_int_distribution<> uid(0, nbrOfIndices - 1);

    Weeksecs weeksecs;
    Indices indices;

    Timer timer;
    for (auto c(0); c != nbrOfRuns; ++c)
    {
        generate_test_data(weeksecs, indices);

        vector<int> lookups(nbrOfIndices);
        generate_n(begin(lookups), nbrOfIndices, [&]() { return uid(eng); });

        for_each(begin(lookups), end(lookups), [&](int i)
        {
            double key = weeksecs[i];
            timer.start();
            bool found(false);
            for (auto&& ws : weeksecs)
            {
                if (ws >= key && (ws - key) < 1E-6)
                {
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                cout << "Failure read vector" << endl;
                spdlog::error("Failure read vector");
            }
            timer.stop();
        });

        cout << '#';
    }

    return timer.elapsedSeconds();
}


void print_result(double secs, string const& msg)
{
    auto v(nbrOfRuns * nbrOfIndices / secs);
    spdlog::info("{:.3f} [s], {:.1f} [items/s], {:.3f} [us/item] :{}", secs, v, 1000000.0/v, msg);
    cout << " " << 1000000.0 / v << "[us/item]" << endl;
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

    Timer timer;
    timer.start();

    cout << "Running insert<vector<double, uint64_t>> ..." << endl;
    secs = insert<map<double, uint64_t>>(weeksecs, indices);
    print_result(secs, "insert<map<double, uint64_t>>");

    cout << "Running read_vector ..." << endl;
    secs = read_vector();
    print_result(secs, "read_vector");

    cout << "Running insert<map<double, uint64_t>> ..." << endl;
    secs = insert<map<double, uint64_t>>(weeksecs, indices);
    print_result(secs, "insert<map<double, uint64_t>>");

    cout << "Running read<map<double, uint64_t>> ..." << endl;
    secs = read<map<double, uint64_t>>();
    print_result(secs, "read<map<double, uint64_t>>");

    cout << "Running insert<unordered_map<double, uint64_t>> ..." << endl;
    secs = insert<unordered_map<double, uint64_t>>(weeksecs, indices);
    print_result(secs, "insert<unordered_map<double, uint64_t>>");

    cout << "Running read<unordered_map<double, uint64_t>> ..." << endl;
    secs = read<map<double, uint64_t>>();
    print_result(secs, "read<unordered_map<double, uint64_t>>");

    timer.stop();
    cout << endl;

    spdlog::info("Total time: {}s", timer.elapsedSeconds());

    return 0;
}