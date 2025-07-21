#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

const char* ssid = "PAYG-SOLAR";
const char* password = "solar1234";

AsyncWebServer server(80);

#define CLIENT1_RELAY D4
#define CLIENT2_RELAY D2
#define LOAD_WATTS 100.0
#define ENERGY_PER_SECOND (LOAD_WATTS / 3600.0 / 1000.0)

struct PowerUser {
  String phone;
  float credit;
  bool power;
  uint8_t relayPin;
};

PowerUser clients[2] = {
  {"0711111111", 0.0, false, CLIENT1_RELAY},
  {"0722222222", 0.0, false, CLIENT2_RELAY}
};

unsigned long lastUpdate = 0;
String currentToken = "";
float tokenCredit = 0.0;

// ========== FILE UTILS ==========
void saveCreditToFile(PowerUser& user) {
  StaticJsonDocument<128> doc;
  doc["credit"] = user.credit;
  doc["power"] = user.power;
  String path = "/credit_" + user.phone + ".json";
  File file = LittleFS.open(path, "w");
  if (file) {
    serializeJson(doc, file);
    file.close();
  }
}

void loadCreditFromFile(PowerUser& user) {
  String path = "/credit_" + user.phone + ".json";
  if (!LittleFS.exists(path)) return;

  File file = LittleFS.open(path, "r");
  if (!file) return;

  StaticJsonDocument<128> doc;
  if (!deserializeJson(doc, file)) {
    user.credit = doc["credit"] | 0.0;
    user.power = doc["power"] | false;
  }
  file.close();
  digitalWrite(user.relayPin, user.power ? LOW : HIGH);
}

// ========== HARDWARE SETUP ==========
void setupClients() {
  for (int i = 0; i < 2; i++) {
    pinMode(clients[i].relayPin, OUTPUT);
    loadCreditFromFile(clients[i]);
    digitalWrite(clients[i].relayPin, clients[i].power ? LOW : HIGH);
  }
}

// ========== SETUP ==========
void setup() {
  Serial.begin(115200);
  if (!LittleFS.begin()) {
    Serial.println("LittleFS mount failed!");
    return;
  }

  setupClients();

  WiFi.softAP(ssid, password);
  Serial.println("AP IP: " + WiFi.softAPIP().toString());

  // Static files and routes
  server.serveStatic("/", LittleFS, "/").setDefaultFile("login.html");
  server.serveStatic("/img", LittleFS, "/img");

  server.on("/login.html", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/login.html", "text/html");
  });
  server.on("/index.html", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/index.html", "text/html");
  });
  server.on("/generate.html", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/generate.html", "text/html");
  });
  server.on("/chart.html", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/chart.html", "text/html");
  });
  server.on("/about.html", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/about.html", "text/html");
  });
  server.on("/home.html", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/home.html", "text/html");
  });

  server.on("/chart.min.js", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/chart.min.js", "application/javascript");
  });

  server.on("/image2.png", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/image2.png", "image/png");
  });

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->redirect("/login.html");
  });

  // ====== JSON APIs ======
  server.on("/status.json", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!request->hasParam("phone")) {
      request->send(400, "application/json", "{\"error\":\"Missing phone\"}");
      return;
    }
    String phone = request->getParam("phone")->value();
    for (int i = 0; i < 2; i++) {
      if (clients[i].phone == phone) {
        StaticJsonDocument<128> doc;
        doc["credit"] = clients[i].credit;
        doc["power"] = clients[i].power ? "ON" : "OFF";
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
        return;
      }
    }
    request->send(404, "application/json", "{\"error\":\"Client not found\"}");
  });

  // ========== TOKEN GENERATION ==========
server.on("/generate-token", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
[](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, data);
  if (error) {
    Serial.println("Failed to parse /generate-token JSON");
    request->send(400, "application/json", "{\"success\":false}");
    return;
  }

  String phone = doc["phone"];
  float amount = doc["amount"];

  Serial.println("=== Token Request ===");
  Serial.println("Phone: " + phone);
  Serial.println("Amount: " + String(amount));

  if (amount <= 0) {
    Serial.println("Invalid amount received");
    request->send(400, "application/json", "{\"success\":false}");
    return;
  }

  currentToken = String(random(10000000, 99999999)); // 8-digit token
  tokenCredit = amount * 0.015;

  for (int i = 0; i < 2; i++) {
    if (clients[i].phone == phone) {
      Serial.println("Client found, index: " + String(i));
      Serial.println("Generated Token: " + currentToken);
      StaticJsonDocument<128> resp;
      resp["success"] = true;
      resp["token"] = currentToken;
      String json;
      serializeJson(resp, json);
      request->send(200, "application/json", json);
      return;
    }
  }

  Serial.println("Client not registered with phone: " + phone);
  request->send(404, "application/json", "{\"success\":false}");
});


  // ========== VERIFY TOKEN ==========
  server.on("/verify", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      StaticJsonDocument<256> doc;
      if (deserializeJson(doc, data)) {
        request->send(400, "application/json", "{\"success\":false}");
        return;
      }

      String phone = doc["phone"];
      String token = doc["token"];

      for (int i = 0; i < 2; i++) {
        if (clients[i].phone == phone) {
          if (token == currentToken) {
            clients[i].credit += tokenCredit;
            clients[i].power = true;
            digitalWrite(clients[i].relayPin, LOW);
            saveCreditToFile(clients[i]);
            request->send(200, "application/json", "{\"success\":true}");
          } else {
            request->send(200, "application/json", "{\"success\":false}");
          }
          return;
        }
      }

      request->send(404, "application/json", "{\"success\":false}");
  });

  server.begin();
}

// ========== LOOP ==========
void loop() {
  unsigned long now = millis();
  if (now - lastUpdate >= 1000) {
    lastUpdate = now;
    for (int i = 0; i < 2; i++) {
      if (clients[i].power) {
        clients[i].credit -= ENERGY_PER_SECOND;
        if (clients[i].credit <= 0) {
          clients[i].credit = 0;
          clients[i].power = false;
          digitalWrite(clients[i].relayPin, HIGH);
          Serial.println("Power OFF for: " + clients[i].phone);
        }
        saveCreditToFile(clients[i]);
      }
    }
  }
}
