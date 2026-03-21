/*
 * BLE scan: list nearby devices (MAC, name, RSSI) to fill MAC_A / MAC_B in leg*_emg.ino.
 * Board: ESP32S3 Dev Module | USB CDC On Boot: Enabled
 */
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>

static BLEScan* pScan = nullptr;

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }
  delay(500);

  BLEDevice::init("MAC_FINDER");
  pScan = BLEDevice::getScan();
  pScan->setActiveScan(true);
  pScan->setInterval(100);
  pScan->setWindow(99);

  Serial.println();
  Serial.println("=== MyoWare / BLE MAC finder ===");
  Serial.println("Turn on shields. Scan runs 15s, then repeats every 5s.");
  Serial.println();
}

void loop() {
  Serial.println("Scanning (15s)...");
  Serial.flush();

  BLEScanResults* r = pScan->start(15, false);
  if (!r) {
    Serial.println("Scan failed.");
    delay(5000);
    return;
  }

  int c = r->getCount();
  Serial.printf("Found %d advertisement(s)\n\n", c);

  for (int i = 0; i < c; i++) {
    BLEAdvertisedDevice d = r->getDevice(i);
    String name = d.haveName() ? String(d.getName().c_str()) : String("(no name)");
    String addr = String(d.getAddress().toString().c_str());
    int rssi = d.getRSSI();

    Serial.printf("  %s | RSSI %d | %s\n", addr.c_str(), rssi, name.c_str());
  }

  pScan->clearResults();
  Serial.println("\n--- Copy MACs into leg1_emg.ino / leg2_emg.ino as MAC_A / MAC_B ---\n");
  delay(5000);
}
