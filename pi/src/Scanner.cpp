#include "Scanner.h"
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <ctime>

static std::string nowTimestamp()
{
    std::time_t t = std::time(nullptr);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S",
        std::localtime(&t));
    return buf;
}

std::vector<Measurement> Scanner::scan()
{
    std::vector<Measurement> out;
    auto pipe = std::unique_ptr<FILE, decltype(&pclose)>(
        popen("sudo iw dev wlan0 scan", "r"), pclose);
    if (!pipe) throw std::runtime_error("popen() failed");

    std::string currentSSID;
    char lineBuf[256];
    while (fgets(lineBuf, sizeof(lineBuf), pipe.get()))
    {
        std::string line(lineBuf);
        // Parse SSID
        if (line.find("SSID:") != std::string::npos)
        {
            currentSSID = line.substr(line.find("SSID:") + 5);
            currentSSID.erase(0, currentSSID.find_first_not_of(" \t\n\r"));
            currentSSID.erase(currentSSID.find_last_not_of(" \t\n\r") + 1);
        }
        // Parse signal
        if (!currentSSID.empty() && line.find("signal:") != std::string::npos)
        {
            double rssi = std::stod(line.substr(line.find("signal:") + 7));
            out.push_back({ nowTimestamp(), "pi", currentSSID, rssi });
            currentSSID.clear();
        }
    }
    return out;
}
