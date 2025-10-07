#include <ESP8266WiFi.h>

const char* ssid = "TP-Link_5DD0";
const char* password = "76144493";
const char* host = "192.168.1.101";  // Server IP
const int port = 10013;

WiFiClient client;

void connectToServer() {
  while (!client.connect(host, port)) {
    Serial.println("Connecting to TCP server...");
    delay(1000);
  }
  Serial.println("Connected to TCP server");
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  connectToServer();
}

void loop() {
  // if connection lost → reconnect
  if (!client.connected()) {
    Serial.println("Server disconnected, reconnecting...");
    client.stop();
    delay(1000);
    connectToServer();
  }

  // if STM32 sent something
  if (Serial.available()) {
    String data = Serial.readStringUntil('\n');  // careful with blocking
    client.print(data + "\n");                   // send clean newline
    Serial.println("Sent: " + data);
  }

  // if server sends data back, read it
  while (client.available()) {
    String line = client.readStringUntil('\n');
    Serial.println("From server: " + line);
  }
}
