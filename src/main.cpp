#include <service/service.h>
#include <service/service_rpc.h>

#include <common/logging.h>
#include <common/exception.h>
#include <common/getopts.h>

#include <iostream>

namespace {

////////////////////////////////////////////////////////////////////////////////

void SetupLogging(NConfig::TConfigPtr config) {
    for (const auto& dst : config->LogDestinations) {
        auto fileHandler = NLogging::CreateFileHandler(dst->Path);
        fileHandler->SetLevel(dst->Level);
        NLogging::GetLogManager().AddHandler(fileHandler);
    }
}

////////////////////////////////////////////////////////////////////////////////

} // namespace

int main(int argc, const char* argv[]) {
    NCommon::GetOpts opts;
    opts.AddOption('h', "help", "Show help message");
    opts.AddOption('v', "version", "Show version information");
    opts.AddOption('c', "config", "Path to config", true);
    opts.AddOption('a', "accelerate", "Acceleration of time", true);

    try {
        opts.Parse(argc, argv);

        if (opts.Has("help")) {
            std::cerr << opts.Help();
            return 0;
        }

        if (opts.Has("version")) {
            std::cerr << "Lab5 Service v1.2\n";
            return 0;
        }

        ASSERT(opts.Has("config"), "Config is required");
        NConfig::TConfigPtr config = NCommon::New<NConfig::TConfig>();
        config->LoadFromFile(opts.Get("config"));
        SetupLogging(config);

        std::function<std::optional<TReading>(double)> processor;

        if (opts.Has("accelerate")) {
            double boost = std::stod(opts.Get("accelerate"));

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
        server->Setup(service);
        server->Start();
        service->Start();

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
