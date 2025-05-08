#include <ESP8266WiFi.h>
#include "HTTPSRedirect.h"
#include <time.h>
#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define DHTPIN D4
#define DHTTYPE DHT11
#define RELAYPIN D8
#define BUZZERPIN D7
#define SMOKEPIN D3 // Digital input for smoke sensor

DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);

const char* ssid = "mahesh24";
const char* password = "mahesh24";

const char* host = "script.google.com";
const char* GScriptId = "AKfycbwnH53V0K5yggZ9ygiwqZd0QnhHkZu-9HzepF3LeMKSvZZh41pdGs7JIg0fktvSEXp6Eg";
const int httpsPort = 443;
String url = String("/macros/s/") + GScriptId + "/exec";
HTTPSRedirect* client = nullptr;

WiFiServer server(80);
const String loginUsername = "sri";
const String loginPassword = "123";
bool isLoggedIn = false;

int totalValue = 0;
float currentTemp = 0;
float currentHum = 0;
bool smokeDetected = false;
unsigned long lastSensorUpdate = 0;

void connectToWiFi() {
  Serial.print("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ Connected to WiFi.");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

String getDate() {
  time_t now = time(nullptr);
  struct tm* timeInfo = localtime(&now);
  char buf[15];
  strftime(buf, sizeof(buf), "%Y-%m-%d", timeInfo);
  return String(buf);
}

String getTime() {
  time_t now = time(nullptr);
  struct tm* timeInfo = localtime(&now);
  char buf[10];
  strftime(buf, sizeof(buf), "%H:%M:%S", timeInfo);
  return String(buf);
}

// ✅ Updated function to include smoke status
void sendToGoogleSheets(float temp, float hum, bool smoke) {
  String smokeStatus = smoke ? "Detected" : "Clear";
  String payload = "{\"command\": \"appendRow\", \"sheet_name\": \"SensorData\", \"values\": \"" +
                   getDate() + "," + getTime() + "," + String(temp) + "," + String(hum) + "," + String(totalValue) + "," + smokeStatus + "\"}";

  if (client != nullptr) {
    if (!client->connected()) {
      client->connect(host, httpsPort);
    }
    if (client->POST(url, host, payload)) {
      Serial.println("✅ Sent to Google Sheets.");
    } else {
      Serial.println("❌ Failed to send.");
    }
  }
}

void serveLoginPage(WiFiClient client, bool error = false) {
  String errorMsg = error ? "<p style='color:red;'>Incorrect login</p>" : "";
  String page = R"rawliteral(
<html>
<head>
  <style>
    body {
      background-color: black;
      color: white;
      font-family: Arial, sans-serif;
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: center;
      height: 100vh;
      margin: 0;
    }
    h2 {
      color: white;
      margin-bottom: 20px;
    }
    form {
      background-color: #222;
      padding: 30px;
      border-radius: 10px;
      box-shadow: 0 0 10px #0f0;
      text-align: center;
    }
    input {
      display: block;
      width: 200px;
      margin: 10px auto;
      padding: 10px;
      border: none;
      border-radius: 5px;
    }
    input[type='submit'] {
      background-color: lightgreen;
      color: black;
      font-weight: bold;
      cursor: pointer;
      transition: background 0.3s;
    }
    input[type='submit']:hover {
      background-color: #90ee90;
    }
  </style>
</head>
<body>
  <h2>Login</h2>
  )rawliteral" + errorMsg + R"rawliteral(
  <form method='POST' action='/login'>
    <input name='username' placeholder='Username'>
    <input name='password' type='password' placeholder='Password'>
    <input type='submit' value='Login'>
  </form>
</body>
</html>
)rawliteral";

  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println();
  client.print(page);
}

