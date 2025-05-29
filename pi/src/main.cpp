#include "Scanner.h"
#include "Logger.h"

#include <mosquitto.h>
#include <iostream>
#include <atomic>
#include <thread>
#include <ctime>

static std::atomic<bool> running{ true };

static std::string nowTimestamp()
{
    std::time_t t = std::time(nullptr);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S",
        std::localtime(&t));
    return buf;
}

// MQTT callback
void on_message(struct mosquitto*, void*, const struct mosquitto_message* msg)
{
    std::string topic(msg->topic);
    std::string payload((char*)msg->payload, msg->payloadlen);
    auto comma = payload.find(',');
    std::string ssid = payload.substr(0, comma);
    double rssi = std::stod(payload.substr(comma + 1));
    Measurement m{ nowTimestamp(), topic, ssid, rssi };
    logger.log(m);
}

int main()
{
    mosquitto_lib_init();
    mosquitto* mosq = mosquitto_new("motion_detector", true, nullptr);
    mosquitto_message_callback_set(mosq, on_message);
    if (mosquitto_connect(mosq, "localhost", 1883, 60) != MOSQ_ERR_SUCCESS)
    {
        std::cerr << "Cannot connect to MQTT broker\n";
        return 1;
    }
    mosquitto_subscribe(mosq, nullptr, "motion/esp32/#", 0);

    // ????? ??? loop MQTT
    std::thread mqttThread([&]()
    {
        while (running) mosquitto_loop(mosq, -1, 1);
    });

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

    mosquitto_disconnect(mosq);
    running = false;
    mqttThread.join();
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
    return 0;
}
