# 🌍 ESP32 IoT Edge Data Logging System

A robust, fault-tolerant IoT data acquisition system designed for environmental monitoring. This project captures multi-sensor data, logs it directly to the cloud via ThingSpeak, and maintains a highly reliable local MicroSD card backup. 

Designed with real-world engineering principles, this system features an **Asynchronous Offline Cache**: if the Wi-Fi or internet connection drops, the system seamlessly buffers data locally and automatically synchronizes the backlog to the cloud the moment connectivity is restored.

## ⚙️ System Architecture
The project follows a complete end-to-end engineering data pipeline:
**Sensing** → **Processing** → **Local Storage** → **Communication** → **Cloud Storage** → **Visualization** → **Alert/Decision**

---

## ✨ Key Features
*   **Multi-Sensor Array:** Simultaneously monitors Temperature, Humidity, Soil Moisture, and Light Intensity.
*   **Dual-Layer Data Storage:** Real-time cloud logging (ThingSpeak) paired with daily CSV backups stored on a local SD card.
*   **Fault-Tolerant Offline Mode:** Automatic internet disconnection detection. Data is queued in a `backlog.csv` file and batch-uploaded upon reconnection.
*   **Local Web Dashboard:** Features an embedded HTTP server accessible via mDNS (`http://esp32logger.local`) for live monitoring and downloading historical CSV files directly from the SD card.
*   **Hybrid Timekeeping:** Uses an onboard DS3231 Real-Time Clock (RTC) for offline timestamping, which automatically calibrates itself via internet NTP servers when online.
*   **On-Device Monitoring:** 0.96" OLED display provides instant physical readouts of sensor metrics and network status.
*   **Threshold Alerts:** Built-in logic to trigger alerts for high temperatures or dry soil conditions.

---

## 🛠️ Hardware Requirements (BOM)

### Core Components
*   **ESP32 Development Board** (Standard 30 or 38-pin)
*   **DHT11** Temperature & Humidity Sensor
*   **Capacitive Soil Moisture Sensor** (v1.2 or similar)
*   **LDR (Photoresistor)** Light Sensor Module
*   **MicroSD Card Module** (SPI) + MicroSD Card (Formatted to FAT32, max 32GB)
*   **DS3231 RTC Module** (I2C)
*   **0.96" OLED Display** (SSD1306, I2C)

### Power System
*   **3.7V Li-ion/Li-Po Batteries** (x4 wired in parallel)
*   **TP4056** Charging Module
*   **Inline Toggle Switch**

---

## 🔌 Wiring Schematic

Ensure all components share a common Ground (GND). Wire the modules to the ESP32 as follows:

| Component | Device Pin | ESP32 Pin / Destination |
| :--- | :--- | :--- |
| **Power Input** | TP4056 OUT+ / OUT- | Switch → **ESP32 VIN (5V)** / **GND** |
| **DHT11 Sensor** | Data | **GPIO 4** |
| **Soil Moisture** | Analog Out | **GPIO 32** (ADC1) |
| **LDR Sensor** | Signal | **GPIO 33** (ADC1) |
| **MicroSD Module**| CS / MOSI / MISO / SCK | **GPIO 5** / **23** / **19** / **18** |
| **DS3231 RTC** | SDA / SCL | **GPIO 21** / **GPIO 22** (I2C Bus) |
| **OLED Display** | SDA / SCL | **GPIO 21** / **GPIO 22** (I2C Bus) |

---

## 💻 Software Installation

### 1. Arduino IDE Setup
Install the necessary libraries via the Arduino Library Manager (`Sketch > Include Library > Manage Libraries`):
*   `DHT sensor library` by Adafruit
*   `RTClib` by Adafruit
*   `Adafruit SSD1306` and `Adafruit GFX Library`
*   `ThingSpeak` by MathWorks

### 2. Configuration
Before compiling, open the main sketch and update the network and API credentials at the top of the file:
```cpp
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
const unsigned long myChannelNumber = YOUR_THINGSPEAK_CHANNEL_ID;
const char* myWriteAPIKey = "YOUR_THINGSPEAK_API_KEY";
```

### 3. Soil Moisture Calibration
Capacitive soil sensors must be calibrated to your specific soil and water conditions. 
1. Upload the code and open the **Arduino Serial Monitor** (115200 baud).
2. Note the `Raw Soil ADC:` value printed every 2 seconds.
3. **Dry Calibration:** Wipe the sensor completely dry and hold it in the air. Record this number (e.g., `4095`).
4. **Wet Calibration:** Submerge the sensor in a glass of water up to the safe fill line. Record this number (e.g., `1200`).
5. Update the `map()` function in the `loop()` of your code with your recorded numbers:
   ```cpp
   // currentSoil = constrain(map(rawSoil, DRY_VAL, WET_VAL, 0, 100), 0, 100);
   currentSoil = constrain(map(rawSoil, 4095, 1200, 0, 100), 0, 100);
   ```
6. Re-upload the updated code.

---

## 🚀 How to Use

### 1. Local Web Dashboard
1. Ensure your smartphone or PC is on the same Wi-Fi network as the ESP32.
2. Open a web browser and navigate to: `http://esp32logger.local`
3. View real-time sensor metrics, active network status, and system alerts.

### 2. Downloading SD Card Backups
Data is saved locally in daily batches named by date (e.g., `2026-10-11.csv`).
1. Navigate to the local dashboard.
2. Scroll to **Download SD Card Backup**.
3. Use the calendar picker to select your desired date and click **Download CSV**.

### 3. Cloud Monitoring via ThingSpeak
Log into your ThingSpeak account to view auto-generated historical charts for:
*   **Field 1:** Temperature (°C)
*   **Field 2:** Humidity (%)
*   **Field 3:** Soil Moisture (%)
*   **Field 4:** Light Level (%)
*   **Field 5:** Connection Status (1 = Live, 2 = Synced from Backlog)

---

## ⚠️ Troubleshooting

*   **OLED says "Booting System..." and hangs:** The ESP32 cannot communicate with an I2C or SPI device. Double-check the SDA/SCL wiring for the OLED and RTC, and the CS/MOSI/MISO/SCK wiring for the SD card. Ensure the SD card is firmly inserted and formatted to FAT32.
*   **Sensors reading 0% or `ERR`:** Verify the 3.3V power rails are supplying voltage to the sensors. For the DHT11, ensure the data pin is secured to GPIO 4.
*   **Cannot access `http://esp32logger.local`:** Some older network routers block mDNS traffic. If the URL does not resolve, check the Arduino Serial Monitor when the ESP32 boots up to find its numeric IP address (e.g., `192.168.1.45`), and type that directly into your browser.