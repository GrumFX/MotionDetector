#include "Scanner.h"
#include <mosquitto.h>
#include <iostream>
#include <fstream>
#include <atomic>
#include <thread>
#include <vector>
#include <mutex>

// CSV??????
class FileLogger
{
    std::mutex mu;
    std::ofstream ofs;
public:
    FileLogger(const std::string& fname)
    {
        ofs.open(fname, std::ios::out);
        ofs << "timestamp,source,rssi\n";
    }
    void log(const Measurement& m)
    {
        std::lock_guard<std::mutex> lk(mu);
        ofs << m.timeStamp << "," << m.source << "," << m.rssi << "\n";
        ofs.flush();
    }
};

static FileLogger logger("motion_all.csv");
static std::atomic<bool> running{ true };

// MQTT callback
void on_message(struct mosquitto*, void*, const struct mosquitto_message* msg)
{
    std::string topic(msg->topic);
    std::string payload((char*)msg->payload, msg->payloadlen);
    // payload — RSSI ?????
    double rssi = std::stod(payload);
    Measurement m;
    // timestamp
    {
        std::time_t t = std::time(nullptr);
        char buf[64];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
        m.timeStamp = buf;
    }
    m.source = topic;  // ????????? "motion/esp32/1"
    m.rssi = rssi;
    logger.log(m);
}

int main()
{
    // ????????????? MQTT
    mosquitto_lib_init();
    mosquitto* mosq = mosquitto_new("motion_system", true, nullptr);
    mosquitto_message_callback_set(mosq, on_message);
    if (mosquitto_connect(mosq, "localhost", 1883, 60) != MOSQ_ERR_SUCCESS)
    {
        std::cerr << "Cannot connect to MQTT broker\n";
        return 1;
    }
    mosquitto_subscribe(mosq, nullptr, "motion/esp32/#", 0);
    // ?????? ?????? ????????? loop
    std::thread mqttThread([&]()
    {
        while (running)
        {
            mosquitto_loop(mosq, -1, 1);
        }
    });

    Scanner scanner;
    // ???????? ????: ???? + ???
    while (running)
    {
        try
        {
            auto vec = scanner.scan();
            for (auto& m : vec)
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

    mosquitto_disconnect(mosq);
    running = false;
    mqttThread.join();
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
    return 0;
}
