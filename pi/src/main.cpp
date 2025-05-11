#include "Scanner.h"
#include <mosquitto.h>
#include <iostream>
#include <fstream>
#include <atomic>
#include <thread>
#include <mutex>
#include <ctime>

// Thread-safe CSV + console logger
class FileLogger
{
    std::mutex mu;
    std::ofstream ofs;
public:
    FileLogger(const std::string& fname)
    {
        ofs.open(fname, std::ios::out);
        ofs << "timestamp,source,ssid,rssi\n";
    }
    void log(const Measurement& m)
    {
        std::lock_guard<std::mutex> lk(mu);
        // CSV
        ofs << m.timeStamp << "," << m.source << "," << m.ssid << "," << m.rssi << "\n";
        ofs.flush();
        // Console
        std::cout << m.timeStamp
            << " | " << m.source
            << " | " << m.ssid
            << " | " << m.rssi << " dBm\n";
    }
};

static FileLogger logger("motion_all.csv");
static std::atomic<bool> running{ true };

// Helper for timestamp in MQTT callback
static std::string nowTimestamp()
{
    std::time_t t = std::time(nullptr);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S",
        std::localtime(&t));
    return buf;
}

// MQTT message callback
void on_message(struct mosquitto*, void*, const struct mosquitto_message* msg)
{
    std::string topic(msg->topic);
    std::string payload((char*)msg->payload, msg->payloadlen);
    // Expect payload "SSID,RSSI"
    auto comma = payload.find(',');
    std::string ssid = payload.substr(0, comma);
    double rssi = std::stod(payload.substr(comma + 1));
    Measurement m{ nowTimestamp(), topic, ssid, rssi };
    logger.log(m);
}

int main()
{
    // Initialize Mosquitto
    mosquitto_lib_init();
    mosquitto* mosq = mosquitto_new("motion_detector", true, nullptr);
    mosquitto_message_callback_set(mosq, on_message);
    if (mosquitto_connect(mosq, "localhost", 1883, 60) != MOSQ_ERR_SUCCESS)
    {
        std::cerr << "Cannot connect to MQTT broker\n";
        return 1;
    }
    mosquitto_subscribe(mosq, nullptr, "motion/esp32/#", 0);

    // Start MQTT loop thread
    std::thread mqttThread([&]()
    {
        while (running) mosquitto_loop(mosq, -1, 1);
    });

    // Main scan loop
    Scanner scanner;
    while (running)
    {
        try
        {
            for (auto& m : scanner.scan())
            {
                logger.log(m);
            }
        }
        catch (const std::exception& e)
        {
            std::cerr << "Scan error: " << e.what() << "\n";
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // Cleanup
    mosquitto_disconnect(mosq);
    running = false;
    mqttThread.join();
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
    return 0;
}
