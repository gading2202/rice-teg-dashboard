#include "soc/rtc_cntl_reg.h"
#include "soc/soc.h"
#include <Adafruit_INA219.h>
#include <ArduinoJson.h>
#include <DallasTemperature.h>
#include <OneWire.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <Wire.h>
#include <max6675.h>

// =================================================================
// 1. KONFIGURASI WIFI & MQTT BROKER
// =================================================================
const char *ssid = "POCO X6 Pro 5G";
const char *password = "imam2222";

const char *mqtt_server = "broker.hivemq.com";
const int mqtt_port =
    1883; // MQTT biasa untuk ESP32 (8000 itu WebSocket untuk browser)

// Ganti GANTI_UNIK dengan teks acak buatanmu, dan samakan di kolom Topik di
// HTML
const char *mqtt_topic = "rice_teg/GANTI_UNIK/telemetry";

WiFiClient espClient;
PubSubClient client(espClient);

// =================================================================
// 2. PIN & OBJEK SENSOR
// =================================================================
// INA219 (I2C: SDA=21, SCL=22)
Adafruit_INA219 ina219(0x40);
bool ina219Available = false;

// MAX6675 (suhu sisi panas)
#define MAX6675_SCK 18
#define MAX6675_CS 5 // Jika ESP32 freeze saat boot, ubah CS ke GPIO 4
#define MAX6675_SO 19
MAX6675 sensorPanas(MAX6675_SCK, MAX6675_CS, MAX6675_SO);

// DS18B20 (suhu sisi dingin), butuh resistor 4.7k antara Data dan 3.3V
#define DS18B20_PIN 14
OneWire oneWire(DS18B20_PIN);
DallasTemperature sensorDingin(&oneWire);

// =================================================================
// 3. INTERVAL PENGIRIMAN
// =================================================================
unsigned long lastMsg = 0;
const unsigned long INTERVAL_MS = 2500;

// Pembulatan 2 desimal
double r2(double v) { return round(v * 100.0) / 100.0; }

