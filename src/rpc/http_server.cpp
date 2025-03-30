#include "assets.h"
#include <rpc/http_server.h>

#include <common/format.h>
#include <common/exception.h>
#include <common/logging.h>

#include <filesystem>
#include <regex>
#include <thread>
#include <signal.h>

#if defined(_WIN32) || defined(_WIN64)
#else
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <poll.h>
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#endif

namespace NRpc {

namespace {

////////////////////////////////////////////////////////////////////////////////

inline const std::string LoggingSource = "HttpServer";

std::vector<std::string> Split(const std::string& s, const std::string& delimiter, size_t limit = 0) {
    std::vector<std::string> tokens;
    size_t pos_start = 0;
    const size_t delimiter_length = delimiter.length();

    if (delimiter_length == 0) {
        tokens.push_back(s);
        return tokens;
    }

    while (true) {
        size_t pos_end = s.find(delimiter, pos_start);
        bool reached_limit = (limit > 0) && (tokens.size() + 1 >= limit);

        if (reached_limit || pos_end == std::string::npos) {
            tokens.push_back(s.substr(pos_start));
            break;
        }

        tokens.push_back(s.substr(pos_start, pos_end - pos_start));
        pos_start = pos_end + delimiter_length;
    }

    return tokens;
}

std::string Trim(const std::string& s) {
    const char* whitespace = " \t\r\n";
    size_t start = s.find_first_not_of(whitespace);
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(whitespace);
    return s.substr(start, end - start + 1);
}

bool IsEmptyLine(const std::string& s) {
    return s.find_first_not_of("\r\n") == std::string::npos;
}

std::string ReadFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    ASSERT(file.is_open(), "Failed to open file: {}", path.string());
    
    const auto file_size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::string content;
    content.resize(static_cast<size_t>(file_size));
    
    ASSERT(file.read(&content[0], file_size), "Error reading contents from: {}", path.string());
    return content;
}

////////////////////////////////////////////////////////////////////////////////

} // namespace

////////////////////////////////////////////////////////////////////////////////

std::string GetHttpStatusName(EHttpCode code) {
    switch(code) {
        case EHttpCode::Continue:           return "Continue";
        case EHttpCode::SwitchingProtocols: return "Switching Protocols";
        case EHttpCode::Ok:                 return "OK";
        case EHttpCode::Created:            return "Created";
        case EHttpCode::Accepted:           return "Accepted";
        case EHttpCode::NonAuthInfo:        return "Non-Authoritative Information";
        case EHttpCode::NoContent:          return "No Content";
        case EHttpCode::ResetContent:       return "Reset Content";
        case EHttpCode::PartialContent:     return "Partial Content";
        
        case EHttpCode::MultipleChoices:    return "Multiple Choices";
        case EHttpCode::MovedPermanently:   return "Moved Permanently";
        case EHttpCode::Found:              return "Found";
        case EHttpCode::SeeOther:           return "See Other";
        case EHttpCode::NotModified:        return "Not Modified";
        case EHttpCode::TemporaryRedirect:  return "Temporary Redirect";
        case EHttpCode::PermanentRedirect:  return "Permanent Redirect";
        
        case EHttpCode::BadRequest:         return "Bad Request";
        case EHttpCode::Unauthorized:       return "Unauthorized";
        case EHttpCode::Forbidden:          return "Forbidden";
        case EHttpCode::NotFound:           return "Not Found";
        case EHttpCode::MethodNotAllowed:   return "Method Not Allowed";
        case EHttpCode::Timeout:            return "Request Timeout";
        case EHttpCode::Conflict:           return "Conflict";
        case EHttpCode::Gone:               return "Gone";
        
        case EHttpCode::InternalError:      return "Internal Server Error";
        case EHttpCode::NotImplemented:     return "Not Implemented";
        case EHttpCode::BadGateway:         return "Bad Gateway";
        case EHttpCode::ServiceUnavailable: return "Service Unavailable";
        case EHttpCode::GatewayTimeout:     return "Gateway Timeout";
        
        default:                            return "Unknown Status Code";
    }
}

////////////////////////////////////////////////////////////////////////////////

TResponse& TResponse::SetStatus(EHttpCode httpStatus) {
    HttpStatus = httpStatus;
    return *this;
}

TResponse& TResponse::SetHeader(const std::string& key, const std::string& value) {
    Headers[key] = value;
    return *this;
}

TResponse& TResponse::SetRaw(const std::string& data) {
    Body = data;
    Headers["Content-Length"] = std::to_string(Body.size());
    return *this;
}


TResponse& TResponse::SetJson(const nlohmann::json& data) {
    Body = data.dump();
    Headers["Content-Type"] = "application/json";
    Headers["Content-Length"] = std::to_string(Body.size());
    return *this;
}

