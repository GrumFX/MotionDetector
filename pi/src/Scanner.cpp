// Scanner.cpp
#include "Scanner.h"
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <ctime>
#include <limits>

// Helper: return current timestamp in "YYYY-MM-DD HH:MM:SS" format
static std::string nowTimestamp()
{
    std::time_t t = std::time(nullptr);
    char buf[64];
    std::strftime(buf, sizeof(buf),
        "%Y-%m-%d %H:%M:%S",
        std::localtime(&t));
    return buf;
}

std::vector<Measurement> Scanner::scan()
{
    std::vector<Measurement> out;

    // Run "sudo iw dev wlan0 scan" and capture its stdout
    auto pipe = std::unique_ptr<FILE, decltype(&pclose)>(
        popen("sudo iw dev wlan0 scan", "r"), pclose);
    if (!pipe)
    {
        throw std::runtime_error("popen() failed");
    }

    std::string currentSSID;
    double      currentRssi = std::numeric_limits<double>::quiet_NaN();
    bool        haveRssi = false;
    bool        is2_4ghz = false;  // true if current BSS block is in 2.4 GHz

    char lineBuf[256];
    while (fgets(lineBuf, sizeof(lineBuf), pipe.get()))
    {
        std::string line(lineBuf);

        // 1) When we see "freq:" ? new BSS block starts
        if (line.find("freq:") != std::string::npos)
        {
            // If prior block had both SSID & RSSI, we should have already pushed it.
            // Now parse this freq to decide if it’s 2.4 GHz.
            std::size_t pos = line.find("freq:");
            std::string freqStr = line.substr(pos + 5);
            freqStr.erase(0, freqStr.find_first_not_of(" \t\n\r"));
            freqStr.erase(freqStr.find_last_not_of(" \t\n\r") + 1);

            try
            {
                int freq = std::stoi(freqStr);
                is2_4ghz = (freq >= 2400 && freq < 2500);
            }
            catch (...)
            {
                is2_4ghz = false;
            }

            // Reset for the new block:
            currentSSID.clear();
            currentRssi = std::numeric_limits<double>::quiet_NaN();
            haveRssi = false;

            continue;
        }

        // 2) If this block is 2.4 GHz, capture "signal:" lines
        if (is2_4ghz && line.find("signal:") != std::string::npos)
        {
            // Example: "        signal: -45.00 dBm"
            std::size_t pos = line.find("signal:");
            std::string rssiStr = line.substr(pos + 7);
            rssiStr.erase(0, rssiStr.find_first_not_of(" \t\n\r"));
            rssiStr.erase(rssiStr.find_last_not_of(" \t\n\r") + 1);

            try
            {
                currentRssi = std::stod(rssiStr);
                haveRssi = true;
            }
            catch (...)
            {
                haveRssi = false;
            }

            // If we already know SSID from earlier in this block, push now:
            if (!currentSSID.empty() && haveRssi)
            {
                out.push_back({ nowTimestamp(), "pi", currentSSID, currentRssi });
                // Clear so we only emit once per block
                currentSSID.clear();
                haveRssi = false;
            }
            continue;
        }

        // 3) If this block is 2.4 GHz, capture "SSID:" lines
        if (is2_4ghz && line.find("SSID:") != std::string::npos)
        {
            // Example: "        SSID: MyNetwork"
            std::string ssidPart = line.substr(line.find("SSID:") + 5);
            ssidPart.erase(0, ssidPart.find_first_not_of(" \t\n\r"));
            ssidPart.erase(ssidPart.find_last_not_of(" \t\n\r") + 1);

            // Skip empty SSID
            if (!ssidPart.empty())
            {
                currentSSID = std::move(ssidPart);
            }

            // If we already have RSSI, push now:
            if (!currentSSID.empty() && haveRssi)
            {
                out.push_back({ nowTimestamp(), "pi", currentSSID, currentRssi });
                // Clear so we only emit once per block
                currentSSID.clear();
                haveRssi = false;
            }
            continue;
        }
    }

    return out;
}
