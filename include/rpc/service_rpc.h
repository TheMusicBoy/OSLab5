#pragma once

#include <rpc/http_server.h>

#include <common/intrusive_ptr.h>
#include <common/weak_ptr.h>
#include <common/periodic_executor.h>
#include <common/threadpool.h>

namespace NRpc {

////////////////////////////////////////////////////////////////////////////////

class TRpcServerBase
    : public NRefCounted::TRefCountedBase {
public:
    TRpcServerBase(const std::string& interfaceIp, const short int port, size_t threadCount);

    void Start();

protected:
    template<typename HandlerFunc>
    void RegisterHandler(const std::string& method, const std::string& url, HandlerFunc&& handler, bool isRaw = false) {
        auto wrappedHandler = [handler = std::forward<HandlerFunc>(handler)](const NRpc::TRequest& req) {
            try {
                return handler(req);
            } catch (const NRpc::THttpException& ex) {
                return NRpc::TResponse()
                    .SetStatus(ex.HttpCode())
                    .SetJson(ex.what());
            } catch (const std::exception& ex) {
                LOG_ERROR("Handler error for {} {}: {}", req.GetMethod(), req.GetURL(), ex.what());
                return NRpc::TResponse()
                    .SetStatus(NRpc::EHttpCode::InternalError)
                    .SetText("Internal Server Error");
            } catch (...) {
                LOG_ERROR("Unknown error in handler for {} {}", req.GetMethod(), req.GetURL());
                return NRpc::TResponse()
                    .SetStatus(NRpc::EHttpCode::InternalError)
                    .SetText("Internal Server Error");
            }
        };
        
        HttpServer_.RegisterHandler(NRpc::THandler(method, url, wrappedHandler, isRaw));
    }

    template<typename HandlerFunc>
    void RegisterNotFoundHandler(HandlerFunc&& handler) {
        auto wrappedHandler = [handler = std::forward<HandlerFunc>(handler)](const NRpc::TRequest& req) {
            try {
                return handler(req);
            } catch (const NRpc::THttpException& ex) {
                return NRpc::TResponse()
                    .SetStatus(ex.HttpCode())
                    .SetJson(ex.what());
            } catch (const std::exception& ex) {
                LOG_ERROR("Handler error for {} {}: {}", req.GetMethod(), req.GetURL(), ex.what());
                return NRpc::TResponse()
                    .SetStatus(NRpc::EHttpCode::InternalError)
                    .SetText("Internal Server Error");
            } catch (...) {
                LOG_ERROR("Unknown error in handler for {} {}", req.GetMethod(), req.GetURL());
                return NRpc::TResponse()
                    .SetStatus(NRpc::EHttpCode::InternalError)
                    .SetText("Internal Server Error");
            }
        };
        
        HttpServer_.SetNotFoundHandler(NRpc::TUnifiedHandler(wrappedHandler));
    }

    void Worker();

    void Job();

    NRpc::THttpServer HttpServer_;

    size_t ThreadCount_;
    NCommon::TThreadPoolPtr ThreadPool_;

    inline static const std::string LoggingSource = "Rpc";

};

////////////////////////////////////////////////////////////////////////////////

} // namespace NRpc
