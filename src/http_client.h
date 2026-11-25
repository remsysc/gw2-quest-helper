#pragma once

#include <atomic>
#include <memory>
#include <string>

namespace gw2qh {

struct HttpResponse {
    int         statusCode        = 0;
    std::string body;
    std::string error;
    int         cacheRemainingSec = -1;

    bool ok() const { return statusCode >= 200 && statusCode < 300; }
};

class IHttpClient {
public:
    virtual ~IHttpClient() = default;
    virtual HttpResponse GetHttps(const std::string& host,
                                  const std::string& pathAndQuery,
                                  const std::string& bearerToken = "",
                                  const std::atomic<bool>* cancelFlag = nullptr) = 0;
};

std::unique_ptr<IHttpClient> CreateWinHttpClient();

} // namespace gw2qh
