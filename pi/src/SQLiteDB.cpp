// src/SQLiteDB.cpp
#include "SQLiteDB.h"
#include <iostream>

bool SQLiteDB::open(const std::string& filename)
{
    if (sqlite3_open(filename.c_str(), &db_) != SQLITE_OK)
    {
        std::cerr << "Cannot open DB: " << sqlite3_errmsg(db_) << "\n";
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
        CREATE TABLE IF NOT EXISTS motions (
          id        INTEGER PRIMARY KEY AUTOINCREMENT,
          note      TEXT    NOT NULL,
          timestamp TEXT    NOT NULL,
          source    TEXT    NOT NULL,
          rssi      REAL    NOT NULL
        );
    )sql";

    char* err = nullptr;
    int   rc = sqlite3_exec(db_, sql, nullptr, nullptr, &err);
    if (rc != SQLITE_OK)
    {
        std::cerr << "Schema init error: " << err << "\n";
        sqlite3_free(err);
        return false;
    }
    return true;
}

bool SQLiteDB::insertMeasurement(const std::string& timestamp,
    const std::string& source,
    const std::string& ssid,
    double              rssi)
{
    const char* sql =
        "INSERT INTO measurements(timestamp, source, ssid, rssi) "
        "VALUES (?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        std::cerr << "Prepare failed: " << sqlite3_errmsg(db_) << "\n";
        return false;
    }
    sqlite3_bind_text(stmt, 1, timestamp.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, source.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, ssid.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 4, rssi);

    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        std::cerr << "Insert failed: " << sqlite3_errmsg(db_) << "\n";
        sqlite3_finalize(stmt);
        return false;
    }
    sqlite3_finalize(stmt);
    return true;
}

bool SQLiteDB::saveMotion(const std::string& note,
    const std::string& timestamp,
    const std::string& source,
    double              rssi)
{
    const char* sql =
        "INSERT INTO motions(note, timestamp, source, rssi) "
        "VALUES (?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        std::cerr << "Prepare motion failed: " << sqlite3_errmsg(db_) << "\n";
        return false;
    }
    sqlite3_bind_text(stmt, 1, note.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, timestamp.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, source.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 4, rssi);

    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        std::cerr << "Motion insert failed: " << sqlite3_errmsg(db_) << "\n";
        sqlite3_finalize(stmt);
        return false;
    }
    sqlite3_finalize(stmt);
    return true;
}

bool SQLiteDB::readSignal(const std::string& from,
    const std::string& to,
    std::vector<Measurement>& out)
{
    const char* sql =
        "SELECT timestamp, source, ssid, rssi "
        "FROM measurements "
        "WHERE timestamp BETWEEN ? AND ? "
        "ORDER BY timestamp;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        std::cerr << "Prepare readSignal: " << sqlite3_errmsg(db_) << "\n";
        return false;
    }
    sqlite3_bind_text(stmt, 1, from.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, to.c_str(), -1, SQLITE_TRANSIENT);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        Measurement m;
        m.timeStamp = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        m.source = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        m.ssid = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        m.rssi = sqlite3_column_double(stmt, 3);
        out.push_back(m);
    }
    sqlite3_finalize(stmt);
    return true;
}

bool SQLiteDB::readMotion(const std::string& from,
    const std::string& to,
    std::vector<Motion>& out)
{
    const char* sql =
        "SELECT note, timestamp, source, rssi "
        "FROM motions "
        "WHERE timestamp BETWEEN ? AND ? "
        "ORDER BY timestamp;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        std::cerr << "Prepare readMotion: " << sqlite3_errmsg(db_) << "\n";
        return false;
    }
    sqlite3_bind_text(stmt, 1, from.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, to.c_str(), -1, SQLITE_TRANSIENT);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        Motion mm;
        mm.note = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        mm.timeStamp = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        mm.source = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        mm.rssi = sqlite3_column_double(stmt, 3);
        out.push_back(mm);
    }
    sqlite3_finalize(stmt);
    return true;
}
