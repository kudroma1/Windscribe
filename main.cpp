#include "Algorithms.hpp"
#include "DnsResolver.hpp"

#include <boost/locale.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/file.hpp>

#include <algorithm>
#include <fstream>
#include <future>
#include <iostream>
#include <random>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

using namespace std;
using namespace Windscribe;

/** Hosts to resolve. */
const vector<wstring> hosts = {
    L"google.com",
    L"linkedin.com",
    L"mail.ru",
    L"blablabla",
    L""
};

/** DNS servers used. */
const vector<wstring> dnsServers = {
    L"8.8.8.8",
    L"9.9.9.9",
    L"208.67.222.222"
};

/** Number of threads to call DnsResolver. */
const int kThreadNum{ 100 };

/** Timeout to wait for the single resolution future. */
const chrono::milliseconds kTimeout{ 5ms };

/** TEST 1. DnsResolver */
void test1() {

    // Configure log options.
    boost::log::add_file_log(
        boost::log::keywords::target_file_name = "test1.log",
        boost::log::keywords::file_name = "test1.log"
    );
    boost::log::core::get()->set_filter
    (
        boost::log::trivial::severity >= boost::log::trivial::debug
    );

    /** Launch tasks to resolve host using DNS servers. */
    BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << " ======================= LOOKUP STARTED =========================";

    const int tasksCount{ kThreadNum * 2 * static_cast<int>(hosts.size()) };
    DnsResolver resolver;
    vector<future<DataPtr>> futures;
    futures.reserve(tasksCount);
    mutex mut; // used only for test purposes.
    auto func = [&]() {
        for (const auto& host : hosts) {
            promise<DataPtr> prom1;
            {
                lock_guard<mutex> lock(mut);
                futures.emplace_back(prom1.get_future());
            }
            resolver.Lookup(host, dnsServers, move(prom1));

            promise<DataPtr> prom2;
            {
                lock_guard<mutex> lock(mut);
                futures.emplace_back(prom2.get_future());
            }
            resolver.Lookup(host, {}, move(prom2));
        }
    };
    vector<thread> threads;
    threads.reserve(kThreadNum);
    for (auto i = 0; i < kThreadNum; ++i) {
        threads.emplace_back(func);
    }
    for (auto&& t : threads)
        t.detach();

    // Wait until all results will be ready and printSmall them when they are ready.
    BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << " ======================= LOOKUP INITIATED FOR ALL HOSTS =========================";
    
    int count{ 0 };
    vector<int> order; // order of tasks in which they are printed (not done, because we will have wait timeout)
    order.reserve(tasksCount);
    unordered_set<int> got; // technical hash-table used to check if the given future was already processed.
    while (count < tasksCount) {
        for (auto i = 0; i < futures.size(); ++i) {
            if (got.find(i) != got.cend())
                continue;
            if (futures[i].wait_for(kTimeout) == future_status::ready) {
                got.insert(i);
                count++;
                futures[i].get()->print(i);
                order.push_back(i);
            }
        }
    }

    // Log order of the tasks to be printed.
    BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << " ======================== LOOKUP FINISHED ===========================";
    string orderStr;
    for (const auto& v : order) {
        orderStr += to_string(v);
        orderStr += " ";
    }
    orderStr += "\n\n";
    orderStr += to_string(order.size()); 
    orderStr += " ";
    orderStr += to_string(futures.size());
    orderStr += " ";
    orderStr += to_string(count);
    orderStr += " ";
    orderStr += to_string(tasksCount);
    BOOST_LOG_TRIVIAL(debug) << orderStr;
}

vector < tuple<vector<int>, vector<int> > > data2 = {
    make_tuple<vector<int>, vector<int>>(
        {1, 2, 3, 4, 5, 6, 6, 7, 8, 9},
        {2, 4, 5, 7, 6, 5, 6, 8, 11, 10}),
    make_tuple<vector<int>, vector<int>>(
        {1, 1, 1, 1, 1, 1, 1, 1, 1},
        {2, 4, 5, 7, 6, 5, 6, 8, 11, 10}),
    make_tuple<vector<int>, vector<int>>(
        {1, 1, 1, 1, 1, 1, 1, 1, 1},
        {2, 4, 1, 1, 1, 5, 7, 6, 5, 6, 8, 11, 10}),
    make_tuple<vector<int>, vector<int>>(
        {1, 4, 4, 1, 1, 5, 5, 1, 1},
        {1, 4, 4, 1, 1, 5, 5, 1, 1}),
    make_tuple<vector<int>, vector<int>>(
        {1, 4, 4, 1, 1, 5, 5, 1, 1},
        {2, 4, 4, 1, 1, 5, 7, 6, 5, 6, 8, 11, 10})
};

