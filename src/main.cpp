#include "Telemetry.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <mutex>
#include <iomanip>

struct CarState {
    TelemetryPoint current_data;
    bool is_running = true;
    std::mutex mtx;
};

void replay_engine(const std::vector<TelemetryPoint>& data, CarState& state) {
    auto start_time = std::chrono::steady_clock::now();

    for (const auto& pt : data) {
        auto target_time = start_time + std::chrono::milliseconds(static_cast<long long>(pt.time_ms));
        std::this_thread::sleep_until(target_time);

        std::lock_guard<std::mutex> lock(state.mtx); 
        state.current_data = pt;
    }

    std::lock_guard<std::mutex> lock(state.mtx);
    state.is_running = false;
}

int main() {
    DataLoader loader;
    std::string filepath = "data/telemetry_VER_Monza.csv";
    std::vector<TelemetryPoint> telemetryData = loader.loadCSV(filepath);

    if (telemetryData.empty())
        return EXIT_FAILURE;

    CarState shared_state;
    shared_state.current_data = telemetryData[0]; 

    std::thread engine_thread(replay_engine, std::ref(telemetryData), std::ref(shared_state));

    auto display_start_time = std::chrono::steady_clock::now();

    while (true) {
        TelemetryPoint display_data;
        bool is_active;

        {
            std::lock_guard<std::mutex> lock(shared_state.mtx);
            display_data = shared_state.current_data;
            is_active = shared_state.is_running;
        }

        if (!is_active) break;

        auto current_time = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsed_chrono = current_time - display_start_time;

        std::cout << std::fixed << std::setprecision(2);
        std::cout << "\r[Chrono: " << elapsed_chrono.count() << "s | Data: " << display_data.time_ms / 1000.0 << "s] "
                  << "Vitesse: " << display_data.speed << " km/h | "
                  << "RPM: " << display_data.rpm << " | "
                  << "Rapport: " << display_data.nGear << " | "
                  << "Frein: " << (display_data.brake ? "OUI  " : "NON ") 
                  << std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(33));
    }

    engine_thread.join();
    return EXIT_SUCCESS;
}