// =================================================================
// FUNGSI KONEKSI WIFI & MQTT
// =================================================================
void setupWiFi() {
  delay(100);
  Serial.println();
  Serial.println("------------------------------------");
  Serial.print("Menghubungkan ke WiFi: ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int counter = 0;
  while (WiFi.status() != WL_CONNECTED && counter < 25) {
    delay(500);
    Serial.print(".");
    counter++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.println("[OK] WiFi Terhubung!");
    Serial.print("IP Address ESP32: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println();
    Serial.println("[PERINGATAN] WiFi belum terhubung. Dicoba lagi di loop...");
  }
  Serial.println("------------------------------------");
}

void reconnectMQTT() {
  if (WiFi.status() != WL_CONNECTED)
    return;

  static unsigned long lastReconnectAttempt = 0;
  unsigned long now = millis();

  if (!client.connected() && (now - lastReconnectAttempt > 4000)) {
    lastReconnectAttempt = now;
    Serial.print("Menghubungkan ke MQTT Broker (" + String(mqtt_server) +
                 ")...");

    String clientId = "ESP32-RiceTEG-" + String(random(0xffff), HEX);

    if (client.connect(clientId.c_str())) {
      Serial.println(" [OK] Terhubung ke MQTT!");
    } else {
      Serial.print(" Gagal, rc=");
      Serial.println(client.state());
    }
  }
}

// =================================================================
// SETUP
// =================================================================
void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // Brownout detector dimatikan

  Serial.begin(115200);
  delay(1500);

  Serial.println();
  Serial.println("====================================");
  Serial.println("    MONITORING RICE-TEG + MQTT      ");
  Serial.println("====================================");

  // I2C
  Wire.begin(21, 22);
  Wire.setTimeOut(1000);

  // INA219 (tidak menghentikan program jika tidak terdeteksi)
  if (!ina219.begin()) {
    Serial.println("[PERINGATAN] INA219 tidak terdeteksi pada 0x40!");
    Serial.println("-> Cek kabel SDA (21), SCL (22), VCC, dan GND.");
    Serial.println(
        "-> Program tetap berjalan (tegangan, arus, daya tidak dikirim).");
    ina219Available = false;
  } else {
    ina219.setCalibration_16V_400mA();
    ina219Available = true;
    Serial.println("[OK] INA219 Terdeteksi & Dikalibrasi (16V 400mA)");
  }

  // Sensor suhu
  sensorDingin.begin();
  Serial.println("[OK] MAX6675 diinisialisasi (SCK:18, CS:5, SO:19)");
  Serial.println("[OK] DS18B20 diinisialisasi (Pin 14)");

  // WiFi & MQTT
  setupWiFi();
  client.setServer(mqtt_server, mqtt_port);
  client.setBufferSize(512); // buffer lebih besar agar JSON tidak terpotong

  Serial.println("====================================");
  Serial.println("Inisialisasi selesai. Memulai pembacaan sensor...");
  Serial.println("====================================");
}

// =================================================================
// LOOP
// =================================================================
void loop() {
  // Reconnect WiFi jika terputus
  if (WiFi.status() != WL_CONNECTED) {
    static unsigned long lastWifiCheck = 0;
    if (millis() - lastWifiCheck > 10000) {
      lastWifiCheck = millis();
      WiFi.reconnect();
    }
  }

  // Reconnect MQTT jika terputus
  if (!client.connected()) {
    reconnectMQTT();
  } else {
    client.loop();
  }

  unsigned long now = millis();
  if (now - lastMsg >= INTERVAL_MS) {
    lastMsg = now;

    // -------------------------------------------------------------
    // 1. BACA SENSOR SUHU
    // -------------------------------------------------------------
    float suhuPanas = sensorPanas.readCelsius();

    sensorDingin.requestTemperatures();
    float suhuDingin = sensorDingin.getTempCByIndex(0);

    bool panasValid = !isnan(suhuPanas);
    bool dinginValid =
        (suhuDingin != DEVICE_DISCONNECTED_C) && !isnan(suhuDingin);

    float deltaT = 0;
    if (panasValid && dinginValid)
      deltaT = suhuPanas - suhuDingin;

    // -------------------------------------------------------------
    // 2. BACA INA219 (TEGANGAN, ARUS, DAYA)
    // -------------------------------------------------------------
    float teganganTEG = 0.0;
    float arus_mA = 0.0;
    float daya_mW = 0.0;

    if (ina219Available) {
      float teganganBus = ina219.getBusVoltage_V();
      float teganganShunt = ina219.getShuntVoltage_mV();
      teganganTEG = teganganBus + (teganganShunt / 1000.0);
      arus_mA =
          ina219.getCurrent_mA(); // negatif = polaritas VIN+/VIN- terbalik
      daya_mW = teganganTEG * arus_mA;
    }

    // -------------------------------------------------------------
    // 3. TAMPILKAN KE SERIAL MONITOR
    // -------------------------------------------------------------
    Serial.println();
    Serial.println("========== DATA RICE-TEG ==========");
    if (panasValid)
      Serial.printf("Suhu Panas (Thot)  : %.2f C\n", suhuPanas);
    else
      Serial.println("Suhu Panas (Thot)  : ERROR (Cek kabel MAX6675)");

    if (dinginValid)
      Serial.printf("Suhu Dingin (Tcold): %.2f C\n", suhuDingin);
    else
      Serial.println(
          "Suhu Dingin (Tcold): ERROR (Cek kabel DS18B20/Resistor 4.7k)");

    if (panasValid && dinginValid)
      Serial.printf("Delta T            : %.2f C\n", deltaT);
    else
      Serial.println("Delta T            : N/A");

    Serial.println("-----------------------------------");
    if (ina219Available) {
      Serial.printf("Tegangan TEG       : %.3f V\n", teganganTEG);
      Serial.printf("Arus TEG           : %.3f mA\n", arus_mA);
      Serial.printf("Daya TEG           : %.3f mW\n", daya_mW);
    } else {
      Serial.println("INA219 tidak terdeteksi, data listrik dilewati");
    }

// -------------------------------------------------------------
// 4. SUSUN JSON (nilai tidak valid dikirim sebagai null)
// -------------------------------------------------------------
#if ARDUINOJSON_VERSION_MAJOR >= 7
    JsonDocument doc;
#else
    StaticJsonDocument<384> doc;
#endif

    if (panasValid)
      doc["thot"] = r2(suhuPanas);
    else
      doc["thot"] = nullptr;
    if (dinginValid)
      doc["tcold"] = r2(suhuDingin);
    else
      doc["tcold"] = nullptr;
    if (panasValid && dinginValid)
      doc["delta_t"] = r2(deltaT);
    else
      doc["delta_t"] = nullptr;

    if (ina219Available) {
      doc["voltage"] = round(teganganTEG * 1000.0) / 1000.0;
      doc["current"] = round(arus_mA * 1000.0) / 1000.0;
      doc["current_a"] = arus_mA / 1000.0;
      doc["power"] = round(daya_mW * 1000.0) / 1000.0;
      doc["power_w"] = daya_mW / 1000.0;
    } else {
      doc["voltage"] = nullptr;
      doc["current"] = nullptr;
      doc["current_a"] = nullptr;
      doc["power"] = nullptr;
      doc["power_w"] = nullptr;
    }

    char buffer[384];
    serializeJson(doc, buffer, sizeof(buffer));

    if (client.connected()) {
      bool ok = client.publish(mqtt_topic, buffer);
      if (ok) {
        Serial.print("[MQTT Berhasil Kirim] -> ");
        Serial.println(buffer);
      } else {
        Serial.println("[MQTT ERROR] Gagal publish ke broker.");
      }
    } else {
      Serial.println(
          "[INFO] Data belum dikirim (MQTT belum terhubung/WiFi offline).");
    }
    Serial.println("===================================");
  }
}
