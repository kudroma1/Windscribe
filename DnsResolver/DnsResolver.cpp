#include "DnsResolver.hpp"
#include "dnsresolver.hpp"

#include <boost/log/trivial.hpp>
#include <boost/locale.hpp>

#include <Ws2tcpip.h>
#include <Mstcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <windns.h>

#define MAX_ADDRESS_STRING_LENGTH   64

using namespace Windscribe;

DnsResolver::~DnsResolver() {}

string Windscribe::DnsResolver::toString(RESULT_CODE code)
{
    switch (code) {
    case RESULT_CODE::SUCCESS:             return "SUCCESS";
    case RESULT_CODE::NOT_RESOLVED:        return "NOT_RESOLVED";
    case RESULT_CODE::INTERNAL_ERROR:      return "INTERNAL_ERROR";
    case RESULT_CODE::EMPTY_HOST:          return "EMPTY_HOST";
    default:                               return "unknown";
    }
}

void Windscribe::DnsResolver::log(const string& func, const string& msg)
{
    BOOST_LOG_TRIVIAL(debug) << this_thread::get_id() << " " << func << " " << msg;
}

/**
* Code for DNS query is from https://github.com/microsoft/Windows-classic-samples/blob/master/Samples/DNSAsyncQuery/cpp/DnsQueryEx.cpp.
*/
struct DnsResolver::Impl
{
    ~Impl() {};
    
    /** @debug Used to track number of allocated contexts. */
    static atomic_int contextsAllocated_;

    /** @debug Used to track number of deleted contexts. */
    static atomic_int contextsDeleted_;

    /** Context for the DnsQueries. */
    typedef struct _QueryContextStruct
    {
        ULONG               RefCount{ 0 };
        WCHAR               QueryName[DNS_MAX_NAME_BUFFER_LENGTH];
        WORD                QueryType;
        ULONG               QueryOptions;
        DNS_QUERY_RESULT    QueryResults;
        DNS_QUERY_CANCEL    QueryCancelContext;
        HANDLE              QueryCompletedEvent;

        /** Member with Data.
        * @note Do not forget make it nullptr before deleting parent structure.
        */
        DataPtr             Data;

        /** Using this index context knows what Data::ips_ member it currently resolves.
        * This parameter allows to change Data by different threads without blocking.
        */
        INT                 Ind{ 0 };
    }QUERY_CONTEXT, * PQUERY_CONTEXT;

    /** Extracts IP from the DNS resolution result and sets it to data at the ind. */
    static void ExtractIp( PDNS_RECORD DnsRecord, DataPtr data, INT ind )
    {
        if (DnsRecord) {
            struct in_addr Ipv4address;
            WCHAR Ipv4String[MAX_ADDRESS_STRING_LENGTH] = L"\0";

            Ipv4address.S_un.S_addr = DnsRecord->Data.A.IpAddress;
            RtlIpv4AddressToStringW(&Ipv4address, Ipv4String);
            data->onIpResolved(ind, move(Ipv4String));
            log(__FUNCTION__, boost::locale::conv::utf_to_utf<char>(Ipv4String));
        }
        else {
            log(__FUNCTION__, "DnsQueryEx() failed!");
            data->onError(ind, RESULT_CODE::NOT_RESOLVED);
        }
    }

    /**
    *  Wrapper function that creates DNS_ADDR_ARRAY from IP address string.
    */
    DWORD CreateDnsServerList(_In_ PWSTR ServerIp, _Out_ PDNS_ADDR_ARRAY DnsServerList)
    {
        DWORD  Error = ERROR_SUCCESS;
        SOCKADDR_STORAGE SockAddr;
        INT AddressLength;
        WSADATA wsaData;

        ZeroMemory(DnsServerList, sizeof(*DnsServerList));

        Error = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (Error != 0)
        {
            WSACleanup();
            return Error;
        }

        AddressLength = sizeof(SockAddr);
        Error = WSAStringToAddressW(ServerIp,
            AF_INET,
            NULL,
            (LPSOCKADDR)&SockAddr,
            &AddressLength);
        if (Error != ERROR_SUCCESS)
        {
            AddressLength = sizeof(SockAddr);
            Error = WSAStringToAddressW(ServerIp,
                AF_INET6,
                NULL,
                (LPSOCKADDR)&SockAddr,
                &AddressLength);
        }

        if (Error != ERROR_SUCCESS)
        {
            WSACleanup();
            return Error;
        }

        DnsServerList->MaxCount = 1;
        DnsServerList->AddrCount = 1;
        CopyMemory(DnsServerList->AddrArray[0].MaxSa, &SockAddr, DNS_ADDR_MAX_SOCKADDR_LENGTH);

        WSACleanup();
        return Error;
    }

    /** Increments ref counter of the Context.
    * @todo Possibly not necessary in the given implementation as we have one Context per DNS.
    */
    VOID AddReferenceQueryContext(_Inout_ PQUERY_CONTEXT QueryContext)
    {
        InterlockedIncrement(&QueryContext->RefCount);
    }

