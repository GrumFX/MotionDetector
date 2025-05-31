#include "Logger.h"

// Declare the global SQLiteDB instance (defined in main.cpp)
extern SQLiteDB db;

// Define the global FileLogger instance (writes to "log.csv")
FileLogger logger("log.csv");
