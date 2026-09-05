/**
 * Title: brief Climate Monitoring & Web Dashboard Server
 *
 *  author  : Georges Hart
 *  date    : 2026
 *  version : 0509206
 *
 *
 *  details :
 *     Arduino sketch for an ESP32/Arduino R4 WiFi-based environmental monitor
 *     that measures temperature, relative humidity, dew point, absolute humidity,
 *     and CO2 concentrations, serving a real-time JSON API and modern HTML dashboard.
 *
 *  hardware Components:
 *   - Arduino Uno WiFi R4 (or compatible board with WiFiS3)
 *   - Adafruit SHT45 (Temperature & Humidity Sensor) via I2C (Wire1 / Qwiic)
 *   - SparkFun SCD30 (CO2 Sensor) via I2C (Wire1 / Qwiic)
 *
 *  dependencies:
 *   - Arduino.h
 *   - Wire.h
 *   - Adafruit_SHT4x.h
 *   - SparkFun_SCD30_Arduino_Library.h
 *   - WiFiS3.h
 *   - secrets.h (Requires WIFI_SSID and WIFI_PASS definitions)
 *
 */

#include <Arduino.h>
#include <Wire.h>
#include "Adafruit_SHT4x.h"
#include <SparkFun_SCD30_Arduino_Library.h>
#include <WiFiS3.h>
#include <cmath>
#include "secrets.h" // Bevat WIFI_SSID en WIFI_PASS

const char *ssid = WIFI_SSID;
const char *pass = WIFI_PASS;

WiFiServer server(80);
Adafruit_SHT4x sht4 = Adafruit_SHT4x();
SCD30 airSensor; // Instantie voor de SCD-30 CO2 sensor

