/*
 * Print this ESP32’s Wi‑Fi MAC (STA) for ESP‑NOW peer setup.
 * Board: ESP32S3 Dev Module | USB CDC On Boot: Enabled
 */
#include <WiFi.h>

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }
  delay(500);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  uint8_t mac[6];
  WiFi.macAddress(mac);

  Serial.println();
  Serial.println("=== ESP MAC (use for ESP-NOW) ===");
  Serial.printf("STA MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.println();
  Serial.println("C array (paste into sketch if needed):");
  Serial.printf("uint8_t peerMAC[] = {0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X};\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.println("Done. Reset board to print again.");
}

void loop() {}
