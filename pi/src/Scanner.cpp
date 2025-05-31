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
    // Launch the scan command via iw
    auto pipe = std::unique_ptr<FILE, decltype(&pclose)>(
        popen("sudo iw dev wlan0 scan", "r"), pclose);
    if (!pipe)
        throw std::runtime_error("popen() failed");

    std::string currentSSID;
    bool is2_4ghz = false;
    char lineBuf[256];

    while (fgets(lineBuf, sizeof(lineBuf), pipe.get()))
    {
        std::string line(lineBuf);

        // Parse the frequency field
        if (line.find("freq:") != std::string::npos)
        {
            // Example: "    freq: 2412"
            std::size_t pos = line.find("freq:");
            std::string freqStr = line.substr(pos + 5);
            // Trim leading/trailing whitespace
            freqStr.erase(0, freqStr.find_first_not_of(" \t\n\r"));
            freqStr.erase(freqStr.find_last_not_of(" \t\n\r") + 1);

            try
            {
                int freq = std::stoi(freqStr);
                // Check if frequency is within 2.4 GHz band (2400–2500 MHz)
                if (freq >= 2400 && freq < 2500)
                    is2_4ghz = true;
                else
                    is2_4ghz = false;
            }
            catch (...)
            {
                is2_4ghz = false;
            }
        }

        // Parse the SSID field
        if (line.find("SSID:") != std::string::npos)
        {
            // Example: "        SSID: MyNetwork"
            currentSSID = line.substr(line.find("SSID:") + 5);
            // Trim leading/trailing whitespace
            currentSSID.erase(0, currentSSID.find_first_not_of(" \t\n\r"));
            currentSSID.erase(currentSSID.find_last_not_of(" \t\n\r") + 1);
        }

        // Parse the signal only if SSID is set and it’s a 2.4 GHz network
        if (!currentSSID.empty() && is2_4ghz && line.find("signal:") != std::string::npos)
        {
            // Example: "        signal: -45.00 dBm"
            std::size_t pos = line.find("signal:");
            std::string rssiStr = line.substr(pos + 7);
            // Trim leading whitespace
            rssiStr.erase(0, rssiStr.find_first_not_of(" \t\n\r"));

            try
            {
                double rssi = std::stod(rssiStr);
                out.push_back({ nowTimestamp(), "pi", currentSSID, rssi });
            }
            catch (...)
            {
                // If conversion fails, skip this entry
            }

            // Clear SSID and reset the flag so we don’t add duplicates
            currentSSID.clear();
            is2_4ghz = false;
        }
    }

    return out;
}