    /** Cleans up Context after DNS resolution. */
    static VOID DeReferenceQueryContext(_Inout_ PQUERY_CONTEXT* QueryContext)
    {
        PQUERY_CONTEXT QC = *QueryContext;

        if (InterlockedDecrement(&QC->RefCount) == 0)
        {
            QC->Data = nullptr; // clean shared_ptr pointed to Data
            contextsDeleted_.fetch_add(1, memory_order_relaxed);
            log(__FUNCTION__, "Contexts deleted ---> " + to_string(contextsDeleted_));
            if (QC->QueryCompletedEvent)
            {
                CloseHandle(QC->QueryCompletedEvent);
            }

            HeapFree(GetProcessHeap(), 0, QC);
            *QueryContext = NULL;
        }
    }

    /** Allocates context for the DNS resolution for the single DNS server. */
    DWORD AllocateQueryContext(_Out_ PQUERY_CONTEXT* QueryContext)
    {
        DWORD Error = ERROR_SUCCESS;

        *QueryContext = (PQUERY_CONTEXT)HeapAlloc(GetProcessHeap(),
            HEAP_ZERO_MEMORY,
            sizeof(QUERY_CONTEXT));
        if (*QueryContext == NULL)
        {
            return GetLastError();
        }

        (*QueryContext)->QueryResults.Version = DNS_QUERY_RESULTS_VERSION1;

        (*QueryContext)->QueryCompletedEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

        if ((*QueryContext)->QueryCompletedEvent == NULL)
        {
            Error = GetLastError();
            DeReferenceQueryContext(QueryContext);
            *QueryContext = NULL;
        }
        return Error;
    }

    /** Callback function called by DNS as part of asynchronous query complete. */
    static VOID WINAPI QueryCompleteCallback(_In_ PVOID Context, _Inout_ PDNS_QUERY_RESULT QueryResults)
    {
        PQUERY_CONTEXT QueryContext = (PQUERY_CONTEXT)Context;

        if (QueryResults->QueryStatus == ERROR_SUCCESS)
        {
            ExtractIp(QueryResults->pQueryRecords, QueryContext->Data, QueryContext->Ind);
        }
        else
        {
            log(__FUNCTION__, "DnsQueryEx() failed!");
            QueryContext->Data->onError(QueryContext->Ind, RESULT_CODE::NOT_RESOLVED);
        }

        if (QueryResults->pQueryRecords)
        {
            DnsRecordListFree(QueryResults->pQueryRecords, DnsFreeRecordList);
        }

        SetEvent(QueryContext->QueryCompletedEvent);
        DeReferenceQueryContext(&QueryContext);
    }

    /** Implements lookup of the host using dns servers and returning the result to caller using res. */
    void Lookup(const wstring& host, const vector<wstring>& dns, promise<DataPtr> res) {
        log(__FUNCTION__, boost::locale::conv::utf_to_utf<char>(host));

        if (host.empty())
        {
            vector<wstring> dns = { L"" };
            auto data = make_shared<Data>(dns, host, move(res));
            data->onError(0, RESULT_CODE::EMPTY_HOST);
            log(__FUNCTION__, "Empty host!");
            return;
        }

        int ind = 0;
        int maxCount = dns.empty() ? 1 : static_cast<int>(dns.size());
        auto data = std::make_shared<DnsResolver::Data>(dns, host, move(res));

        do {
            DWORD Error{ ERROR_SUCCESS };
            PQUERY_CONTEXT QueryContext{ nullptr };
            DNS_QUERY_REQUEST DnsQueryRequest;
            DNS_ADDR_ARRAY DnsServerList;

            /**
            *   Allocate QueryContext
            */
            Error = AllocateQueryContext(&QueryContext);
            if (Error != ERROR_SUCCESS)
            {
                vector<wstring> dns = { L"" };
                auto data = make_shared<Data>(dns, host, move(res));
                data->onError(0, RESULT_CODE::INTERNAL_ERROR);
                log(__FUNCTION__, "Context allocation failed!");
                return;
            }
            memcpy(QueryContext->QueryName, const_cast<wchar_t*>(host.c_str()), host.size() * sizeof(wchar_t));
            QueryContext->QueryType = DNS_TYPE_A;
            QueryContext->QueryOptions = 0;
            QueryContext->RefCount = 0;
            QueryContext->Data = data;
            QueryContext->Ind = ind;
            contextsAllocated_.fetch_add(1, memory_order_relaxed);
            log(__FUNCTION__, "Contexts allocated ---> " + to_string(contextsAllocated_));

            /**
            *   Initiate asynchronous DnsQuery: Note that QueryResults and
            *   QueryCancelContext should be valid till query completes.
            */
            ZeroMemory(&DnsQueryRequest, sizeof(DnsQueryRequest));
            DnsQueryRequest.Version = DNS_QUERY_REQUEST_VERSION1;
            DnsQueryRequest.QueryName = QueryContext->QueryName;
            DnsQueryRequest.QueryType = QueryContext->QueryType;
            DnsQueryRequest.QueryOptions = (ULONG64)QueryContext->QueryOptions;
            DnsQueryRequest.pQueryContext = QueryContext;
            DnsQueryRequest.pQueryCompletionCallback = QueryCompleteCallback;

            /**
            *   Increase reference count in order to avoid context dereferencing in the middle.
            */
            AddReferenceQueryContext(QueryContext);

            size_t ind = 0;
            size_t maxCount = dns.empty() ? 1 : dns.size();
        
            /**
            *   If user specifies server, construct DNS_ADDR_ARRAY
            */
            const wstring addr = dns.empty() ? L"" : dns[ind];
            if (!addr.empty())
            {
                Error = CreateDnsServerList(const_cast<wchar_t*>(addr.c_str()), &DnsServerList);

                if (Error != ERROR_SUCCESS)
                {
                    log(__FUNCTION__, "CreateDnsServerList() failed!");
                }

                DnsQueryRequest.pDnsServerList = &DnsServerList;
            }

            log(__FUNCTION__, "Async DnsQueryEx() call for dns " + boost::locale::conv::utf_to_utf<char>(addr) + " host " + boost::locale::conv::utf_to_utf<char>(host));
            Error = DnsQueryEx(&DnsQueryRequest,
                &QueryContext->QueryResults,
                &QueryContext->QueryCancelContext);

            /**
            *   If DnsQueryEx() returns  DNS_REQUEST_PENDING, Completion routine
            *   will be invoked. If not (when completed inline) completion routine
            *   will not be invoked.
            */
            if (Error != DNS_REQUEST_PENDING)
            {
                log(__FUNCTION__, "Ñallback is called synchroneously.");
                QueryCompleteCallback(QueryContext, &QueryContext->QueryResults);
            }
        } while (++ind < maxCount);
    }
};

