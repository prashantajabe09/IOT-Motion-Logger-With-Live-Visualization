#include <ESP8266WiFi.h>

const char* ssid = "TP-Link_5DD0";
const char* password = "76144493";
const char* host = "192.168.1.101";  // PC running Hercules
const int port = 10013;

WiFiClient client;

void setup() {
  Serial.begin(115200);   // UART to STM32
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi...");
  }

  Serial.println("Connected to WiFi");

  if (client.connect(host, port)) {
    Serial.println("Connected to TCP server");
  } else {
    Serial.println("Connection to TCP server failed");
  }
}

void loop() {
  if (client.connected() && Serial.available()) {
    String data = Serial.readStringUntil('\n');  // read STM32 line
    client.println(data);                        // send to Hercules
  }
}
