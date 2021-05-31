#include "DnsResolver.hpp"

#include <boost/locale.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/file.hpp>

#include <fstream>
#include <future>
#include <iostream>
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

    // Wait until all results will be ready and print them when they are ready.
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

int main(int argc, char** argv) {
    cout << "Test 1: DnsResolver. Doing ..." << endl;
    test1();
}