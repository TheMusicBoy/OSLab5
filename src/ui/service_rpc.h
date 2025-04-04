#pragma once

#include <ui/service.h>

#include <rpc/service_rpc.h>

////////////////////////////////////////////////////////////////////////////////

class TRpcServer
    : public NRpc::TRpcServerBase {
public:
    using NRpc::TRpcServerBase::TRpcServerBase;

    void Setup(NService::TServicePtr service);

};

DECLARE_REFCOUNTED(TRpcServer);

////////////////////////////////////////////////////////////////////////////////
