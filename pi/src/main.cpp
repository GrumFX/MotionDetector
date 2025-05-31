// main.cpp
#include "Scanner.h"
#include "Logger.h"
#include "SQLiteDB.h"
#include "MotionDetector.h"
#include <mosquitto.h>
#include <iostream>
#include <atomic>
#include <thread>
#include <chrono>
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

void on_message(struct mosquitto* mosq, void* user_data, const struct mosquitto_message* msg)
{
    // ??????????? payload ? Measurement
    std::string topic(msg->topic);
    std::string payload((char*)msg->payload, msg->payloadlen);
    auto comma = payload.find(',');
    Measurement m{
        nowTimestamp(),
        topic,
        payload.substr(0, comma),
        std::stod(payload.substr(comma + 1))
    };

    if (user_data)
    {
        auto detector = static_cast<MotionDetector*>(user_data);
        detector->addSample(m);
    }

    logger.log(m);

    if (!db.saveSignal(m.timeStamp, m.source, m.ssid, m.rssi))
    {
        std::cerr << "DB insert error (MQTT signal): "
            << m.timeStamp << ", " << m.source
            << ", " << m.ssid << ", RSSI=" << m.rssi << "\n";
    }
}

int main()
{
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

    mosquitto_lib_init();


    MotionDetector detector(30 /*seconds*/, 10.0 /*threshold*/);

    mosquitto* mosq = mosquitto_new(
        "motion_detector",
        true,             
        &detector         
    );
    if (!mosq)
    {
        std::cerr << "Failed to create mosquitto client\n";
        return 1;
    }
    mosquitto_message_callback_set(mosq, on_message);

    if (mosquitto_connect(mosq, "localhost", 1883, 60) != MOSQ_ERR_SUCCESS)
    {
        std::cerr << "Cannot connect to MQTT broker\n";
        return 1;
    }
    mosquitto_subscribe(mosq, nullptr, "motion/esp32/#", 0);

    std::thread mqttThread([&]()
    {
        while (running)
        {
            mosquitto_loop(mosq, -1, 1);
        }
    });

    Scanner scanner;

    std::cout << "Calibrating for " << detector.getDuration()
        << " seconds: collecting samples from Scanner + MQTT...\n";

    auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start
        < std::chrono::seconds(detector.getDuration()))
    {
        for (auto& m : scanner.scan())
        {
            detector.addSample(m);
        }
        mosquitto_loop(mosq, 0, 1);
    }

    detector.computeAverages();
    std::cout << "Calibration complete. Average RSSI per source:\n";
    for (auto& [src, avg] : detector.getAverages())
    {
        std::cout << "  Source = " << src
                  << "   AvgRSSI = " << avg << "\n";
    }
    std::cout << std::string(40, '-') << "\n";

    while (running)
    {
        try
        {
            auto batch = scanner.scan();
            for (auto& m : batch)
            {
                logger.log(m);
                if (!db.saveSignal(m.timeStamp, m.source, m.ssid, m.rssi))
                {
                    std::cerr << "DB insert error (scan)\n";
                }
                if (detector.isMovement(m))
                {
                    std::cout << nowTimestamp()
                        << "  Movement! Source: " << m.source
                        << " RSSI=" << m.rssi << "\n";
                    if (!db.saveMotion("movement detected",
                        m.timeStamp,
                        m.source,
                        m.rssi))
                    {
                        std::cerr << "DB insert error (motion)\n";
                    }
                }
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