/** Test 2. Set intersection with repetitions. */
void test2() {

    // Configure log options.
    boost::log::add_file_log(
        boost::log::keywords::target_file_name = "test2.log",
        boost::log::keywords::file_name = "test2.log"
    );
    boost::log::core::get()->set_filter
    (
        boost::log::trivial::severity >= boost::log::trivial::debug
    );

    // Helper lambda for printing small arrays.
    Algorithms<int> alg;

    // Test small sets
    auto printSmall = [](const tuple<vector<int>, vector<int>>& in, const vector<int>& res) {
        string str = "";
        str += "(";
        for (const auto& el : get<0>(in))
            str += to_string(el) + " ";
        str += "), (";
        for (const auto& el : get<1>(in))
            str += to_string(el) + " ";
        str += ")  ----->  (";
        for (const auto& el : res)
            str += to_string(el) + " ";
        str += ")";
        BOOST_LOG_TRIVIAL(debug) << str;

    };
    for (const auto& in : data2) {
        printSmall(in, alg.intersection(get<0>(in), get<1>(in)));
        auto v1 = get<0>(in);
        auto v2 = get<1>(in);
        printSmall(make_tuple(v1, v2), alg.intersection2(v1, v2));
    }

    // Test big sets;
    auto testPrintBig = [&](const string& testName, const tuple<vector<int>, vector<int>>& in) {
        
        {
            const auto nowS = chrono::high_resolution_clock::now();
            const auto resS = alg.intersection(get<0>(in), get<1>(in));
            const auto endS = chrono::high_resolution_clock::now();
            const auto durS = chrono::duration_cast<chrono::milliseconds>(endS - nowS).count();

            BOOST_LOG_TRIVIAL(debug) << testName << " in1.size=" << get<0>(in).size()
                << " in2.size=" << get<1>(in).size()
                << " res.size=" << resS.size()
                << " timeSingle=" << durS;
        } 
        
        {
            auto v1 = get<0>(in);
            auto v2 = get<1>(in);
            const auto nowS = chrono::high_resolution_clock::now();
            const auto resS = alg.intersection2(v1, v2);
            const auto endS = chrono::high_resolution_clock::now();
            const auto durS = chrono::duration_cast<chrono::milliseconds>(endS - nowS).count();

            BOOST_LOG_TRIVIAL(debug) << testName << " in1.size=" << get<0>(in).size()
                << " in2.size=" << get<1>(in).size()
                << " res.size=" << resS.size()
                << " timeSingle=" << durS;
        }
    };
    testPrintBig("not intersected ", make_tuple(vector<int>(2e8, 1), vector<int>(2e8, 2)));
    vector<int> v1(1e8), v2(2e8);
    testPrintBig("totally intersected ", make_tuple(vector<int>(1e8, 1), vector<int>(2e8, 1)));
};

void test3() {

    // Configure log options.
    boost::log::add_file_log(
        boost::log::keywords::target_file_name = "test3.log",
        boost::log::keywords::file_name = "test3.log"
    );
    boost::log::core::get()->set_filter
    (
        boost::log::trivial::severity >= boost::log::trivial::debug
    );

    // Test data.
    vector<Segments<int>> data3 = {
        {make_pair(1,3), make_pair(4,5), make_pair(8,10)}, // not intersected
        {make_pair(1,3), make_pair(4,5), make_pair(8,10), make_pair(0, 30)}, // one result segment
        {make_pair(1,3), make_pair(1,3), make_pair(1,3)}, // all the same
        {make_pair(1,4), make_pair(2,3), make_pair(3,5), make_pair(4,10), make_pair(9,13), make_pair(8,10) }, // last intersected, inside
        {make_pair(1,4), make_pair(2,3), make_pair(3,5), make_pair(4,10), make_pair(9,13), make_pair(14,15) } // last intersected, inside
    };

    Algorithms<int> alg;

    auto toString = [](const Segments<int>& segs) {
        string res;
        for (const auto& seg : segs) {
            res += "(";
            res += to_string(seg.first);
            res += ", ";
            res += to_string(seg.second);
            res += ") ";
        }
        return res;
    };
    auto calcAndPrint = [&](Segments<int>& segs) {
        BOOST_LOG_TRIVIAL(debug) << toString(segs) << " ----> " << toString(alg.segmentsUnion(segs));
    };
    for (auto& segs : data3)
        calcAndPrint(segs);
}

int main(int argc, char** argv) {
    cout << "Test 1: DnsResolver. Doing ..." << endl;
    test1();
    cout << "Test 2: Sets intersection. Doing ..." << endl;
    test2();
    cout << "Test 3: Segments union. Doing ..." << endl;
    test3();
}