#include <WiFi.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <DHT.h>
#include "RTClib.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ThingSpeak.h>
#include <time.h> 

// ================= Pinout Definitions =================
#define DHTPIN 4
#define DHTTYPE DHT11
#define SOIL_MOISTURE_PIN 32
#define LDR_PIN 33
#define SD_CS_PIN 5
#define I2C_SDA 21
#define I2C_SCL 22

// ================= Network & ThingSpeak Config =================
const char* ssid = "Jesus is Lord";
const char* password = "ROBOTICS";
const unsigned long myChannelNumber = 3531488;
const char* myWriteAPIKey = "7309V64LKMEO2ZMF";
const char* mdnsHost = "esp32logger";

// NTP Server Settings (UTC+1 for Nigeria)
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600; 
const int daylightOffset_sec = 0;

// ================= Threshold Configurations =================
const float TEMP_HIGH_THRESHOLD = 35.0;
const int SOIL_LOW_THRESHOLD = 30;

// ================= Global Objects & State =================
DHT dht(DHTPIN, DHTTYPE);
RTC_DS3231 rtc;
Adafruit_SSD1306 display(128, 64, &Wire, -1);
WiFiClient client;
WebServer server(80);

// Decoupled Timers
unsigned long lastSensorUpdate = 0;
const unsigned long sensorInterval = 1000; // 1 second for Fast UI/Serial Update

unsigned long lastCloudUpdate = 0;
const unsigned long cloudInterval = 15000; // 15 seconds for Cloud/SD Logging

unsigned long lastWifiRetry = 0;
const unsigned long wifiRetryInterval = 10000; 

bool sdAvailable = false;
bool rtcSyncedWithNTP = false;

// Global Sensor Variables
float currentTemp = 0.0;
float currentHum = 0.0;
int currentSoil = 0;
int currentLight = 0;
String currentTimestamp = "N/A";
String lastAlertMsg = "System Nominal";

// ================= Web Server Handlers =================
void handleRoot() {
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>ESP32 IoT Node Dashboard</title>";
  html += "<style>body{font-family:Arial,sans-serif;background:#121212;color:#e0e0e0;padding:20px;margin:0;}";
  html += ".card{background:#1e1e1e;border-radius:10px;padding:20px;margin-bottom:20px;box-shadow:0 4px 6px rgba(0,0,0,0.3);}";
  html += ".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:15px;}";
  html += ".metric{font-size:24px;font-weight:bold;color:#4CAF50;}";
  html += ".alert{color:#ff5252;}";
  html += "input[type='date'],button{padding:10px;border-radius:5px;border:none;margin-top:5px;font-size:16px;}";
  html += "button{background:#00bcd4;color:#fff;cursor:pointer;font-weight:bold;}";
  html += "button:hover{background:#008ba3;}</style></head><body>";
  
  html += "<h2>ESP32 Edge Data Logger</h2>";
  html += "<div class='card'>";
  html += "<p><strong>Timestamp:</strong> <span id='timestamp'>" + currentTimestamp + "</span></p>";
  html += "<p><strong>Wi-Fi Status:</strong> <span id='wifiStat'>" + String(WiFi.status() == WL_CONNECTED ? "<span style='color:#4CAF50;'>Connected</span>" : "<span class='alert'>Disconnected</span>") + "</span></p>";
  html += "<p><strong>Alert Status:</strong> <span class='alert' id='alertStat'>" + lastAlertMsg + "</span></p>";
  html += "</div>";

  html += "<div class='grid'>";
  html += "<div class='card'>Temperature<div class='metric' id='tempVal'>" + String(currentTemp, 1) + " &deg;C</div></div>";
  html += "<div class='card'>Humidity<div class='metric' id='humVal'>" + String(currentHum, 1) + " %</div></div>";
  html += "<div class='card'>Soil Moisture<div class='metric' id='soilVal'>" + String(currentSoil) + " %</div></div>";
  html += "<div class='card'>Light Intensity<div class='metric' id='lightVal'>" + String(currentLight) + " %</div></div>";
  html += "</div>";

  html += "<div class='card'>";
  html += "<h3>Download SD Card Backup</h3>";
  html += "<form method='GET' action='/download'>";
  html += "<input type='date' id='date' name='date' required> ";
  html += "<button type='submit'>Download CSV</button>";
  html += "</form></div>";
  
  // AJAX script to update values without reloading the page
  html += "<script>";
  html += "setInterval(function() {";
  html += "  fetch('/data').then(response => response.json()).then(data => {";
  html += "    document.getElementById('timestamp').innerText = data.timestamp;";
  html += "    document.getElementById('tempVal').innerHTML = data.temp + ' &deg;C';";
  html += "    document.getElementById('humVal').innerText = data.hum + ' %';";
  html += "    document.getElementById('soilVal').innerText = data.soil + ' %';";
  html += "    document.getElementById('lightVal').innerText = data.light + ' %';";
  html += "    document.getElementById('alertStat').innerText = data.alert;";
  html += "    let wifiEl = document.getElementById('wifiStat');";
  html += "    if(data.wifi === 'Connected') {";
  html += "      wifiEl.innerHTML = \"<span style='color:#4CAF50;'>Connected</span>\";";
  html += "    } else {";
  html += "      wifiEl.innerHTML = \"<span class='alert'>Disconnected</span>\";";
  html += "    }";
  html += "  });";
  html += "}, 2000);"; // Fetch new data every 2 seconds
  html += "</script></body></html>";

  server.send(200, "text/html", html);
}

