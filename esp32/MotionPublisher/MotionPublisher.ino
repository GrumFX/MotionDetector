#include <WiFi.h>
#include <PubSubClient.h>

// 1) Home Wi-Fi credentials
const char* ssidAP     = "ASUS AX6000";
const char* passwordAP = "159632487";
// 2) Raspberry Pi IP on LAN
const char* mqttServer = "192.168.50.57";

WiFiClient   espClient;
PubSubClient mqtt(espClient);

void connectWiFi() {
  Serial.printf("Connecting to Wi-Fi \"%s\" …", ssidAP);
  WiFi.begin(ssidAP, passwordAP);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.printf("\nConnected, IP: %s\n", WiFi.localIP().toString().c_str());
}

void connectMQTT() {
  Serial.println("Connecting to MQTT broker…");
  while (!mqtt.connected()) {
    if (mqtt.connect("esp32_1")) {
      Serial.println("MQTT connected");
    } else {
      Serial.printf("MQTT rc=%d; retrying\n", mqtt.state());
      delay(1000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  connectWiFi();
  mqtt.setServer(mqttServer, 1883);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) connectWiFi();
  if (!mqtt.connected())             connectMQTT();
  mqtt.loop();

  Serial.println("Scanning Wi-Fi networks…");
  int n = WiFi.scanNetworks();

  for (int i = 0; i < n; ++i) {
    String ssid = WiFi.SSID(i);
    int rssi = WiFi.RSSI(i);
    String payload = ssid + "," + String(rssi);
    bool ok = mqtt.publish("motion/esp32/scan", payload.c_str());
    Serial.printf("Sent: %s | %s\n", payload.c_str(), ok ? "OK" : "FAIL");
  }

  WiFi.scanDelete(); // Clear memory
}
