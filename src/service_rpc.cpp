#include <service_rpc.h>


////////////////////////////////////////////////////////////////////////////////

TRpcServer::TRpcServer(const std::string& interfaceIp, const short int port, size_t threadCount)
    : HttpServer_(interfaceIp, port),
      ThreadCount_(threadCount),
      ThreadPool_(NCommon::New<NCommon::TThreadPool>(ThreadCount_))
{}

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
    while (!job()); 
}

void TRpcServer::Job() {
    HttpServer_.ProcessClient();
}