void serveSensorPage(WiFiClient client) {
  String page = R"rawliteral(
<html>
<head>
  <meta http-equiv="refresh" content="5">
  <style>
    body {
      background-color: black;
      color: white;
      font-family: Arial, sans-serif;
      margin: 0;
      padding: 20px;
      text-align: center;
    }
    h1 {
      color: #00FF00;
      text-transform: uppercase;
      margin-bottom: 30px;
    }
    .sensor-box {
      display: inline-block;
      background-color: #222;
      border: 2px solid #00FF00;
      border-radius: 10px;
      padding: 20px;
      margin: 10px;
      width: 200px;
      font-size: 18px;
    }
    .logout {
      margin-top: 30px;
    }
    .logout a {
      background-color: red;
      padding: 10px 20px;
      color: white;
      text-decoration: none;
      font-weight: bold;
      border-radius: 5px;
    }
  </style>
</head>
<body>
  <h1>Welcome IOT System</h1>

  <div class="sensor-box">
    Temp: )rawliteral" + String(currentTemp) + R"rawliteral( °C
  </div>
  <div class="sensor-box">
    Humidity: )rawliteral" + String(currentHum) + R"rawliteral( %
  </div>
  <div class="sensor-box">
    Total Value: )rawliteral" + String(totalValue) + R"rawliteral(
  </div>
  <div class="sensor-box">
    Relay: )rawliteral" + (currentTemp >= 34 ? "ON" : "OFF") + R"rawliteral(
  </div>
  <div class="sensor-box">
    Buzzer: )rawliteral" + ((totalValue >= 10 || smokeDetected) ? "ON" : "OFF") + R"rawliteral(
  </div>
  <div class="sensor-box">
    Smoke: )rawliteral" + (smokeDetected ? "Detected" : "Clear") + R"rawliteral(
  </div>

  <div class="logout">
    <a href='/logout'>Logout</a>
  </div>
</body>
</html>
)rawliteral";

  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println();
  client.print(page);
}

void handleLogin(WiFiClient client, String request) {
  if (request.indexOf("username=" + loginUsername + "&password=" + loginPassword) >= 0) {
    isLoggedIn = true;
    client.println("HTTP/1.1 303 See Other");
    client.println("Location: /");
    client.println();
  } else {
    isLoggedIn = false;
    serveLoginPage(client, true);
  }
}

void handleLogout(WiFiClient client) {
  isLoggedIn = false;
  client.println("HTTP/1.1 303 See Other");
  client.println("Location: /");
  client.println();
}

void setup() {
  Serial.begin(115200);
  pinMode(RELAYPIN, OUTPUT);
  pinMode(BUZZERPIN, OUTPUT);
  pinMode(SMOKEPIN, INPUT);
  digitalWrite(RELAYPIN, LOW);
  digitalWrite(BUZZERPIN, LOW);

  dht.begin();
  Wire.begin(D2, D1);
  lcd.begin(16, 2);
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("  hi  ");
  lcd.setCursor(0, 1);
  lcd.print("  ESP + LCD  ");
  delay(3000);
  lcd.clear();

  connectToWiFi();

  client = new HTTPSRedirect(httpsPort);
  client->setInsecure();
  client->setPrintResponseBody(true);
  client->setContentTypeHeader("application/json");

  configTime(19800, 0, "pool.ntp.org", "time.nist.gov");
  server.begin();
}

void loop() {
  unsigned long now = millis();

  if (now - lastSensorUpdate >= 10000) {
    lastSensorUpdate = now;
    totalValue++;

    float temp = dht.readTemperature();
    float hum = dht.readHumidity();
    int smokeVal = digitalRead(SMOKEPIN);
    smokeDetected = (smokeVal == LOW);

    if (!isnan(temp) && !isnan(hum)) {
      currentTemp = temp;
      currentHum = hum;

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Temp: ");
      lcd.print(temp, 1);
      lcd.print(" C");

      lcd.setCursor(0, 1);
      lcd.print("Hum : ");
      lcd.print(hum, 1);
      lcd.print(" %");

      digitalWrite(RELAYPIN, temp >= 34 ? HIGH : LOW);
      digitalWrite(BUZZERPIN,totalValue >= 10 ? HIGH : LOW);

      // ✅ Send to Google Sheets including smoke
      sendToGoogleSheets(temp, hum, smokeDetected);
    } else {
      Serial.println("❌ DHT read error");
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("❌ DHT Error");
      lcd.setCursor(0, 1);
      lcd.print("Check Sensor");
    }
  }

  WiFiClient client = server.available();
  if (client) {
    String request = "";
    unsigned long timeout = millis() + 2000;
    while (client.connected() && millis() < timeout) {
      while (client.available()) {
        char c = client.read();
        request += c;
        timeout = millis() + 2000;
      }
    }

    if (request.indexOf("POST /login") >= 0) {
      handleLogin(client, request);
    } else if (request.indexOf("GET /logout") >= 0) {
      handleLogout(client);
    } else if (isLoggedIn) {
      serveSensorPage(client);
    } else {
      serveLoginPage(client);
    }

    client.stop();
  }
}
