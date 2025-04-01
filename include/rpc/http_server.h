#pragma once

#include <common/logging.h>
#include <common/exception.h>
#include <common/periodic_executor.h>

#include <nlohmann/json.hpp>

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <regex>

#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#define SOCKET int
#endif

namespace NRpc {

////////////////////////////////////////////////////////////////////////////////

#define READ_WAIT_MS 50
#define DEFAULT_HTTP_VERSION "HTTP/1.1"

enum class EHttpCode {
    Continue           = 100,
    SwitchingProtocols = 101,
    Ok                 = 200,
    Created            = 201,
    Accepted           = 202,
    NonAuthInfo        = 203,
    NoContent          = 204,
    ResetContent       = 205,
    PartialContent     = 206,
    
    MultipleChoices    = 300,
    MovedPermanently   = 301,
    Found              = 302,
    SeeOther           = 303,
    NotModified        = 304,
    TemporaryRedirect  = 307,
    PermanentRedirect  = 308,
    
    BadRequest         = 400,
    Unauthorized       = 401,
    Forbidden          = 403,
    NotFound           = 404,
    MethodNotAllowed   = 405,
    Timeout            = 408,
    Conflict           = 409,
    Gone               = 410,
    
    InternalError      = 500,
    NotImplemented     = 501,
    BadGateway         = 502,
    ServiceUnavailable = 503,
    GatewayTimeout     = 504
};

std::string GetHttpStatusName(EHttpCode code);

////////////////////////////////////////////////////////////////////////////////

class THttpException
    : public NCommon::TException
{
public:
    THttpException(EHttpCode code)
        : NCommon::TException(""), Code_(code)
    {}

    template<typename... Args>
    THttpException(EHttpCode code, const std::string& format, Args&&... args)
        : NCommon::TException(format, std::forward<Args>(args)...), Code_(code)
    {}

    EHttpCode HttpCode() const {
        return Code_;
    }

private:
    EHttpCode Code_;

};

////////////////////////////////////////////////////////////////////////////////

class TRequest;

struct TResponse {
    EHttpCode HttpStatus;
    std::unordered_map<std::string, std::string> Headers;
    std::string Body;

    TResponse& SetStatus(EHttpCode httpStatus);
    TResponse& SetRaw(const std::string& data);
    TResponse& SetHeader(const std::string& key, const std::string& value);

    TResponse& SetJson(const nlohmann::json& data);
    TResponse& SetText(const std::string& data);
    TResponse& SetHtml(const std::filesystem::path& path);

    template <typename Asset>
    TResponse& SetAsset(const Asset& asset) {
        SetRaw(asset.Data);
        SetHeader("Content-Type", asset.Mime);
        return *this;
    }
};

bool IsAcceptType(const NRpc::TRequest& request, const std::string& type);

template <typename Callable, typename... Args>
auto MakeHandler(Callable&& cb, Args&&... args) {
    return [
        callable = std::forward<Callable>(cb),
        bound_args = std::make_tuple(std::forward<Args>(args)...)
    ](const TRequest& request) -> TResponse {
        try {
            return std::apply([&](auto&&... items) {
                return std::invoke(callable, ConvertWeakPtr(items)..., request);
            }, bound_args);
        } catch (NCommon::TExpiredWeakPtr) {
            return {};
        } catch (std::exception& ex) {
            throw;
        }
    };
}


class THandlerBase {
protected:
    std::string Version_;
    std::unordered_map<std::string, std::string> DefaultHeaders_;

public:
    THandlerBase(const TRequest &request);

    THandlerBase(const std::string& version = DEFAULT_HTTP_VERSION);

    THandlerBase SetVersion(const std::string& version);
    THandlerBase SetContentType(const std::string& contentType);
    std::string FormatResponse(const TResponse& response) const;
};

template <typename Handler>
concept CIsHandler = std::is_base_of_v<THandlerBase, Handler>;

class THandler
    : public THandlerBase
{
protected:
    std::string Method_;
    std::string Url_;
    std::function<TResponse(const TRequest&)> BodyFunc_;
    bool IsRaw_;

public:
    template <typename BodyFunc>
    THandler(const std::string& method, const std::string& url, BodyFunc&& bodyFunc, bool isRaw = false)
        : THandlerBase(),
          Method_(method),
          Url_(url),
          BodyFunc_(std::forward<BodyFunc>(bodyFunc)),
          IsRaw_(isRaw)
    {}

    TResponse GetResponse(const TRequest& request);
    std::string GetAnswer(const TRequest& request);
    const std::string& GetMethod() const;
    const std::string& GetURL() const;
    
    bool IsRaw();
};

class TErrorHandler
    : public THandlerBase {
public:
    TErrorHandler(const std::string& errorPage);

    std::string GetAnswer() const;

private:
    std::string ErrorPage_;
    
};

////////////////////////////////////////////////////////////////////////////////

class TRequest {
    std::string Method_;
    std::string Url_;

    std::string Version_;
    std::unordered_map<std::string, std::string> Headers_;
    std::unordered_map<std::string, std::string> UrlArgs_;

public:
    TRequest(const std::string& recieved_data);

    template <typename Handler>
    requires(CIsHandler<Handler>)
    bool CheckResponse(const Handler& response) const {
        return response.GetMethod() == GetMethod() && std::regex_match(GetURL(), std::regex(response.GetURL()));
    }

    std::string GetMethod() const;
    std::string GetURL() const;
    std::string GetVersion() const;
    std::string GetHeader(const std::string& key) const;

};

////////////////////////////////////////////////////////////////////////////////

class TSocketBase {
public:
    TSocketBase();
    ~TSocketBase();

    static int ErrorCode();
    bool IsValid();

protected:
    void CloseSocket();
    static void CloseSocket(SOCKET sock);
    static int Poll(const SOCKET& socket, int timeout_ms = READ_WAIT_MS);

    SOCKET Socket_;
};

class THttpServer
    : public TSocketBase
{
public:
    THttpServer(const std::string& interfaceIp, const short int port);

    void Listen(const std::string& interfaceIp, const short int port);
    void ProcessClient();

    template <typename Handler>
    requires(CIsHandler<Handler>)
    void RegisterHandler(const Handler& handler) {
        Handlers_.emplace_back(handler);
    }

private:
    char InputBuf_[1024];

    std::vector<THandler> Handlers_;
    TErrorHandler ErrorHandler_;
};

}
