# Rice-TEG IoT Telemetry Dashboard & Monitoring System

Sistem monitoring real-time untuk pemanenan energi termoelektrik (**Thermoelectric Generator - TEG**) berbasis **ESP32** dan **MQTT (HiveMQ)**.

## 📌 Fitur Utama
- **Monitoring Suhu Real-Time**:
  - Sisi Panas ($T_{hot}$) menggunakan sensor termokopel **MAX6675**.
  - Sisi Dingin ($T_{cold}$) menggunakan sensor waterproof **DS18B20**.
  - Kalkulasi gradien temperatur $\Delta T = T_{hot} - T_{cold}$.
- **Karakteristik Listrik**:
  - Pengukuran Tegangan ($V$), Arus ($I$), dan Daya Listrik ($P$) menggunakan sensor **INA219** (I2C).
- **Protokol IoT & Cloud**:
  - Komunikasi nirkabel via **MQTT Broker** (`broker.hivemq.com`).
  - Web Dashboard interaktif menggunakan **Chart.js** & **Paho MQTT WebSocket** (Port 8000 WS / 8884 WSS).
  - Perekaman log telemetri dan fitur **Export CSV**.

## 🔌 Konfigurasi Pin ESP32
| Sensor | Fungsi | Pin ESP32 | Keterangan |
| :--- | :--- | :--- | :--- |
| **MAX6675** | Suhu Panas ($T_{hot}$) | SCK: 18, CS: 5, SO: 19 | SPI Thermocouple K |
| **DS18B20** | Suhu Dingin ($T_{cold}$) | GPIO 14 | OneWire (Pull-up $4.7\text{k}\Omega$) |
| **INA219** | Tegangan & Arus | SDA: 21, SCL: 22 | I2C Addr `0x40` (16V 400mA) |

## 🚀 Live Demo Web Dashboard
Akses web dashboard secara langsung via GitHub Pages:
👉 **[https://gading2202.github.io/rice-teg-dashboard/](https://gading2202.github.io/rice-teg-dashboard/)**

