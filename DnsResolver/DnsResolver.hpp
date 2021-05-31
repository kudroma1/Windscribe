#pragma once

#include <winerror.h>

#include <atomic>
#include <future>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

namespace Windscribe { 

/** Simple async thread-safe DNS resolver. */
class DnsResolver
{
public:

    /** Result code of the resolution on one DNS server. */
    enum class RESULT_CODE {
        SUCCESS,
        EMPTY_HOST,
        NOT_RESOLVED,
        INTERNAL_ERROR
    };

    /** Resulting ip of the DNS resolution. 
    * If ip is empty then resCode contains error occured.
    */
    struct ResIp {
        ResIp() = default;
        ResIp(const wstring& ip, RESULT_CODE resCode = RESULT_CODE::SUCCESS) : ip(ip), resCode(resCode) {}
        wstring ip;
        RESULT_CODE resCode{};
    };

    /** Incapsulates data of the DNS resolution. */
    struct Data : public enable_shared_from_this<Data> {
        using DataPtr = shared_ptr<DnsResolver::Data>;

        Data(const vector<wstring>& dns, const wstring& host, promise<DataPtr> promise);
        ~Data();

        const vector<ResIp>& ips() const { return ips_; }

        /** Called if error was occured during resolution on some DNS server. */
        void onError(int ind, RESULT_CODE code);

        /** Called if ip was resolved on some DNS server. */
        void onIpResolved(int ind, wstring&& ip);

        /** Called if DNS resolution for the given host is done. */
        void onFinish();

        /** Prints information about Data. */
        void print(int id = -1) const;

    private:
        /** Resolved ips and occured errors. Size of the vector is equal to the count of DNS servers. */
        vector<ResIp> ips_;

        /** Host to resolve. */
        wstring host_;

        /** Provided by user DNS servers. */
        vector<wstring> dns_;

        /** Technical member equal to the number of DNS servers. 
        * Used to control when resolution is finished.
        */
        int maxCount_{ 0 };

        /** Promise used by caller to get the result. */
        promise<DataPtr> promise_;

        /** Tracks count of the already processed DNS servers. */
        atomic_int processedCount_{ 0 };

        /** If there are not user defined dns there is one resolved ip. */
        static const int DEFAULT_DNS_SIZE{ 1 };

        /** @debug To track number of allocated Data objects. */
        static atomic_int createdCount;

        /** @debug To track number of deleted Data objects. */
        static atomic_int deletedCount;
    };

    /** Lookups DNS serveres to resolve host. 
    * @param host Host to resolve.
    * @param dns Dns servers.
    * @param res Promise to return the result to the caller.
    */
    void Lookup(const wstring& host, const vector<wstring>& dns, promise<shared_ptr<DnsResolver::Data>> res);

    DnsResolver() = default;
    DnsResolver(DnsResolver&&) = default;            
    DnsResolver& operator=(DnsResolver&&) = default;
    ~DnsResolver();

    /** Converts RESULT_CODE to string. */
    static string toString(RESULT_CODE code);

    /** @debug Logs useful information. */
    static void log(const string& func, const string& msg = "");

private:
    struct Impl;
    struct ImplDeleter { void operator()(Impl*) const; };
    std::unique_ptr<Impl, ImplDeleter> pImpl_{ nullptr };
};

using DataPtr = shared_ptr<DnsResolver::Data>;

}