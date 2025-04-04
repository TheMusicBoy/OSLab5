#include <ui/service_rpc.h>

////////////////////////////////////////////////////////////////////////////////

void TRpcServer::Setup(NService::TServicePtr service) {
    RegisterNotFoundHandler(NRpc::MakeHandler(&NService::TService::HandleNotFoundPage, MakeWeak(&*service)));
    RegisterHandler("GET", "/", NRpc::MakeHandler(&NService::TService::HandleMainPage, MakeWeak(&*service)));
    RegisterHandler("GET", "/assets/.*", NRpc::MakeHandler(&NService::TService::HandleAssets, MakeWeak(&*service)));
    RegisterHandler("GET", "/list/raw", NRpc::MakeHandler(&NService::TService::HandleRawReadings, MakeWeak(&*service)));
    RegisterHandler("GET", "/list/hour", NRpc::MakeHandler(&NService::TService::HandleHourlyAverages, MakeWeak(&*service)));
    RegisterHandler("GET", "/list/day", NRpc::MakeHandler(&NService::TService::HandleDailyAverages, MakeWeak(&*service)));
}

////////////////////////////////////////////////////////////////////////////////