TResponse& TResponse::SetText(const std::string& data) {
    Body = data;
    Headers["Content-Type"] = "text/plain";
    Headers["Content-Length"] = std::to_string(Body.size());
    return *this;
}

TResponse& TResponse::SetHtml(const std::filesystem::path& path) {
    Body = ReadFile(path);
    Headers["Content-Type"] = "text/html";
    Headers["Content-Length"] = std::to_string(Body.size());
    return *this;
}

////////////////////////////////////////////////////////////////////////////////

TRequest::TRequest(const std::string& recievedData) {
    std::istringstream recv(recievedData);
    std::string line;
    std::getline(recv, line);

    auto split = Split(Trim(line), " ");
    auto urlSplit = Split(split[1], "?");

    Method_ = split[0];
    Url_ = urlSplit[0];
    if (urlSplit.size() > 1)
    {
        for (auto args : Split(urlSplit[1], "&"))
        {
            auto sp = Split(args, "=");
            UrlArgs_[sp[0]] = sp[1];
        }
    }
    Version_ = split[2];

    while (true) {
        if (!std::getline(recv, line) || line == "\r" || line == "\r\n") {
            break;
        }
        split = Split(Trim(line), ": ");
        Headers_[split[0]] = split[1];
    }
}

std::string TRequest::GetMethod() const {
    return Method_;
}

std::string TRequest::GetURL() const {
    return Url_;
}

std::string TRequest::GetVersion() const {
    return Version_;
}

std::string TRequest::GetHeader(const std::string& key) const {
    return Headers_.at(key);
}

////////////////////////////////////////////////////////////////////////////////

THandlerBase::THandlerBase(const std::string& version)
    : Version_(version)
{}

THandlerBase::THandlerBase(const TRequest& request)
    : THandlerBase()
{
    Version_ = request.GetVersion();
}

THandlerBase THandlerBase::SetVersion(const std::string& version) {
    Version_ = version;
    return *this;
}

std::string THandlerBase::FormatResponse(const TResponse& response) const {
    std::ostringstream headers;
    for (const auto& [name, value] : response.Headers) {
        headers << name << ": " << value << "\r\n";
    }

    return NCommon::Format(
        "{} {} {}\r\n{}\r\n{}",
        Version_, uint32_t(response.HttpStatus), GetHttpStatusName(response.HttpStatus),
        headers.str(), response.Body);
}

////////////////////////////////////////////////////////////////////////////////

TResponse THandler::GetResponse(const TRequest& request) {
    if (BodyFunc_ == NULL) {
        return TResponse().SetStatus(EHttpCode::NotImplemented).SetJson({{"message", "Handler not found"}});
    }
    return BodyFunc_(request);
}

std::string THandler::GetAnswer(const TRequest& request) {
    return THandlerBase::FormatResponse(GetResponse(request));
}

const std::string& THandler::GetMethod() const {
    return Method_;
}

const std::string& THandler::GetURL() const {
    return Url_;
}

bool THandler::IsRaw() {
    return IsRaw_;
}

////////////////////////////////////////////////////////////////////////////////

TErrorHandler::TErrorHandler(const std::string& errorPage)
    : THandlerBase(), ErrorPage_(errorPage)
{}

std::string TErrorHandler::GetAnswer() const {
    if (ErrorPage_ != "") {
        return THandlerBase::FormatResponse(
            TResponse()
                .SetStatus(EHttpCode::NotFound)
                .SetRaw(ErrorPage_)
                .SetHeader("Content-Type", "text/html")
        );
    }
    return THandlerBase::FormatResponse(
        TResponse()
            .SetStatus(EHttpCode::NotFound)
            .SetRaw("")
    );
}

////////////////////////////////////////////////////////////////////////////////

TSocketBase::TSocketBase()
    : Socket_(INVALID_SOCKET)
{
#if defined(_WIN32) || defined(_WIN64)
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#else
    signal(SIGPIPE, SIG_IGN);
#endif
}

TSocketBase::~TSocketBase() {
    CloseSocket();
#if defined(WIN32)
    WSACleanup();
#endif
}

