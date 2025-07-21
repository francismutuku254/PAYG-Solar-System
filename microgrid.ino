#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

const char* ssid = "PAYG-SOLAR";
const char* password = "solar1234";

AsyncWebServer server(80);
bool powerStatus = false;
float remainingCredit = 0.0;

#define LED_PIN LED_BUILTIN
#define LOAD_WATTS 100.0
#define ENERGY_PER_SECOND (LOAD_WATTS / 3600.0 / 1000.0)

unsigned long lastCreditUpdate = 0;

String currentToken = "";
float tokenCredit = 0.0;

// Save credit and power status to file
void saveCredit() {
  StaticJsonDocument<128> doc;
  doc["credit"] = remainingCredit;
  doc["power"] = powerStatus;

  File file = LittleFS.open("/credit.json", "w");
  if (file) {
    serializeJson(doc, file);
    file.close();
  } else {
    Serial.println("Failed to save credit");
  }
}

// Load credit and power status from file
void loadCredit() {
  if (!LittleFS.exists("/credit.json")) {
    Serial.println("No saved credit file.");
    return;
  }

  File file = LittleFS.open("/credit.json", "r");
  if (!file) {
    Serial.println("Failed to open credit file");
    return;
  }

  StaticJsonDocument<128> doc;
  DeserializationError error = deserializeJson(doc, file);
  if (!error) {
    remainingCredit = doc["credit"] | 0.0;
    powerStatus = doc["power"] | false;
    Serial.print("Loaded saved credit: ");
    Serial.println(remainingCredit);
    Serial.print("Power status: ");
    Serial.println(powerStatus ? "ON" : "OFF");

    digitalWrite(LED_PIN, powerStatus ? LOW : HIGH);
  } else {
    Serial.println("Failed to parse saved credit");
  }
  file.close();
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  if (!LittleFS.begin()) {
    Serial.println("LittleFS failed!");
    return;
  }

  loadCredit();

  WiFi.softAP(ssid, password);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  server.on("/chart.min.js", HTTP_GET, [](AsyncWebServerRequest *request){
  request->send(LittleFS, "/chart.min.js", "application/javascript");
  });


  // Web pages
  server.on("/index.html", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/index.html", "text/html");
  });

  server.on("/generate.html", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/generate.html", "text/html");
  });

  server.on("/chart.html", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/chart.html", "text/html");
  });

 server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
  request->redirect("/home.html");
});

  server.on("/about.html", HTTP_GET, [](AsyncWebServerRequest *request){
  request->send(LittleFS, "/about.html", "text/html");
  });
    server.on("/home.html", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/home.html", "text/html");
  });
  server.serveStatic("/img", LittleFS, "/img");
  server.on("/image2.png", HTTP_GET, [](AsyncWebServerRequest *request){
  request->send(LittleFS, "/image2.png", "image/png");
});



  // JSON status
  server.on("/status.json", HTTP_GET, [](AsyncWebServerRequest *request){
    String json;
    StaticJsonDocument<128> doc;
    doc["power"] = powerStatus ? "ON" : "OFF";
    doc["credit"] = remainingCredit;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
  });

  // Token verification
  server.on("/verify", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      StaticJsonDocument<128> doc;
      if (deserializeJson(doc, data)) {
        request->send(400, "application/json", "{\"success\":false}");
        return;
      }

      String token = doc["token"];
      Serial.print("Entered token: ");
      Serial.println(token);

      if (token == currentToken) {
        remainingCredit += tokenCredit;
        powerStatus = true;
        digitalWrite(LED_PIN, LOW);
        lastCreditUpdate = millis();
        saveCredit();
        Serial.println("Token accepted. Power ON.");
        request->send(200, "application/json", "{\"success\":true}");
      } else {
        request->send(200, "application/json", "{\"success\":false}");
      }
  });

  // Token generation
  server.on("/generate-token", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      StaticJsonDocument<256> doc;
      if (deserializeJson(doc, data)) {
        request->send(400, "application/json", "{\"success\":false}");
        return;
      }

      String user = doc["user"];
      float amount = doc["amount"];
      if (amount <= 0) {
        request->send(400, "application/json", "{\"success\":false}");
        return;
      }

      currentToken = String(random(10000000, 99999999));
      tokenCredit = amount * 0.015;

      Serial.println("----- TOKEN GENERATED -----");
      Serial.print("User: "); Serial.println(user);
      Serial.print("Amount: KES "); Serial.println(amount);
      Serial.print("Token: "); Serial.println(currentToken);
      Serial.print("Credit (kWh): "); Serial.println(tokenCredit, 3);
      Serial.println("---------------------------");

      String json;
      StaticJsonDocument<128> resp;
      resp["success"] = true;
      resp["token"] = currentToken;
      serializeJson(resp, json);
      request->send(200, "application/json", json);
  });

  server.begin();
}

void loop() {
  if (powerStatus) {
    unsigned long now = millis();
    if (now - lastCreditUpdate >= 1000) {
      remainingCredit -= ENERGY_PER_SECOND;
      lastCreditUpdate = now;

      if (remainingCredit <= 0.0) {
        powerStatus = false;
        remainingCredit = 0.0;
        digitalWrite(LED_PIN, HIGH);
        Serial.println("Credit depleted. Power OFF.");
      }

      saveCredit();
    }
  }
}
