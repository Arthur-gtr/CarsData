#include "Telemetry.hpp"
#include <fstream>
#include <sstream>
#include <iostream>

const std::vector<TelemetryPoint> DataLoader::loadCSV(const std::string& filepath) {
    std::vector<TelemetryPoint> data;
    std::ifstream file(filepath);

    if (!file.is_open()) {
        std::cerr << "Error: failed to open the file:" << filepath << std::endl;
        return data;
    }

    std::string line;
    std::getline(file, line);

    while (std::getline(file, line)) {
        if (line.empty()) continue;

        std::stringstream ss(line);
        std::string token;
        TelemetryPoint point;

        try {
            std::getline(ss, token, ','); point.speed = std::stoi(token);
            std::getline(ss, token, ','); point.rpm = std::stoi(token);
            std::getline(ss, token, ','); point.nGear = std::stoi(token);
            std::getline(ss, token, ','); point.throttle = std::stoi(token);

            std::getline(ss, token, ','); 
            point.brake = (token == "True"); 

            std::getline(ss, token, ','); point.time_ms = std::stod(token);
            data.push_back(point);
        } catch (const std::exception& e) {
            std::cerr << "Error CSV format." << line << std::endl;
        }
    }

    file.close();
    return data;
}