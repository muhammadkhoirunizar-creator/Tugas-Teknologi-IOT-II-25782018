#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

// Kredensial Jaringan Wi-Fi
const char* ssid = "Nizar";
const char* password = "12345678";

// Konfigurasi Broker MQTT
const char* mqttServer = "broker.hivemq.com";
const int mqttPort = 1883;
const char* clientId = "ESP8266_LabIoT_25782018_Tugas";

// Definisi Topik Tugas
const char* topicTelemetri = "kampus/labiot/25782018/telemetri";
const char* topicAlert     = "kampus/labiot/25782018/alert";
const char* topicKendali   = "kampus/labiot/25782018/kendali";

// Konfigurasi Pin Perangkat Keras
const byte dhtPin = 2;   // Pin D4 (GPIO 2)
const byte ledPin = 5;   // Pin D1 (GPIO 5)

#define DHTTYPE DHT22
DHT dht(dhtPin, DHTTYPE);

WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastTelemetryTime = 0;
const unsigned long telemetryInterval = 4000; // Kirim tiap 4 detik

// Fungsi Callback untuk Menerima Perintah Kendali LED
void callback(char* topic, byte* payload, unsigned int length) {
  char incomingBuffer[length + 1];
  memcpy(incomingBuffer, payload, length);
  incomingBuffer[length] = '\0';

  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, incomingBuffer);
  if (err) return;

  if (doc.containsKey("led")) {
    int val = doc["led"];
    digitalWrite(ledPin, val == 1 ? HIGH : LOW);
    Serial.printf("[KONTROL] Status LED diubah ke: %s\n", val == 1 ? "MENYALA" : "PADAM");
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Menghubungkan ke Broker MQTT...");
    if (client.connect(clientId)) {
      Serial.println(" Sukses!");
      client.subscribe(topicKendali);
      Serial.printf("Subskripsi Topik: %s\n", topicKendali);
    } else {
      Serial.printf("Gagal (rc=%d). Coba lagi dalam 4 detik...\n", client.state());
      delay(4000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);
  dht.begin();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi Terkoneksi!");

  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long now = millis();
  if (now - lastTelemetryTime >= telemetryInterval) {
    lastTelemetryTime = now;

    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (!isnan(t) && !isnan(h)) {
      // 1. Logika Klasifikasi Status Suhu Tugas
      String kondisiSuhu = (t > 31.0) ? "PANAS" : "NORMAL";

      // 2. Kirim Paket Telemetri Rutin
      StaticJsonDocument<256> docOut;
      docOut["node_id"] = clientId;
      docOut["suhu"] = serialized(String(t, 2));
      docOut["kelembapan"] = serialized(String(h, 2));
      docOut["status_led"] = digitalRead(ledPin);
      docOut["kondisi_suhu"] = kondisiSuhu;

      char buffer[256];
      serializeJson(docOut, buffer);
      client.publish(topicTelemetri, buffer);
      Serial.printf("[TELEMETRI] %s\n", buffer);

      // 3. Logika Kanal Peringatan Khusus (Alert) jika Suhu > 31.0°C
      if (t > 31.0) {
        StaticJsonDocument<256> docAlert;
        docAlert["status"] = "BAHAYA";
        docAlert["parameter"] = "OVERHEAT";
        docAlert["nilai_suhu"] = serialized(String(t, 2));
        docAlert["pesan"] = "Suhu melebihi batas aman 31.0 C!";

        char alertBuffer[256];
        serializeJson(docAlert, alertBuffer);
        client.publish(topicAlert, alertBuffer);
        Serial.printf("[ALERT BAHAYA] %s\n", alertBuffer);
      }
    }
  }
}