#include "Telemetry.hpp"
#include <iostream>

#include "Telemetry.hpp"
#include <iostream>
#include <chrono>
#include <thread>

int main() {
    DataLoader loader;
    std::string filepath = "data/telemetry_VER_Monza.csv";

    std::cout << "load data frome:" << filepath << std::endl;

    std::vector<TelemetryPoint> telemetryData = loader.loadCSV(filepath);
    if (telemetryData.empty())
        return EXIT_FAILURE;

    std::cout << "Succes:" << telemetryData.size() << " line loads.\n" << std::endl;

    auto start_time = std::chrono::steady_clock::now();

    for (const auto& pt : telemetryData) {
        auto target_time = start_time + std::chrono::milliseconds(static_cast<long long>(pt.time_ms));

        std::this_thread::sleep_until(target_time);

        std::cout << "\r[Real Time : " << pt.time_ms / 1000.0 << "s] "
                  << "Vitesse: " << pt.speed << " km/h | "
                  << "RPM: " << pt.rpm << " | "
                  << "Rapport: " << pt.nGear << " | "
                  << "Frein: " << (pt.brake ? "OUI  " : "NON ") 
                  << std::flush;
    }
    return EXIT_SUCCESS;
}