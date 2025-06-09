// src/SQLiteDB.h
#pragma once
#include "Measurement.h"
#include <sqlite3.h>
#include <string>
#include <vector>

// new struct for “motion” records
struct Motion
{
    std::string note;
    std::string timeStamp;
    std::string source;
    std::string ssid;
    double      rssi;
};

class SQLiteDB
{
    sqlite3* db_ = nullptr;

public:
    ~SQLiteDB() { if (db_) sqlite3_close(db_); }

    // open (or create) the file
    bool open(const std::string& filename);

    // create both tables if missing
    bool initSchema();

    // === SIGNAL methods ===
    bool saveSignal(const std::string& timestamp,
        const std::string& source,
        const std::string& ssid,
        double              rssi)
    {
        // alias to your old insertMeasurement
        return insertMeasurement(timestamp, source, ssid, rssi);
    }

    // read all measurements whose timestamp is BETWEEN from…to
    bool readSignal(const std::string& from,
        const std::string& to,
        std::vector<Measurement>& out);

    // === MOTION methods ===
    bool saveMotion(const std::string& note,
        const std::string& timestamp,
        const std::string& source,
        const std::string& ssid,
        double             rssi);

    bool readMotion(const std::string& from,
        const std::string& to,
        std::vector<Motion>& out);

private:
    // your existing implementation
    bool insertMeasurement(const std::string& timestamp,
        const std::string& source,
        const std::string& ssid,
        double              rssi);
};
