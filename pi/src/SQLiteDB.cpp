#include "SQLiteDB.h"
#include <iostream>

bool SQLiteDB::open(const std::string& filename)
{
    if (sqlite3_open(filename.c_str(), &db_) != SQLITE_OK)
    {
        std::cerr << "Cannot open SQLite DB: " << sqlite3_errmsg(db_) << "\n";
        return false;
    }
    return true;
}

bool SQLiteDB::initSchema()
{
    const char* sql = R"sql(
        CREATE TABLE IF NOT EXISTS measurements (
          id        INTEGER PRIMARY KEY AUTOINCREMENT,
          timestamp TEXT    NOT NULL,
          source    TEXT    NOT NULL,
          ssid      TEXT    NOT NULL,
          rssi      REAL    NOT NULL
        );
    )sql";
    char* err = nullptr;
    int   rc = sqlite3_exec(db_, sql, nullptr, nullptr, &err);
    if (rc != SQLITE_OK)
    {
        std::cerr << "SQLite schema error: " << err << "\n";
        sqlite3_free(err);
        return false;
    }
    return true;
}

bool SQLiteDB::insertMeasurement(const std::string& timestamp,
    const std::string& source,
    const std::string& ssid,
    double rssi)
{
    const char* sql =
        "INSERT INTO measurements(timestamp, source, ssid, rssi) "
        "VALUES (?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        std::cerr << "SQLite prepare failed: " << sqlite3_errmsg(db_) << "\n";
        return false;
    }
    sqlite3_bind_text(stmt, 1, timestamp.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, source.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, ssid.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 4, rssi);

    std::cout << "Inserting into DB: "
              << timestamp << ", "
              << source << ", "
              << ssid << ", "
              << rssi << " dBm\n";
    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        std::cerr << "SQLite insert failed: " << sqlite3_errmsg(db_) << "\n";
        sqlite3_finalize(stmt);
        return false;
    }
    sqlite3_finalize(stmt);
    return true;
}
