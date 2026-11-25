#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "http_client.h"
#include "gw2_logic.h"
#include "version.h"
#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>
#include <windows.h>
#include <winhttp.h>

namespace gw2qh {

namespace {

std::string QueryHeaderUtf8(HINTERNET hRequest, DWORD queryInfo) {
    DWORD size = 0;
    WinHttpQueryHeaders(hRequest, queryInfo, WINHTTP_HEADER_NAME_BY_INDEX, nullptr, &size, WINHTTP_NO_HEADER_INDEX);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || size == 0) return "";
    std::wstring wbuf(size / sizeof(wchar_t), L'\0');
    if (!WinHttpQueryHeaders(hRequest, queryInfo, WINHTTP_HEADER_NAME_BY_INDEX, wbuf.data(), &size, WINHTTP_NO_HEADER_INDEX)) {
        return "";
    }
    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wbuf.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (utf8Len <= 1) return "";
    std::string out(static_cast<size_t>(utf8Len - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wbuf.c_str(), -1, out.data(), utf8Len, nullptr, nullptr);
    return out;
}

bool QueryHeaderSystemTime(HINTERNET hRequest, DWORD queryInfo, uint64_t& outFileTime100ns) {
    SYSTEMTIME st{};
    DWORD size = sizeof(st);
    if (!WinHttpQueryHeaders(
            hRequest,
            queryInfo | WINHTTP_QUERY_FLAG_SYSTEMTIME,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &st,
            &size,
            WINHTTP_NO_HEADER_INDEX)) {
        return false;
    }
    FILETIME ft{};
    if (!SystemTimeToFileTime(&st, &ft)) return false;
    outFileTime100ns = (static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
    return true;
}

std::wstring ToWide(const std::string& utf8) {
    if (utf8.empty()) return L"";
    int needed = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (needed <= 0) return L"";
    std::wstring out(static_cast<size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), out.data(), needed);
    return out;
}

struct WinHttpHandle {
    HINTERNET h = nullptr;
    explicit WinHttpHandle(HINTERNET handle = nullptr) : h(handle) {}
    ~WinHttpHandle() {
        if (h) WinHttpCloseHandle(h);
    }
    WinHttpHandle(const WinHttpHandle&) = delete;
    WinHttpHandle& operator=(const WinHttpHandle&) = delete;
    operator HINTERNET() const { return h; }
    explicit operator bool() const { return h != nullptr; }
};

class WinHttpClientImpl final : public IHttpClient {
public:
    HttpResponse GetHttps(const std::string& host,
                          const std::string& pathAndQuery,
                          const std::string& bearerToken,
                          const std::atomic<bool>* cancelFlag) override {
        HttpResponse resp{};
        if (cancelFlag && cancelFlag->load()) {
            resp.error = "Canceled";
            return resp;
        }

        std::wstring userAgent = ToWide(std::string("GW2QuestHelper-NexusAddon/") + GW2QH_VERSION_STR + " (https://raidcore.gg)");
        WinHttpHandle hSession(WinHttpOpen(
            userAgent.c_str(),
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0));
        if (!hSession) {
            resp.error = "WinHttpOpen failed (" + std::to_string(GetLastError()) + ")";
            return resp;
        }

        // Keep timeouts short so addon hot-unload never hangs on network stalls
        WinHttpSetTimeouts(hSession, 5000, 6000, 8000, 10000);

        std::wstring wHost = ToWide(host);
        WinHttpHandle hConnect(WinHttpConnect(hSession, wHost.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0));
        if (!hConnect) {
            resp.error = "WinHttpConnect failed (" + std::to_string(GetLastError()) + ")";
            return resp;
        }

        std::wstring wPath = ToWide(pathAndQuery);
        WinHttpHandle hRequest(WinHttpOpenRequest(
            hConnect,
            L"GET",
            wPath.c_str(),
            nullptr,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            WINHTTP_FLAG_SECURE));
        if (!hRequest) {
            resp.error = "WinHttpOpenRequest failed (" + std::to_string(GetLastError()) + ")";
            return resp;
        }

        std::wstring headers = L"Accept: application/json\r\n";
        if (!bearerToken.empty()) {
            headers += L"Authorization: Bearer " + ToWide(bearerToken) + L"\r\n";
        }

        BOOL sent = WinHttpSendRequest(
            hRequest,
            headers.c_str(),
            static_cast<DWORD>(headers.size()),
            WINHTTP_NO_REQUEST_DATA,
            0,
            0,
            0);
        if (!sent) {
            resp.error = "WinHttpSendRequest failed (" + std::to_string(GetLastError()) + ")";
            return resp;
        }

        if (!WinHttpReceiveResponse(hRequest, nullptr)) {
            resp.error = "WinHttpReceiveResponse failed (" + std::to_string(GetLastError()) + ")";
            return resp;
        }

        DWORD statusCode = 0;
        DWORD statusSize = sizeof(statusCode);
        if (WinHttpQueryHeaders(
                hRequest,
                WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX,
                &statusCode,
                &statusSize,
                WINHTTP_NO_HEADER_INDEX)) {
            resp.statusCode = static_cast<int>(statusCode);
        }

        std::string cacheCtrl = QueryHeaderUtf8(hRequest, WINHTTP_QUERY_CACHE_CONTROL);
        std::string ageHdr    = QueryHeaderUtf8(hRequest, WINHTTP_QUERY_AGE);
        resp.cacheRemainingSec = ParseHttpCacheRemainingSec(cacheCtrl, ageHdr);
        if (resp.cacheRemainingSec < 0) {
            uint64_t dateFt = 0;
            uint64_t expFt  = 0;
            if (QueryHeaderSystemTime(hRequest, WINHTTP_QUERY_DATE, dateFt) &&
                QueryHeaderSystemTime(hRequest, WINHTTP_QUERY_EXPIRES, expFt) &&
                expFt > dateFt) {
                int deltaSec = static_cast<int>((expFt - dateFt) / 10000000ULL);
                resp.cacheRemainingSec = std::clamp(deltaSec, 0, 600);
            }
        }

        std::string body;
        DWORD avail = 0;
        do {
            if (cancelFlag && cancelFlag->load()) {
                resp.error = "Canceled";
                return resp;
            }
            avail = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &avail)) {
                resp.error = "WinHttpQueryDataAvailable failed (" + std::to_string(GetLastError()) + ")";
                return resp;
            }
            if (avail == 0) break;

            std::vector<char> chunk(avail);
            DWORD downloaded = 0;
            if (!WinHttpReadData(hRequest, chunk.data(), avail, &downloaded)) {
                resp.error = "WinHttpReadData failed (" + std::to_string(GetLastError()) + ")";
                return resp;
            }
            body.append(chunk.data(), downloaded);
        } while (avail > 0);

        resp.body = std::move(body);
        return resp;
    }
};

} // namespace

std::unique_ptr<IHttpClient> CreateWinHttpClient() {
    return std::make_unique<WinHttpClientImpl>();
}

} // namespace gw2qh