atomic_int DnsResolver::Impl::contextsAllocated_ = 0;
atomic_int DnsResolver::Impl::contextsDeleted_ = 0;

void Windscribe::DnsResolver::Lookup(const wstring& host, const vector<wstring>& dns, promise<DataPtr> res)
{
    pImpl_->Lookup(host, dns, move(res));
}

/** PIMPL stuff */
void Windscribe::DnsResolver::ImplDeleter::operator()(DnsResolver::Impl* ptr) const { delete ptr; }

Windscribe::DnsResolver::Data::Data(const vector<wstring>& dns, const wstring& host, promise<DataPtr> promise)
    : host_(host), dns_(dns), promise_(move(promise))
{
    createdCount.fetch_add(1, memory_order_relaxed);
    log(__FUNCTION__, to_string(createdCount));
    if (!dns.empty())
        ips_.resize(dns.size());
    else
        ips_.resize(DEFAULT_DNS_SIZE); // @todo avoid hard-code.
    maxCount_ = static_cast<int>(ips_.size());
}

Windscribe::DnsResolver::Data::~Data()
{
    deletedCount.fetch_add(1, memory_order_relaxed);
    log(__FUNCTION__, to_string(deletedCount));
}

void Windscribe::DnsResolver::Data::onError(int ind, RESULT_CODE code)
{
    DnsResolver::log(__FUNCTION__, to_string(ind) + " " + DnsResolver::toString(code));
    if (ind < ips_.size() && code != RESULT_CODE::SUCCESS) {
        ips_[ind] = ResIp(L"", code);
        processedCount_.fetch_add(1, memory_order_seq_cst);
        if (processedCount_ == maxCount_)
            onFinish();
    }
}

void Windscribe::DnsResolver::Data::onIpResolved(int ind, wstring&& ip)
{
    DnsResolver::log(__FUNCTION__, to_string(ind) + " " + boost::locale::conv::utf_to_utf<char>(ip));
    if (ind <= ips_.size()) {
        ips_[ind] = ResIp(ip); // @todo Provide move-ctor for ResIp.
        processedCount_.fetch_add(1, memory_order_seq_cst);
        if (processedCount_ == maxCount_)
            onFinish();
    }
}

void Windscribe::DnsResolver::Data::onFinish()
{
    log(__FUNCTION__);
    promise_.set_value(shared_from_this());
}

void Windscribe::DnsResolver::Data::print(int id) const
{
    wstring res = L"HOST(";
    res += to_wstring(id);
    res += L") ";
    res += host_;
    BOOST_LOG_TRIVIAL(debug) << res;
    for (const auto& ip : ips_) {
        res = L"\t";
        res += boost::locale::conv::utf_to_utf<wchar_t>(DnsResolver::toString(ip.resCode));
        res += L" ";
        res += ip.ip;
        BOOST_LOG_TRIVIAL(debug) << res;
    }
}

atomic_int Windscribe::DnsResolver::Data::createdCount = 0;
atomic_int Windscribe::DnsResolver::Data::deletedCount = 0;
