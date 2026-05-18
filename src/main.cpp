#include "Telemetry.hpp"
#include <iostream>

int main() {
    DataLoader loader;
    std::string filepath = "data/telemetry_VER_Monza.csv";

    std::cout << "load data frome:" << filepath << std::endl;

    const std::vector<TelemetryPoint> telemetryData = loader.loadCSV(filepath);

    if (telemetryData.empty()) {
        std::cerr << "Telemetry is empty" << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "Succes:" << telemetryData.size() << " line loads.\n" << std::endl;

    for (size_t i = 0; i < telemetryData.size(); i++) {
        const auto& pt = telemetryData[i];
        std::cout << "[Temps: " << pt.time_ms << " ms] "
                  << "Vitesse: " << pt.speed << " km/h | "
                  << "RPM: " << pt.rpm << " | "
                  << "Rapport: " << pt.nGear << " | "
                  << "Frein: " << (pt.brake ? "OUI" : "NON") << std::endl;
    }

    return 0;
}