// Background JSON data endpoint for AJAX
void handleData() {
  String json = "{";
  json += "\"temp\":\"" + String(currentTemp, 1) + "\",";
  json += "\"hum\":\"" + String(currentHum, 1) + "\",";
  json += "\"soil\":\"" + String(currentSoil) + "\",";
  json += "\"light\":\"" + String(currentLight) + "\",";
  json += "\"timestamp\":\"" + currentTimestamp + "\",";
  json += "\"wifi\":\"" + String(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected") + "\",";
  json += "\"alert\":\"" + lastAlertMsg + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void handleDownload() {
  if (!server.hasArg("date")) {
    server.send(400, "text/plain", "Missing 'date' parameter.");
    return;
  }
  String reqDate = server.arg("date");
  String filePath = "/" + reqDate + ".csv";

  if (!SD.exists(filePath)) {
    server.send(404, "text/plain", "No record found for date: " + reqDate);
    return;
  }

  File downloadFile = SD.open(filePath, FILE_READ);
  if (downloadFile) {
    server.sendHeader("Content-Disposition", "attachment; filename=\"" + reqDate + ".csv\"");
    server.streamFile(downloadFile, "text/csv");
    downloadFile.close();
  }
}

// ================= SD & Network Helpers =================
void appendFile(const char* path, const String& message) {
  if (!sdAvailable) return;
  File file = SD.open(path, FILE_APPEND);
  if (file) {
    file.println(message);
    file.close();
  }
}

void syncNTPTime() {
  if (!rtcSyncedWithNTP && WiFi.status() == WL_CONNECTED) {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 5000)) {
      rtc.adjust(DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec));
      rtcSyncedWithNTP = true;
      Serial.println("[NTP] Internet time acquired. RTC synchronized.");
    }
  }
}

bool processOneBacklogItem() {
  if (!sdAvailable || !SD.exists("/backlog.csv")) return false;

  File backlog = SD.open("/backlog.csv", FILE_READ);
  if (!backlog || backlog.size() == 0) {
    if (backlog) backlog.close();
    SD.remove("/backlog.csv");
    return false;
  }

  String entry = backlog.readStringUntil('\n');
  entry.trim();

  if (entry.length() > 10) {
    int c1 = entry.indexOf(','); int c2 = entry.indexOf(',', c1 + 1);
    int c3 = entry.indexOf(',', c2 + 1); int c4 = entry.indexOf(',', c3 + 1);

    if (c1 > 0 && c2 > 0 && c3 > 0 && c4 > 0) {
      ThingSpeak.setField(1, entry.substring(c1 + 1, c2).toFloat());
      ThingSpeak.setField(2, entry.substring(c2 + 1, c3).toFloat());
      ThingSpeak.setField(3, entry.substring(c3 + 1, c4).toInt());
      ThingSpeak.setField(4, entry.substring(c4 + 1).toInt());
      ThingSpeak.setField(5, 2); 

      int response = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
      if (response != 200) {
        backlog.close();
        return true; 
      }
    }
  }

  File temp = SD.open("/temp.csv", FILE_WRITE);
  while (backlog.available()) {
    temp.print(backlog.readStringUntil('\n') + "\n");
  }
  temp.close();
  backlog.close();
  SD.remove("/backlog.csv");
  SD.rename("/temp.csv", "/backlog.csv");
  
  Serial.println("[SYNC] Uploaded 1 backlog record.");
  return true; 
}

