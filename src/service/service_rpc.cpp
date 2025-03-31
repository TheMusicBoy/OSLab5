#include "service_rpc.h"

////////////////////////////////////////////////////////////////////////////////

TRpcServer::TRpcServer(const std::string& interfaceIp, const short int port, size_t threadCount)
    : HttpServer_(interfaceIp, port),
      ThreadCount_(threadCount),
      ThreadPool_(NCommon::New<NCommon::TThreadPool>(ThreadCount_))
{}

void TRpcServer::Setup(NService::TServicePtr service) {
    RegisterHandler("GET", "/", NRpc::MakeHandler(&NService::TService::HandleMainPage, MakeWeak(&*service)));
    RegisterHandler("GET", "/assets/.*", NRpc::MakeHandler(&NService::TService::HandleAssets, MakeWeak(&*service)));
    RegisterHandler("GET", "/list/raw", NRpc::MakeHandler(&NService::TService::HandleRawReadings, MakeWeak(&*service)));
    RegisterHandler("GET", "/list/hour", NRpc::MakeHandler(&NService::TService::HandleHourlyAverages, MakeWeak(&*service)));
    RegisterHandler("GET", "/list/day", NRpc::MakeHandler(&NService::TService::HandleDailyAverages, MakeWeak(&*service)));

}

void TRpcServer::Start() {
    for (size_t iter = 0; iter < ThreadCount_; iter++) {
        ThreadPool_->enqueue(NCommon::Bind(
            &TRpcServer::Worker,
            MakeWeak(this)
        ));
    }
}

void TRpcServer::Worker() {
    auto job = NCommon::Bind(&TRpcServer::Job, MakeWeak(this));
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

void TRpcServer::Job() {
    HttpServer_.ProcessClient();
}

////////////////////////////////////////////////////////////////////////////////
