#include <service/config.h>

#include <ipc/serial_port.h>
#include <ipc/decode_encode.h>
#include <common/logging.h>
#include <common/getopts.h>

#include <chrono>
#include <thread>
#include <cmath>
#include <random>

class TSimulator {
public:
    TSimulator(NConfig::TSimulatorConfigPtr config)
        : Config_(config),
          BaseTemp(20.0),
          Amplitude(15.0),
          Period(std::chrono::seconds(60)) 
    {}

    void Run() {
        auto port = NCommon::New<NIpc::TComPort>(Config_->SerialConfig);
        port->Open();
        auto format = NDecode::ParseTemperatureFormat(Config_->SerialConfig->Format);
        auto encoder = NDecode::CreateEncoder(format);
        encoder->SetComPort(port);
        
        LOG_INFO("Temperature simulator started on {}", Config_->SerialConfig->SerialPort);

        try {
            while (true) {
                auto now = std::chrono::system_clock::now();
                double temp = CalculateSimulatedTemp(now);
                encoder->WriteTemperature(temp);
                LOG_INFO("Sent temperature: {}C", temp);
                std::this_thread::sleep_for(std::chrono::milliseconds(Config_->DelayMs));
            }
        } catch (...) {
            port->Close();
            throw;
        }
    }

private:
    double CalculateSimulatedTemp(std::chrono::system_clock::time_point now) {
        // Get duration in seconds with multiplier
        auto real_duration = now.time_since_epoch();
        double sec = duration_cast<std::chrono::duration<double>>(real_duration).count();
        double simulated_time = sec * Config_->TimeMultiplier;

        // Calculate seasonal variation (1 year period)
        const double yearly_period = 365.2425 * 24 * 3600; 
        double season_factor = std::sin((2 * M_PI * simulated_time) / yearly_period);
        
        // Calculate daily variation (24-hour period)
        const double daily_period = 24 * 3600;
        double daily_phase = std::fmod(simulated_time, daily_period);
        double daily_factor = std::sin((2 * M_PI * daily_phase) / daily_period);
        
        // Calculate current simulated day
        int current_day = static_cast<int>(simulated_time / daily_period);
        if(current_day != last_simulated_day_) {
            current_daily_offset_ = daily_offset_dist_(gen_);
            last_simulated_day_ = current_day;
        }

        // Combine components
        double temp = (BaseTemp + 10 * season_factor)
                    + (Amplitude * daily_factor)
                    + current_daily_offset_
                    + noise_dist_(gen_);
        
        return temp;
    }


    std::mt19937 gen_;
    std::uniform_real_distribution<> daily_offset_dist_{-5.0, 5.0};
    std::uniform_real_distribution<> noise_dist_{-0.5, 0.5};
    double current_daily_offset_ = 0;
    int last_simulated_day_ = -1;

    NConfig::TSimulatorConfigPtr Config_;

    std::chrono::system_clock::time_point StartTime;
    double BaseTemp;
    double Amplitude;
    std::chrono::nanoseconds Period;
};

int main(int argc, const char* argv[]) {
    NCommon::GetOpts opts;
    opts.AddOption('h', "help", "Show help message");
    opts.AddOption('c', "config", "Path to config file", true);
    opts.AddOption('p', "port", "Serial port", true);
    opts.AddOption('b', "baud", "Baud rate", true);
    opts.AddOption('m', "multiplier", "Time multiplier", true);

    try {
        opts.Parse(argc, argv);
        
        if (opts.Has('h')) {
            std::cerr << "Usage: " << argv[0] << " [OPTIONS] [PORT] [BAUD] [MULTIPLIER]\n"
                      << opts.Help() 
                      << "\nExample with config:\n  " << argv[0] << " -c simulator_config.json\n"
                      << "\nExample with CLI args:\n  " << argv[0] << " /dev/ttyS0 115200 60.0\n";
            return 0;
        }

        auto config = NCommon::New<NConfig::TSimulatorConfig>();

        if (opts.Has('c')) {
            config->LoadFromFile(opts.Get("config"));
        } else {
            config->SerialConfig = NCommon::New<NIpc::TSerialConfig>();
        }

        if (opts.Has('p')) config->SerialConfig->SerialPort = opts.Get("p");
        if (opts.Has('b')) config->SerialConfig->BaudRate = std::stoul(opts.Get("b"));
        if (opts.Has('m')) config->TimeMultiplier = std::stod(opts.Get("m"));

        const std::vector<std::string>& args = opts.GetPositional();
        if (!args.empty()) {
            config->SerialConfig->SerialPort = args[0];
            if (args.size() > 1) config->SerialConfig->BaudRate = std::stoul(args[1]);
            if (args.size() > 2) config->TimeMultiplier = std::stod(args[2]);
        }

        ASSERT(!config->SerialConfig->SerialPort.empty(), "Serial port must be specified");
        ASSERT(config->SerialConfig->BaudRate, "Baud rate must be specified");
        
        TSimulator simulator(config);
        simulator.Run();
    } catch (const std::exception& ex) {
        LOG_ERROR("Simulator error: {}", ex.what());
        return 2;
    }
    
    return 0;
}
