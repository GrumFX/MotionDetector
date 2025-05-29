// main.cpp
#include "Scanner.h"
#include "Logger.h"
#include "SQLiteDB.h"
#include <mosquitto.h>
#include <iostream>
#include <atomic>
#include <thread>
#include <ctime>

static SQLiteDB db;
static std::atomic<bool> running{ true };

static std::string nowTimestamp()
{
    std::time_t t = std::time(nullptr);
    char buf[64];
    std::strftime(buf, sizeof(buf),
        "%Y-%m-%d %H:%M:%S",
        std::localtime(&t));
    return buf;
}

// Callback for incoming MQTT messages
void on_message(struct mosquitto*, void*, const struct mosquitto_message* msg)
{
    std::string topic(msg->topic);
    std::string payload((char*)msg->payload, msg->payloadlen);
    auto comma = payload.find(',');
    Measurement m{
        nowTimestamp(),
        topic,
        payload.substr(0, comma),
        std::stod(payload.substr(comma + 1))
    };

    // 1) Log to CSV file and console
    logger.log(m);

    // 2) Insert into SQLite database
    if (!db.insertMeasurement(m.timeStamp, m.source, m.ssid, m.rssi))
    {
        std::cerr << "DB insert error (MQTT)\n";
    }
}

int main()
{
    // 0) Open SQLite database and initialize schema
    if (!db.open("motion_detector.db"))
    {
        std::cerr << "Cannot open SQLite DB\n";
        return 1;
    }
    if (!db.initSchema())
    {
        std::cerr << "Cannot initialize DB schema\n";
        return 1;
    }

    // 1) Initialize Mosquitto library and connect
    mosquitto_lib_init();
    mosquitto* mosq = mosquitto_new("motion_detector", true, nullptr);
    mosquitto_message_callback_set(mosq, on_message);
    if (mosquitto_connect(mosq, "localhost", 1883, 60) != MOSQ_ERR_SUCCESS)
    {
        std::cerr << "Cannot connect to MQTT broker\n";
        return 1;
    }
    mosquitto_subscribe(mosq, nullptr, "motion/esp32/#", 0);

    // 2) Start the MQTT network loop in a separate thread
    std::thread mqttThread([&]()
    {
        while (running)
        {
            mosquitto_loop(mosq, -1, 1);
        }
    });

    // 3) Main Wi-Fi scan loop
    Scanner scanner;
    while (running)
    {
        try
        {
            for (auto& m : scanner.scan())
            {
                // Log to CSV and console
                logger.log(m);
                // Also insert into the database
                if (!db.SaveSignal(m.timeStamp, m.source, m.ssid, m.rssi))
                {
                    std::cerr << "DB insert error (scan)\n";
                }
            }
        }
        catch (const std::exception& e)
        {
            std::cerr << "Scan error: " << e.what() << "\n";
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // 4) Clean up and exit
    mosquitto_disconnect(mosq);
    running = false;
    mqttThread.join();
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
    return 0;
}
