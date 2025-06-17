#include "config.h"           // user-provided secrets & server endpoint

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>

const int encA = 5;   // GPIO5  (D1)
const int encB = 4;   // GPIO4  (D2)

// Machine modes: payment amount (Tenge) -> number of pulses
struct Mode {
  int amount;   // payment amount in Tenge
  int pulses;   // number of encoder pulses to send
};
Mode washModes[] = {
  {250, 3},
  {500, 7},
  {1000, 15}
  // add new modes here!
};

const int machineId      = 90;       // change per installation
const uint16_t pollDelay = 10000;    // ms between HTTP polls

int failedConnectionAttempts = 0;

void setup() {
  pinMode(encA, OUTPUT);
  pinMode(encB, OUTPUT);
  digitalWrite(encA, LOW);
  digitalWrite(encB, LOW);

  Serial.begin(115200);
  Serial.println();
  Serial.printf("Connecting to %s", ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print('.');
    if (++failedConnectionAttempts >= 30) {
      Serial.println("\nFailed to connect – restarting …");
      ESP.restart();
    }
  }

  Serial.println();
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());
  failedConnectionAttempts = 0; // reset counter
}

void loop() {
  // Build request URL – config.h must define apiPath *with* trailing slash
  String url = String(apiPath) + String(machineId);  // e.g. /api/v1/payments/90

  HTTPClient http;
  http.begin(host, 80, url);
  http.setTimeout(30000);

  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    String response = http.getString();
    Serial.println("\nResponse:");
    Serial.println(response);

    int sum = parseXMLSum(response);
    if (sum > 0) {
      int pulses = 0;
      for (unsigned int i = 0; i < sizeof(washModes) / sizeof(washModes[0]); ++i) {
        if (sum == washModes[i].amount) {
          pulses = washModes[i].pulses;
          break;
        }
      }
      if (pulses > 0) {
        pulseEncoder(pulses);
      } else {
        Serial.println("Unknown sum, no action taken.");
      }
    } else {
      digitalWrite(encA, LOW);
      digitalWrite(encB, LOW);
    }
  } else {
    Serial.printf("\nHTTP GET failed, error: %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();
  delay(pollDelay);
}

int parseXMLSum(const String& xml) {
  const String openTag  = "<sum>";
  const String closeTag = "</sum>";
  int start = xml.indexOf(openTag);
  int end   = xml.indexOf(closeTag, start);
  if (start != -1 && end != -1) {
    String val = xml.substring(start + openTag.length(), end);
    return val.toInt();
  }
  return 0;
}

void pulseEncoder(int times) {
  for (int i = 0; i < times; ++i) {
    digitalWrite(encA, HIGH);
    delay(50);
    digitalWrite(encB, HIGH);
    delay(50);
    digitalWrite(encA, LOW);
    delay(50);
    digitalWrite(encB, LOW);
    delay(200);
  }
}
