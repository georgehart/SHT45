/* ==================================================================================

                        Project: SHT45 Klimaat Monitor

  Description:
  SHT45 Klimaat Monitor
  Meet temperatuur, luchtvochtigheid en dauwpunt en toont de gegevens op een webpagina.

  author: Georges Hart
  date: august 2026

  version: 01082026

===================================================================================== */

#include <Arduino.h>
#include <Wire.h>
#include "Adafruit_SHT4x.h"
#include <WiFiS3.h>
#include <cmath>
#include "secrets.h" // Haal de gegevens op uit je verborgen bestand

// Gebruik de variabelen uit secrets.h
const char *ssid = WIFI_SSID;
const char *pass = WIFI_PASS;

WiFiServer server(80);
Adafruit_SHT4x sht4 = Adafruit_SHT4x();

// Dauwpunt berekening (Magnus-formule)
double calculateDewPoint(double temp, double humidity)
{
  double a = 17.62;
  double b = 243.12;
  double alpha = log(humidity / 100.0) + (a * temp) / (b + temp);
  return (b * alpha) / (a - alpha);
}

void setup()
{
  Serial.begin(115200);
  while (!Serial)
    delay(10);

  // Start I2C op Qwiic poort
  Wire1.begin();
  if (!sht4.begin(&Wire1))
  {
    Serial.println("Kon SHT45 niet vinden!");
    while (1)
      delay(10);
  }
  sht4.setPrecision(SHT4X_HIGH_PRECISION);
  sht4.setHeater(SHT4X_NO_HEATER);

  // Stel hier je vaste gegevens in (pas aan op basis van jouw Telenet netwerk, meestal 192.168.0.x)
  IPAddress ip(192, 168, 0, 200);     // Het vaste IP-adres dat je wilt gebruiken
  IPAddress gateway(192, 168, 0, 1);  // Het IP-adres van je Telenet modem
  IPAddress subnet(255, 255, 255, 0); // Subnetmasker

  WiFi.config(ip, gateway, subnet);
  // Verbinden met WiFi
  Serial.print("Verbinden met WiFi: ");
  Serial.println(ssid);

  WiFi.begin(ssid, pass);

  // Wacht tot we verbonden zijn ÉN een echt IP-adres hebben ontvangen (geen 0.0.0.0)
  while (WiFi.status() != WL_CONNECTED || WiFi.localIP() == IPAddress(0, 0, 0, 0))
  {
    delay(1000);
    Serial.print(".");
  }

  Serial.println("\nWiFi verbonden!");

  // Haal het MAC-adres op in een byte-array van 6 bytes
  byte mac[6];
  WiFi.macAddress(mac);

  Serial.print("MAC-adres: ");
  for (int i = 0; i < 6; i++)
  {
    if (i > 0)
      Serial.print(":");
    if (mac[i] < 16)
      Serial.print("0");
    Serial.print(mac[i], HEX);
  }
  Serial.println();

  Serial.print("Open dit IP-adres in je browser: http://");
  Serial.println(WiFi.localIP());

  // Start de webserver
  server.begin();
}

void loop()
{
  WiFiClient client = server.available();

  if (client)
  {
    String currentLine = "";
    bool currentLineIsBlank = true;
    String requestString = "";

    while (client.connected())
    {
      if (client.available())
      {
        char c = client.read();
        requestString += c;

        if (c == '\n' && currentLineIsBlank)
        {
          // Bepaal of de browser om de API-data vraagt of om de HTML-pagina
          if (requestString.indexOf("GET /data") >= 0)
          {
            // --- API ENDPOINT VOOR JSON DATA ---
            sensors_event_t humidity, temp;
            sht4.getEvent(&humidity, &temp);
            float t = temp.temperature;
            float h = humidity.relative_humidity;
            float dp = calculateDewPoint(t, h);

            client.println("HTTP/1.1 200 OK");
            client.println("Content-Type: application/json");
            client.println("Connection: close");
            client.println();

            client.print("{\"temperature\":");
            client.print(t, 2);
            client.print(",\"humidity\":");
            client.print(h, 2);
            client.print(",\"dewpoint\":");
            client.print(dp, 2);
            client.println("}");
          }
          else
          {
            // --- HOOFDPAGINA (HTML / CSS / JS) ---
            client.println("HTTP/1.1 200 OK");
            client.println("Content-Type: text/html");
            client.println("Connection: close");
            client.println();

            client.println("<!DOCTYPE html>");
            client.println("<html><head><meta charset='UTF-8'>");
            client.println("<title>SHT45 Klimaat Monitor</title>");
            client.println("<style>");
            client.println("body { font-family: Arial, sans-serif; background: #8bebcb; text-align: center; padding-top: 50px; }");
            client.println(".card { background: white; max-width: 400px; margin: 0 auto; padding: 30px; border-radius: 12px; box-shadow: 0 4px 15px rgba(0,0,0,0.1); }");
            client.println("h1 { color: #333; font-size: 24px; }");
            client.println(".value { font-size: 36px; font-weight: bold; color: #0078D7; margin: 10px 0; }");
            client.println(".label { color: #666; font-size: 14px; text-transform: uppercase; }");
            client.println(".metric { margin-bottom: 25px; }");
            client.println("</style>");
            client.println("<script>");
            client.println("function fetchData() {");
            client.println("  fetch('/data').then(response => response.json()).then(data => {");
            client.println("    document.getElementById('temp').innerText = data.temperature.toFixed(2) + ' °C';");
            client.println("    document.getElementById('hum').innerText = data.humidity.toFixed(2) + ' %';");
            client.println("    document.getElementById('dew').innerText = data.dewpoint.toFixed(2) + ' °C';");
            client.println("  });");
            client.println("}");
            client.println("setInterval(fetchData, 2000);"); // Elke 2 seconden verversen
            client.println("window.onload = fetchData;");
            client.println("</script>");
            client.println("</head><body>");
            client.println("<div class='card'>");
            client.println("<h1>SHT45 Dashboard</h1>");
            client.println("<div class='metric'><div class='label'>Temperatuur</div><div class='value' id='temp'>Laden...</div></div>");
            client.println("<div class='metric'><div class='label'>Luchtvochtigheid</div><div class='value' id='hum'>Laden...</div></div>");
            client.println("<div class='metric'><div class='label'>Dauwpunt</div><div class='value' id='dew'>Laden...</div></div>");
            client.println("</div></body></html>");
          }
          break;
        }

        if (c == '\n')
        {
          currentLineIsBlank = true;
          currentLine = "";
        }
        else if (c != '\r')
        {
          currentLineIsBlank = false;
          currentLine += c;
        }
      }
    }
    delay(1);
    client.stop();
  }
}