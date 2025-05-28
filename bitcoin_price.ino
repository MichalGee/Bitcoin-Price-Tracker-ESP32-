#include <Wire.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <U8g2lib.h>

// ─── WIFI SETTINGS ────────────────────────────────────────────────────────────
const char* ssid     = "Vectra-WiFi-206C0A";
const char* password = "7xkxxql0xwr01jmz";
IPAddress primaryDNS(8,8,8,8), secondaryDNS(1,1,1,1);

// ─── OLED (U8g2) SETTINGS ─────────────────────────────────────────────────────
// Full-buffer SSD1306 128×64 via HW I2C on SDA=21, SCL=22:
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// ─── API SETTINGS ─────────────────────────────────────────────────────────────
const char* host = "api.coingecko.com";
const char* url  = "/api/v3/simple/price?ids=bitcoin&vs_currencies=usd";

void setup() {
  Serial.begin(115200);
  delay(100);

  // 1) init I2C pins and OLED
  Wire.begin(21, 22);
  u8g2.begin();

  // quick init splash
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(0, 12, "INIT OK");
  u8g2.sendBuffer();
  delay(1500);

  // 2) Wi-Fi + DNS override
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, primaryDNS, secondaryDNS);
  Serial.printf("Connecting to Wi-Fi '%s' ", ssid);
  WiFi.begin(ssid, password);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    Serial.print('.');
    delay(500);
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\n❌ Wi-Fi failed!");
    for (;;) delay(1000);
  }
  Serial.println("\n✅ Wi-Fi connected: " + WiFi.localIP().toString());

  // DNS check
  IPAddress resolved;
  Serial.printf("Resolving %s… ", host);
  if (WiFi.hostByName(host, resolved)) {
    Serial.printf("OK → %s\n", resolved.toString().c_str());
  } else {
    Serial.println("❌ DNS failed!");
  }
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi lost, restarting...");
    ESP.restart();
  }

  // HTTPS GET
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient https;
  https.setTimeout(10000);
  https.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  String fullUrl = String("https://") + host + url;
  Serial.printf("\nRequesting: %s\n", fullUrl.c_str());
  if (!https.begin(client, fullUrl)) {
    Serial.println("Unable to start HTTPS");
    delay(5000);
    return;
  }

  int code = https.GET();
  Serial.printf("HTTP code: %d\n", code);
  if (code == 200) {
    String payload = https.getString();
    Serial.println("Payload: " + payload);

    // parse {"bitcoin":{"usd":12345}}
    StaticJsonDocument<128> doc;
    auto err = deserializeJson(doc, payload);
    if (err) {
      Serial.println("JSON parse error: " + String(err.c_str()));
    } else {
      float price = doc["bitcoin"]["usd"];
      Serial.printf("Parsed price: $%.2f\n", price);

      // display via U8g2
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_ncenB08_tr);
      u8g2.drawStr(10, 12, "Cena bitcoina (USD):");

      char buf[16]; z  
      snprintf(buf, sizeof(buf), "$%.2f", price);
      u8g2.setFont(u8g2_font_ncenB14_tr);
      u8g2.drawStr(10, 48, buf);

      u8g2.sendBuffer();
    }
  } else {
    Serial.println("GET failed: " + https.errorToString(code));
  }
  https.end();

  delay(10000);
}