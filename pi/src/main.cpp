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

SQLiteDB db;

static std::atomic<bool> running{ true };

static std::atomic<bool> calibrated{ false };

static std::string nowTimestamp()
{
    std::time_t t = std::time(nullptr);
    char buf[64];
    std::strftime(buf, sizeof(buf),
        "%Y-%m-%d %H:%M:%S",
        std::localtime(&t));
    return buf;
}

/**
 * MQTT callback: Called whenever a new message arrives on the subscribed topic.
 * 1) Parse payload into a Measurement.
 * 2) Add it to MotionDetector (for calibration or online detection).
 * 3) Call FileLogger.log(m) ? writes to CSV, prints to console, saves to measurements table.
 * 4) If already calibrated, immediately check for movement on this ESP measurement,
 *    and if movement is detected, print and save to motions table.
 */
void on_message(struct mosquitto*, void* user_data, const struct mosquitto_message* msg)
{
    // 1) Parse topic and payload
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

    // 3) Log to CSV, console, and measurements table
    logger.log(m);

    // 4) If calibration is already done, immediately check for movement:
    if (calibrated.load())
    {
        auto detector = static_cast<MotionDetector*>(user_data);
        if (detector->isMovement(m))
        {
            // Print to stdout that an ESP-based movement was detected
            std::cout << nowTimestamp()
                      << " Movement! (ESP) Source: " << m.source
                      << " RSSI=" << m.rssi << "\n";

            // Save this movement event into the motions table
            if (!db.saveMotion("movement detected (ESP)", m.timeStamp, m.source, m.rssi))
            {
                std::cerr << "DB insert error (motion for ESP)\n";
            }
        }
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

    // 1) Initialize Mosquitto library and create a client.
    //    We pass &detector as user_data so on_message can call detector->addSample(...) & isMovement...
    mosquitto_lib_init();
    MotionDetector detector(30 /* calibration seconds */, 10.0 /* RSSI threshold */);

    mosquitto* mosq = mosquitto_new(
        "motion_detector",   // client ID
        true,                // clean session
        &detector            // user_data ? pointer to our MotionDetector
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

    /**
     * 2) Start a background thread to continuously call mosquitto_loop(-1,1),
     *    so incoming MQTT messages are handled in the background (ESP ? on_message).
     *
     * Note:
     *   - mosquitto_loop(-1,1) blocks until at least one network packet is processed.
     */
    std::thread mqttThread([&]()
    {
        while (running.load())
        {
            mosquitto_loop(mosq, -1, 1);
        }
    });

    Scanner scanner;

    // ---- 3) CALIBRATION: Collect samples from both Scanner (Raspberry) and MQTT (ESP) for 30 seconds ----
    std::cout << "Calibrating for " << detector.getDuration()
        << " seconds (collecting samples from Scanner + MQTT)..." << std::endl;

    auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start
        < std::chrono::seconds(detector.getDuration()))
    {
        // 3.1) Perform one Wi-Fi scan (Raspberry), add each measurement to the detector
        auto batch = scanner.scan();
        for (auto& m : batch)
        {
            detector.addSample(m);
            logger.log(m);
        }
    }

    // 4) After 30 seconds, compute per-source average RSSI and print them
    detector.computeAverages();
    std::cout << "Calibration complete. Average RSSI per source:" << std::endl;
    for (auto& [src, avg] : detector.getAverages())
    {
        std::cout << "  Source = " << src
            << "   AvgRSSI = " << avg << std::endl;
    }
    std::cout << std::string(40, '-') << std::endl;

    // 5) Mark calibration as done so on_message() will start checking ESP samples for movement
    calibrated.store(true);

    // ---- 6) MAIN LOOP: Wi-Fi scanning (Raspberry) + movement detection for Raspberry ----
    while (running.load())
    {
        try
        {
            // 6.1) Perform a Wi-Fi scan, log each measurement, and check for movement
            auto batch = scanner.scan();
            for (auto& m : batch)
            {
                // Log to CSV, console, and measurements table
                logger.log(m);

                // Check for movement on Raspberry measurements only
                if (detector.isMovement(m))
                {
                    std::cout << nowTimestamp()
                              << " Movement! Source: " << m.source
                              << " RSSI = " << m.rssi << std::endl;

                    // Save this Raspberry?based movement event
                    if (!db.saveMotion("Movement detected", m.timeStamp, m.source, m.rssi))
                    {
                        std::cerr << "DB insert error (motion for Raspberry)" << std::endl;
                    }
                }
            }

            /**
             * 6.2) Any incoming ESP messages will be handled asynchronously by on_message()
             *      in the background mqttThread. Since on_message() now checks isMovement(m)
             *      once calibrated == true, you do not need to do anything else here
             *      for ESP-based movement. If you *wanted* to “force” an immediate poll
             *      of pending network I/O, you could call mosquitto_loop(mosq,0,1) here,
             *      but mqttThread is already running mosquitto_loop(-1), so it’s optional.
             */
        }
        catch (const std::exception& e)
        {
            std::cerr << "Scan error: " << e.what() << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // ---- 7) CLEANUP AND EXIT ----
    mosquitto_disconnect(mosq);
    running.store(false);
    mqttThread.join();
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
    return 0;
}
