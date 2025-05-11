#pragma once
#include <vector>
#include <string>

struct Measurement
{
    std::string timeStamp;  
    std::string source;     // "pi" or "motion/esp32/1"
    std::string ssid;       // the SSID of the scanned network
    double      rssi;       // dBm
};

class Scanner
{
public:
    std::vector<Measurement> scan();
};
