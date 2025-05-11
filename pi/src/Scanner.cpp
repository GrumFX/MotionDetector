#include "Scanner.h"
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <ctime>
#include <sstream>

static std::string nowTimestamp()
{
    std::time_t t = std::time(nullptr);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
    return buf;
}

std::vector<Measurement> Scanner::scan()
{
    std::vector<Measurement> out;
    const std::string cmd = "sudo iw dev wlan0 scan";
    auto pipe = std::unique_ptr<FILE, decltype(&pclose)>(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) throw std::runtime_error("popen() failed");

    char buffer[256];
    bool gotSignal = false;
    Measurement m;
    m.source = "pi";

    while (fgets(buffer, sizeof(buffer), pipe.get()))
    {
        std::string line(buffer);
        if (line.find("signal:") != std::string::npos)
        {
            // ????????? ?????
            auto pos = line.find("signal:");
            std::string tail = line.substr(pos + 7);
            double rssi = std::stod(tail);
            m.rssi = rssi;
            m.timeStamp = nowTimestamp();
            out.push_back(m);
        }
    }
    return out;
}
