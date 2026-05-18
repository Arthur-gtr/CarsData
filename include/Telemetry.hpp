#pragma once

#include <string>
#include <vector>

struct TelemetryPoint {
    int speed;
    int rpm;
    int nGear;
    int throttle;
    bool brake;
    double time_ms;
};

class DataLoader {
    public:
        DataLoader() = default;
        ~DataLoader() = default;
    
        std::vector<TelemetryPoint> loadCSV(const std::string& filepath);
};