#ifndef LOGGER_H
#define LOGGER_H

#include <mutex>
#include <fstream>
#include <string>
#include <iostream>
#include "Scanner.h"

#include "SQLiteDB.h"

extern SQLiteDB db;

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

        // 1) Append a line to the CSV file
        ofs << m.timeStamp << ","
            << m.source << ","
            << m.ssid << ","
            << m.rssi << "\n";
        ofs.flush();

        // 2) Print to console
        std::cout << m.timeStamp
            << " | " << m.source
            << " | " << m.ssid
            << " | " << m.rssi << " dBm\n";

        // 3) Insert into the measurements table (via the global db object).
        //    If the insertion fails, print an error to stderr.
        if (!db.saveSignal(m.timeStamp, m.source, m.ssid, m.rssi))
        {
            std::cerr << "DB insert error (saveSignal): "
                << m.timeStamp << ", "
                << m.source << ", "
                << m.ssid << ", RSSI=" << m.rssi << "\n";
        }
    }
};

extern FileLogger logger;

#endif // LOGGER_H
