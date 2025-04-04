#include <rpc/service_rpc.h>

namespace NRpc {

////////////////////////////////////////////////////////////////////////////////

TRpcServerBase::TRpcServerBase(const std::string& interfaceIp, const short int port, size_t threadCount)
    : HttpServer_(interfaceIp, port),
      ThreadCount_(threadCount),
      ThreadPool_(NCommon::New<NCommon::TThreadPool>(ThreadCount_))
{}

void TRpcServerBase::Start() {
    for (size_t iter = 0; iter < ThreadCount_; iter++) {
        ThreadPool_->enqueue(NCommon::Bind(
            &TRpcServerBase::Worker,
            MakeWeak(this)
        ));
    }
}

void TRpcServerBase::Worker() {
    auto job = NCommon::Bind(&TRpcServerBase::Job, MakeWeak(this));
    while (true) {
        try {
            if (job()) {
                break;
            }
        } catch (const std::exception& ex) {
            LOG_ERROR("Error in worker: {}", ex.what());
        }
    }
}

void TRpcServerBase::Job() {
    HttpServer_.ProcessClient();
}

////////////////////////////////////////////////////////////////////////////////

} // namespace NRpc
