#include <WiFi.h>
#include <PubSubClient.h>

// 1) Home Wi-Fi credentials
const char* ssidAP     = "Starlink";
const char* passwordAP = "159632487";
// 2) Raspberry Pi IP on LAN
const char* mqttServer = "192.168.1.187";

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
  if (!mqtt.connected())       connectMQTT();
  mqtt.loop();

  long rssi = WiFi.RSSI();
  String ssid = WiFi.SSID();
  // payload = "SSID,RSSI"
  String payload = ssid + "," + String(rssi);
  bool ok = mqtt.publish("motion/esp32/1", payload.c_str());
  Serial.printf("%s | RSSI=%ld dBm | %s\n",
                ssid.c_str(), rssi, ok?"Pub OK":"Pub FAIL");

  delay(500);
}