// ================= Setup =================
void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL);

  if (display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    display.clearDisplay(); display.setTextColor(WHITE); display.setTextSize(1);
    display.setCursor(0, 0); display.println("Booting System..."); display.display();
  }

  if (!rtc.begin()) Serial.println("[RTC] Not found!");
  
  if (SD.begin(SD_CS_PIN)) sdAvailable = true;

  dht.begin();
  pinMode(SOIL_MOISTURE_PIN, INPUT);
  pinMode(LDR_PIN, INPUT);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  // Register Web Routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/data", HTTP_GET, handleData);
  server.on("/download", HTTP_GET, handleDownload);
  server.begin();
  
  if (MDNS.begin(mdnsHost)) MDNS.addService("http", "tcp", 80);
  ThingSpeak.begin(client);
}

// ================= Main Loop =================
void loop() {
  server.handleClient(); 

  // 1. Non-Blocking Wi-Fi Reconnection & NTP Sync
  if (WiFi.status() != WL_CONNECTED) {
    rtcSyncedWithNTP = false;
    if (millis() - lastWifiRetry >= wifiRetryInterval) {
      Serial.println("[NETWORK] Attempting to reconnect Wi-Fi...");
      WiFi.disconnect();
      WiFi.reconnect();
      lastWifiRetry = millis();
    }
  } else {
    syncNTPTime();
  }

  // 2. FAST LOOP: Sensor Reading, UI Update, and Serial Logging 
  if (millis() - lastSensorUpdate >= sensorInterval) {
    lastSensorUpdate = millis();

    DateTime now = rtc.now();
    char dateBuf[12], timeBuf[10], isoBuf[25];
    sprintf(dateBuf, "%04d-%02d-%02d", now.year(), now.month(), now.day());
    sprintf(timeBuf, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
    sprintf(isoBuf, "%s %s", dateBuf, timeBuf);
    currentTimestamp = String(isoBuf);

    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t) && !isnan(h)) {
      currentTemp = t;
      currentHum = h;
    }
    
    // --- SOIL MOISTURE CALIBRATION ---
    int rawSoil = analogRead(SOIL_MOISTURE_PIN);
    currentSoil = constrain(map(rawSoil, 0, 1200, 0, 100), 0, 100);
    currentLight = constrain(map(analogRead(LDR_PIN), 0, 4095, 0, 100), 0, 100);

    lastAlertMsg = "Nominal";
    if (currentTemp > TEMP_HIGH_THRESHOLD) lastAlertMsg = "High Temp Alert!";
    else if (currentSoil < SOIL_LOW_THRESHOLD) lastAlertMsg = "Low Moisture Alert!";

    // ---> PRINT TO SERIAL MONITOR <---
    Serial.printf("[LIVE DATA] Time: %s | Temp: %.1fC | Hum: %.1f%% | Soil: %d%% | Light: %d%%\n", 
                  timeBuf, currentTemp, currentHum, currentSoil, currentLight);

    display.clearDisplay();
    display.setCursor(0, 0);
    display.printf("%s\n", timeBuf);
    display.printf("T:%.1fC | H:%.1f%%\n", currentTemp, currentHum);
    display.printf("Soil:%d%% | LDR:%d%%\n", currentSoil, currentLight);
    display.printf("Net: %s\n", WiFi.status() == WL_CONNECTED ? "ONLINE" : "OFFLINE");
    display.printf("%s", lastAlertMsg.c_str());
    display.display();
  }

  // 3. SLOW LOOP: Cloud Logging & Data Archiving
  if (millis() - lastCloudUpdate >= cloudInterval) {
    lastCloudUpdate = millis();
    
    String dailyFileName = "/" + currentTimestamp.substring(0, 10) + ".csv";
    String logEntry = currentTimestamp + "," + String(currentTemp, 2) + "," + 
                      String(currentHum, 2) + "," + String(currentSoil) + "," + String(currentLight);

    if (sdAvailable && !SD.exists(dailyFileName)) {
      appendFile(dailyFileName.c_str(), "Timestamp,Temperature,Humidity,Soil,Light");
    }

    if (WiFi.status() == WL_CONNECTED) {
      if (!processOneBacklogItem()) {
        ThingSpeak.setField(1, currentTemp);
        ThingSpeak.setField(2, currentHum);
        ThingSpeak.setField(3, currentSoil);
        ThingSpeak.setField(4, currentLight);
        ThingSpeak.setField(5, 1); 
        
        int tsCode = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
        if (tsCode == 200) {
          Serial.println("[CLOUD] Live payload sent successfully.");
          appendFile(dailyFileName.c_str(), logEntry); 
        } else {
          Serial.printf("[CLOUD] Error %d. Payload sent to backlog.\n", tsCode);
          appendFile("/backlog.csv", logEntry); 
        }
      } else {
         appendFile(dailyFileName.c_str(), logEntry);
      }
    } else {
      appendFile(dailyFileName.c_str(), logEntry); 
      appendFile("/backlog.csv", logEntry);        
    }
  }
}