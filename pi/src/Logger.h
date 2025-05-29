#ifndef LOGGER_H
#define LOGGER_H

#include <mutex>
#include <fstream>
#include <string>
#include <iostream>
#include "Scanner.h"

class FileLogger
{
    std::mutex mu;
    std::ofstream ofs;
public:
    explicit FileLogger(const std::string& fname)
    {
        ofs.open(fname, std::ios::out);
        ofs << "timestamp,source,ssid,rssi\n";
    }
    void log(const Measurement& m)
    {
        std::lock_guard<std::mutex> lk(mu);
        ofs << m.timeStamp << ","
            << m.source << ","
            << m.ssid << ","
            << m.rssi << "\n";
        ofs.flush();
        std::cout << m.timeStamp
            << " | " << m.source
            << " | " << m.ssid
            << " | " << m.rssi << " dBm\n";
    }
};

extern FileLogger logger;

#endif
