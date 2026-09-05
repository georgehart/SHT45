#include <Arduino.h>
#include <Wire.h>
#include "Adafruit_SHT4x.h"
#include <WiFiS3.h>
#include <cmath>
#include "secrets.h" // Bevat WIFI_SSID en WIFI_PASS

const char *ssid = WIFI_SSID;
const char *pass = WIFI_PASS;

WiFiServer server(80);
Adafruit_SHT4x sht4 = Adafruit_SHT4x();

// 1. Dauwpunt berekening (Magnus-formule)
double calculateDewPoint(double temp, double humidity)
{
  double a = 17.62;
  double b = 243.12;
  double alpha = log(humidity / 100.0) + (a * temp) / (b + temp);
  return (b * alpha) / (a - alpha);
}

// 2. Absolute luchtvochtigheid berekening (in g/m^3)
double calculateAbsoluteHumidity(double temp, double humidity)
{
  double absHum = (6.112 * exp((17.67 * temp) / (temp + 243.5)) * humidity * 2.1674) / (273.15 + temp);
  return absHum;
}

void setup()
{
  Serial.begin(115200);
  while (!Serial)
    delay(10);

  // Start I2C op de Qwiic poort
  Wire1.begin();
  if (!sht4.begin(&Wire1))
  {
    Serial.println("Kon SHT45 niet vinden!");
    while (1)
      delay(10);
  }
  sht4.setPrecision(SHT4X_HIGH_PRECISION);
  sht4.setHeater(SHT4X_NO_HEATER);

  // --- VAST IP-ADRES INSTELLEN (Telenet netwerk) ---
  IPAddress ip(192, 168, 0, 200);     // Vast IP-adres voor je Arduino
  IPAddress gateway(192, 168, 0, 1);  // Je Telenet modem IP-adres
  IPAddress subnet(255, 255, 255, 0); // Subnetmasker
  WiFi.config(ip, gateway, subnet);

  // Verbinden met WiFi
  Serial.print("Verbinden met WiFi (Vast IP): ");
  Serial.println(ssid);

  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.print(".");
  }

  Serial.println("\nWiFi verbonden!");

  // Toon MAC-adres
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

  Serial.print("Open het dashboard in je browser: http://");
  Serial.println(WiFi.localIP());

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
          // Haal vers de waarden op voor de verzoekafhandeling
          sensors_event_t humidity, temp;
          sht4.getEvent(&humidity, &temp);
          float t = temp.temperature;
          float h = humidity.relative_humidity;
          float dp = calculateDewPoint(t, h);
          float ah = calculateAbsoluteHumidity(t, h);

          if (requestString.indexOf("GET /data") >= 0)
          {
            // --- API JSON ENDPOINT ---
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
            client.print(",\"absolutedelta\":");
            client.print(ah, 2);
            client.println("}");
          }
          else
          {
            // --- HTML / CSS / JS DASHBOARD ---
            client.println("HTTP/1.1 200 OK");
            client.println("Content-Type: text/html");
            client.println("Connection: close");
            client.println();

            client.println("<!DOCTYPE html>");
            client.println("<html><head><meta charset='UTF-8'>");
            client.println("<title>SHT45 Klimaat Monitor</title>");
            client.println("<style>");
            client.println("body { font-family: Arial, sans-serif; background: #f4f7f6; text-align: center; padding: 30px; }");
            client.println(".card { background: white; max-width: 450px; margin: 0 auto; padding: 25px; border-radius: 12px; box-shadow: 0 4px 15px rgba(0,0,0,0.1); }");
            client.println("h1 { color: #333; font-size: 22px; margin-bottom: 20px; }");
            client.println(".metric { margin-bottom: 20px; border-bottom: 1px solid #eee; padding-bottom: 10px; }");
            client.println(".metric:last-child { border-bottom: none; }");
            client.println(".value { font-size: 28px; font-weight: bold; color: #0078D7; margin-top: 5px; }");
            client.println(".label { color: #666; font-size: 13px; text-transform: uppercase; }");
            client.println("</style>");
            client.println("<script>");
            client.println("function fetchData() {");
            client.println("  fetch('/data').then(response => response.json()).then(data => {");
            client.println("    document.getElementById('temp').innerText = data.temperature.toFixed(2) + ' °C';");
            client.println("    document.getElementById('hum').innerText = data.humidity.toFixed(2) + ' %';");
            client.println("    document.getElementById('dew').innerText = data.dewpoint.toFixed(2) + ' °C';");
            client.println("    document.getElementById('abs').innerText = data.absolutedelta.toFixed(2) + ' g/m\u00b3';");
            client.println("  });");
            client.println("}");
            client.println("setInterval(fetchData, 2000);");
            client.println("window.onload = fetchData;");
            client.println("</script>");
            client.println("</head><body>");
            client.println("<div class='card'>");
            client.println("<h1>Klimaat Dashboard App: 0101</h1>");
            client.println("<div class='metric'><div class='label'>Temperatuur</div><div class='value' id='temp'>--</div></div>");
            client.println("<div class='metric'><div class='label'>Relatieve Vochtigheid</div><div class='value' id='hum'>--</div></div>");
            client.println("<div class='metric'><div class='label'>Dauwpunt</div><div class='value' id='dew'>--</div></div>");
            client.println("<div class='metric'><div class='label'>Absolute Vochtigheid</div><div class='value' id='abs'>--</div></div>");
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