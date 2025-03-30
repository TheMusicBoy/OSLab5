#include <service/service.h>
#include <service/service_rpc.h>

#include <common/logging.h>
#include <common/exception.h>
#include <common/getopts.h>

#include <iostream>

int main(int argc, const char* argv[]) {
    NCommon::GetOpts opts;
    opts.AddOption('h', "help", "Show help message");
    opts.AddOption('v', "version", "Show version information");
    opts.AddOption('c', "config", "Path to config", true);
    opts.AddOption('a', "accelerate", "Acceleration of time", true);

    try {
        opts.Parse(argc, argv);

        if (opts.Has('h') || opts.Has("help")) {
            std::cerr << opts.Help();
            return 0;
        }

        if (opts.Has('v') || opts.Has("version")) {
            std::cerr << "Lab3 Service v2.0\n";
            return 0;
        }

        ASSERT(opts.Has('c') || opts.Has("config"), "Config is required");
        NConfig::TConfigPtr config = NCommon::New<NConfig::TConfig>();
        config->LoadFromFile(opts.Get("config"));

        for (const auto& c : config->LogDestinations) {
            auto fileHandler = NLogging::CreateFileHandler(c->Path);
            fileHandler->SetLevel(c->Level);
            NLogging::GetLogManager().AddHandler(fileHandler);
        }

        std::function<std::optional<TReading>(double)> processor;

        if (opts.Has('a') || opts.Has("accelerate")) {
            double boost = 1;

            if (opts.Has('a')) boost = std::stod(opts.Get('a'));
            if (opts.Has("accelerate")) boost = std::stod(opts.Get("accelerate"));

            auto startTime = std::chrono::system_clock::now();

            processor = [startTime, boost](double value) -> std::optional<TReading> {
                if (value < -100 || value > 100) {
                    return {};
                }

                auto difference = std::chrono::system_clock::now() - startTime;
                difference *= boost;
                return TReading(startTime + difference, value);
            };
        } else {
            processor = [](double value) -> std::optional<TReading> {
                if (value < -100 || value > 100) {
                    return {};
                }

                return TReading(std::chrono::system_clock::now(), value);
            };
        }

        TRpcServerPtr server = NCommon::New<TRpcServer>("0.0.0.0", 8080, 5);

        auto service = NCommon::New<NService::TService>(config, processor);

        service->Start();
        
        server->RegisterHandler("GET", "/", NRpc::MakeHandler(&NService::TService::HandleMainPage, MakeWeak(&*service)));
        server->RegisterHandler("GET", "/assets/.*", NRpc::MakeHandler(&NService::TService::HandleAssets, MakeWeak(&*service)));

        server->RegisterHandler("GET", "/list/raw", NRpc::MakeHandler(&NService::TService::HandleRawReadings, MakeWeak(&*service)));
        server->RegisterHandler("GET", "/list/hour", NRpc::MakeHandler(&NService::TService::HandleHourlyAverages, MakeWeak(&*service)));
        server->RegisterHandler("GET", "/list/day", NRpc::MakeHandler(&NService::TService::HandleDailyAverages, MakeWeak(&*service)));

        server->Start();

        LOG_INFO("Service started. Press Ctrl+C to exit.");
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    } catch (const NCommon::TException& ex) {
        LOG_ERROR("Error: {}", ex.what());
        return 1;
    }
    
    return 0;
}
