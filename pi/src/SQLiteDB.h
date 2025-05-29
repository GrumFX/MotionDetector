#pragma once

#include <sqlite3.h>
#include <string>

class SQLiteDB
{
    sqlite3* db_ = nullptr;
public:
    ~SQLiteDB()
    {
        if (db_) sqlite3_close(db_);
    }

    bool open(const std::string& filename);

    bool initSchema();

    bool SaveSignal(const std::string& timestamp,
        const std::string& source,
        const std::string& ssid,
        double rssi);
};
