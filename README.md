# MotionDetector

A cyber-physical motion detection system that fuses Wi-Fi RSSI measurements from an ESP32 and a Raspberry Pi. The ESP32 publishes its RSSI readings over MQTT, while the Pi both scans its own RSSI and subscribes to the ESP32 feed—logging everything to a CSV.

---

## 📂 Repository Structure

```
MotionDetector/
├─ CMakeLists.txt
├─ pi/
│   ├─ CMakeLists.txt
│   └─ src/
│       ├─ Scanner.h
│       ├─ Scanner.cpp
│       └─ main.cpp
└─ esp32/
    └─ MotionPublisher/
        └─ MotionPublisher.ino
```

---

## 🛠️ 1. Raspberry Pi Setup & Build

### 1.1 Install Dependencies

```bash
sudo apt update
sudo apt install build-essential cmake mosquitto mosquitto-clients libmosquitto-dev
```

### 1.2 Configure Mosquitto for Remote & Anonymous Access

Create a drop-in file:

```bash
sudo tee /etc/mosquitto/conf.d/10-listen-all.conf > /dev/null <<EOF
listener 1883
allow_anonymous true
EOF
```

Reload and start the broker:

```bash
sudo systemctl daemon-reload
sudo systemctl enable mosquitto
sudo systemctl restart mosquitto
sudo systemctl status mosquitto
```

You should see:

```
● mosquitto.service - Mosquitto MQTT Broker
   Loaded: loaded (/lib/systemd/system/mosquitto.service; enabled)
   Active: active (running) since …
```

### 1.3 Build the C++ Application

```bash
cd MotionDetector/pi
mkdir -p build && cd build
cmake ..
make
```

### 1.4 Run the Motion Detector

```bash
sudo ./motion_detector
```

This will:

* Create or append to `motion_all.csv` with columns:

  ```
  timestamp,source,ssid,rssi
  ```
* Print each measurement, for example:

  ```
  2025-05-11 21:00:00 | pi             | SSID      | -32 dBm
  2025-05-11 21:00:00 | motion/esp32/1 | SSID      | -45 dBm
  ```

---

## 📡 2. ESP32 Deployment

1. Open **`esp32/MotionPublisher/MotionPublisher.ino`** in the Arduino IDE (or PlatformIO).

2. Install the **PubSubClient** library via **Sketch → Include Library → Manage Libraries → PubSubClient**.

3. Edit the top of the sketch to match your network:

   ```cpp
   // Your home Wi-Fi
   const char* ssidAP     = "YourHomeSSID";
   const char* passwordAP = "YourHomePassword";
   // Raspberry Pi’s LAN IP (from `hostname -I`)
   const char* mqttServer = "192.168.x.y";
   ```

4. Select your ESP32 board & port under **Tools → Board**, then click **Upload**.

5. Open the Serial Monitor at **115200 baud** to see:

   ```
   Connecting to Wi-Fi "YourHomeSSID" … IP=192.168.x.z
   Connecting to MQTT broker…
   MQTT connected
   SSID | RSSI=-45 dBm | Pub OK
   ...
   ```

The ESP32 will publish every 500 ms to topic `motion/esp32/1` with payload `"SSID,RSSI"`.




---