float lastValidCo2 = 400.0;

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

    // Start I2C op de Qwiic poort (Wire1)
    Wire1.begin();

    // Initialiseer SHT45
    if (!sht4.begin(&Wire1))
    {
        Serial.println("Kon SHT45 niet vinden!");
        while (1)
            delay(10);
    }
    sht4.setPrecision(SHT4X_HIGH_PRECISION);
    sht4.setHeater(SHT4X_NO_HEATER);

    // Initialiseer SCD-30
    if (!airSensor.begin(Wire1))
    {
        Serial.println("Kon SCD30 CO2 sensor niet vinden!");
        while (1)
            delay(10);
    }

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

                    // Lees CO2 uit en behoud de laatste geldige waarde om '0' waarden te voorkomen static float lastValidCo2 = 400.0; // Startwaarde op normale buitenlucht
                    if (airSensor.dataAvailable())
                    {
                        float currentCo2 = airSensor.getCO2();
                        if (currentCo2 > 0)
                        {
                            lastValidCo2 = currentCo2;
                        }
                    }
                    float co2 = lastValidCo2;

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
                        client.print(",\"co2\":");
                        client.print(co2, 1);
                        client.println("}");
                    }
                    else
                    {
                        // --- GEMODERNISEERD HTML / CSS / JS DASHBOARD ---
                        client.println("HTTP/1.1 200 OK");
                        client.println("Content-Type: text/html");
                        client.println("Connection: close");
                        client.println();

                        client.println("<!DOCTYPE html>");
                        client.println("<html lang='nl'><head><meta charset='UTF-8'>");
                        client.println("<meta name='viewport' content='width=device-width, initial-scale=1.0'>");
                        client.println("<title>Klimaat Monitor | Dashboard</title>");
                        client.println("<link href='https://fonts.googleapis.com/css2?family=Inter:wght@300;400;600;700&display=swap' rel='stylesheet'>");
                        client.println("<style>");
                        client.println(":root { --bg-color: #0f172a; --card-bg: #1e293b; --text-main: #f8fafc; --text-muted: #94a3b8; --accent: #38bdf8; }");
                        client.println("* { box-sizing: border-box; margin: 0; padding: 0; }");
                        client.println("body { font-family: 'Inter', sans-serif; background-color: var(--bg-color); color: var(--text-main); display: flex; justify-content: center; align-items: center; min-height: 100vh; padding: 20px; }");
                        client.println(".dashboard { width: 100%; max-width: 700px; background: var(--card-bg); padding: 30px; border-radius: 20px; box-shadow: 0 10px 25px -5px rgba(0, 0, 0, 0.3); border: 1px solid rgba(255, 255, 255, 0.05); }");
                        client.println(".header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 25px; border-bottom: 1px solid rgba(255, 255, 255, 0.1); padding-bottom: 15px; }");
                        client.println("h1 { font-size: 20px; font-weight: 600; color: var(--text-main); letter-spacing: 0.5px; }");
                        client.println(".status { display: flex; align-items: center; gap: 8px; font-size: 12px; color: #4ade80; font-weight: 500; }");
                        client.println(".dot { width: 8px; height: 8px; background-color: #4ade80; border-radius: 50%; box-shadow: 0 0 8px #4ade80; animation: pulse 2s infinite; }");
                        client.println("@keyframes pulse { 0% { opacity: 1; } 50% { opacity: 0.4; } 100% { opacity: 1; } }");
                        client.println(".grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(190px, 1fr)); gap: 15px; }");
                        client.println(".card { background: rgba(255, 255, 255, 0.03); padding: 20px; border-radius: 14px; border: 1px solid rgba(255, 255, 255, 0.04); transition: transform 0.2s ease, border-color 0.2s ease; }");
                        client.println(".card:hover { transform: translateY(-3px); border-color: rgba(56, 189, 248, 0.3); }");
                        client.println(".card.wide { grid-column: 1 / -1; display: flex; justify-content: space-between; align-items: center; }");
                        client.println(".label { font-size: 11px; text-transform: uppercase; color: var(--text-muted); font-weight: 600; letter-spacing: 1px; margin-bottom: 8px; }");
                        client.println(".value { font-size: 26px; font-weight: 700; color: var(--accent); }");
                        client.println(".card.wide .value { font-size: 32px; }");
                        client.println("</style>");
                        client.println("<script>");
                        client.println("function fetchData() {");
                        client.println("  fetch('/data').then(response => response.json()).then(data => {");
                        client.println("    document.getElementById('temp').innerText = data.temperature.toFixed(1) + ' °C';");
                        client.println("    document.getElementById('hum').innerText = data.humidity.toFixed(1) + ' %';");
                        client.println("    document.getElementById('dew').innerText = data.dewpoint.toFixed(1) + ' °C';");
                        client.println("    document.getElementById('abs').innerText = data.absolutedelta.toFixed(1) + ' g/m\u00b3';");
                        client.println("    document.getElementById('co2').innerText = data.co2.toFixed(0) + ' ppm';");
                        client.println("  }).catch(err => console.error('Fout bij ophalen data:', err));");
                        client.println("}");
                        client.println("setInterval(fetchData, 2000);");
                        client.println("window.onload = fetchData;");
                        client.println("</script>");
                        client.println("</head><body>");
                        client.println("<div class='dashboard'>");
                        client.println("<div class='header'><h1>Thermoregulatie app:0101 (GH)</h1><div class='status'><div class='dot'></div>Live</div></div>");
                        client.println("<div class='grid'>");
                        client.println("<div class='card wide'><div class='info'><div class='label'>CO2 Concentratie</div><div class='value' id='co2'>--</div></div></div>");
                        client.println("<div class='card'><div class='label'>Temperatuur</div><div class='value' id='temp'>--</div></div>");
                        client.println("<div class='card'><div class='label'>Relatieve Vochtigheid</div><div class='value' id='hum'>--</div></div>");
                        client.println("<div class='card'><div class='label'>Dauwpunt</div><div class='value' id='dew'>--</div></div>");
                        client.println("<div class='card'><div class='label'>Absolute Vochtigheid</div><div class='value' id='abs'>--</div></div>");
                        client.println("</div></div></body></html>");
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