int TSocketBase::ErrorCode() {
#ifdef WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

bool TSocketBase::IsValid() {
    return Socket_ != INVALID_SOCKET;
}

void TSocketBase::CloseSocket() {
    CloseSocket(Socket_);
    Socket_ = INVALID_SOCKET;
}

void TSocketBase::CloseSocket(SOCKET sock) {
    if (sock == INVALID_SOCKET)
        return;
#ifdef WIN32
    shutdown(sock, SD_SEND);
    closesocket(sock);
#else
    shutdown(sock, SHUT_WR);
    close(sock);
#endif
}

int TSocketBase::Poll(const SOCKET& socket, int timeoutMs) {
    struct pollfd polstr;
    memset(&polstr, 0, sizeof(polstr));
    polstr.fd = socket;
    polstr.events |= POLLIN;
#ifdef WIN32
    return WSAPoll(&polstr, 1, timeout_ms);
#else
    return poll(&polstr, 1, timeoutMs);
#endif
}

////////////////////////////////////////////////////////////////////////////////

THttpServer::THttpServer(const std::string& interfaceIp, const short int port)
    : ErrorHandler_(NDetail::NotFoundPage)
{
    Listen(interfaceIp, port);
    LOG_INFO("Successfuly start listening...");
}

void THttpServer::Listen(const std::string& interfaceIp, const short int port) {
    try {
        if (Socket_ != INVALID_SOCKET) {
            CloseSocket();
        }

        struct addrinfo hints;
        memset(&hints, 0, sizeof(hints));

        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        hints.ai_flags = AI_PASSIVE;

        struct addrinfo *addr = NULL;
        int res = getaddrinfo(interfaceIp.c_str(), std::to_string(port).c_str(), &hints, &addr);

        if (res != 0) {
            freeaddrinfo(addr);
            THROW("Failed getaddrinfo: {}", res);
        }

        Socket_ = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
        bool needFree = true;
        if (Socket_ == INVALID_SOCKET) {
            LOG_ERROR("Cant open socket: {}", ErrorCode());
            freeaddrinfo(addr);
            needFree = false;
        }

        if (bind(Socket_, addr->ai_addr, addr->ai_addrlen) == SOCKET_ERROR) {
            LOG_ERROR("Failed to bind: {}", ErrorCode());
            freeaddrinfo(addr);
            CloseSocket();
            needFree = false;
        }

        if (needFree) {
            freeaddrinfo(addr);
        }

        if (listen(Socket_, SOMAXCONN) == SOCKET_ERROR) {
            CloseSocket();
            THROW("Failed to start listen: {}", ErrorCode());
        }
    } catch (std::exception& ex) {
        LOG_ERROR("Failed to listen: {}", ex);
        std::this_thread::sleep_for(std::chrono::seconds(1));
        Listen(interfaceIp, port);
    }
}

////////////////////////////////////////////////////////////////////////////////

void THttpServer::ProcessClient() {
    if (!IsValid()) {
        LOG_ERROR("Server (listening) socket is invalid!");
        return;
    }

    // Ожидает пока не примет запрос
    SOCKET clientSocket = accept(Socket_, NULL, NULL);
    if (clientSocket == INVALID_SOCKET) {
        LOG_ERROR("Error accepting client: {}", ErrorCode());
        CloseSocket(clientSocket);
        return;
    }

    if (Poll(clientSocket) <= 0) {
        CloseSocket(clientSocket);
        return;
    }

    std::stringstream recivedString;

    int bufSize = sizeof(InputBuf_) - 1;

    // Читаем поток днных
    int result = -1;
    do {
        result = recv(clientSocket, InputBuf_, bufSize, 0);
        if (result < 0) {
            break;
        }
        InputBuf_[result] = '\0';
        recivedString << InputBuf_;
    } while (result >= bufSize);

    if (result == SOCKET_ERROR || result < 0) {
        LOG_ERROR("Error on client receive: {}", ErrorCode());
        CloseSocket(clientSocket);
        return;
    }
    else if (result == 0) {
        LOG_ERROR("Client closed connection before getting any data!");
        CloseSocket(clientSocket);
        return;
    }

    auto request = TRequest(recivedString.str());

    std::cout << request.GetMethod() << " " << request.GetURL() << std::endl;

    int index = -1;
    for (uint64_t i = 0; i < Handlers_.size(); ++i) {
        if (request.CheckResponse(Handlers_[i])) {
            index = i;
            break;
        }
    }

    std::string response;
    if (index != -1) {
        if (Handlers_[index].IsRaw()) {
            response = Handlers_[index].GetResponse(request).Body;
        } else {
            response = Handlers_[index].GetAnswer(request);
        }
    } else {
        response = ErrorHandler_.GetAnswer();
    }

    LOG_INFO("Request: {}", recivedString.str());
    LOG_INFO("Response: {}", response);

    result = send(clientSocket, response.c_str(), (int)response.length(), 0);
    if (result == SOCKET_ERROR) {
        LOG_ERROR("Failed to send responce to client: {}", ErrorCode());
    }

    CloseSocket(clientSocket);
}

////////////////////////////////////////////////////////////////////////////////

} // namespace NRpc
