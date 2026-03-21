/*
 * One WROOM for both legs (matches leg1_* / leg2_* wroomMAC[] → same hub).
 * Set four STA MACs from ESP_MAC_FINDER on each Leg S3 board.
 *
 * Serial 115200: "1" = L1 cal, "2" = L1 enc reset, "3" = L2 cal, "4" = L2 enc reset.
 */
#include <WiFi.h>
#include <cstring>
#include <esp_now.h>
#include <esp_wifi.h>

static uint8_t LEG1_IMU_MAC[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static uint8_t LEG1_EMG_MAC[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static uint8_t LEG2_IMU_MAC[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static uint8_t LEG2_EMG_MAC[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

#define SERIAL_THROTTLE_MS 100

typedef struct {
  float knee, raw, enc;
  long encCount;
  float a1x, a1y, a1z, a2x, a2y, a2z;
  uint8_t calDone;
  float calOffset;
  uint8_t encReset;
} ImuPacket;

typedef struct {
  float emg1, emg2;
  uint8_t conn1, conn2;
} EmgPacket;

typedef struct {
  uint8_t cmd;
} CmdPacket;

static ImuPacket lastImu1{}, lastImu2{};
static EmgPacket lastEmg1{}, lastEmg2{};
static volatile bool haveImu1, haveImu2, haveEmg1, haveEmg2;
static unsigned long lastSerial;

static bool macAllZero(const uint8_t* m) {
  for (int i = 0; i < 6; i++) {
    if (m[i] != 0) return false;
  }
  return true;
}

static void tryAddPeer(const char* label, uint8_t* mac) {
  if (macAllZero(mac)) {
    Serial.printf("[WROOM] Set %s MAC in sketch.\n", label);
    return;
  }
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, mac, 6);
  peer.channel = 1;
  peer.encrypt = false;
  esp_err_t e = esp_now_add_peer(&peer);
  if (e == ESP_OK) {
    Serial.printf("[WROOM] Peer OK: %s\n", label);
  } else if (e == ESP_ERR_ESPNOW_EXIST) {
  } else {
    Serial.printf("[WROOM] add_peer %s err %d\n", label, (int)e);
  }
}

static bool macEq(const uint8_t* a, const uint8_t* b) {
  return memcmp(a, b, 6) == 0;
}

static void onRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  const uint8_t* src = info->src_addr;
  if (len == (int)sizeof(ImuPacket)) {
    if (macEq(src, LEG1_IMU_MAC)) {
      memcpy(&lastImu1, data, sizeof(ImuPacket));
      haveImu1 = true;
    } else if (macEq(src, LEG2_IMU_MAC)) {
      memcpy(&lastImu2, data, sizeof(ImuPacket));
      haveImu2 = true;
    }
  } else if (len == (int)sizeof(EmgPacket)) {
    if (macEq(src, LEG1_EMG_MAC)) {
      memcpy(&lastEmg1, data, sizeof(EmgPacket));
      haveEmg1 = true;
    } else if (macEq(src, LEG2_EMG_MAC)) {
      memcpy(&lastEmg2, data, sizeof(EmgPacket));
      haveEmg2 = true;
    }
  }
}

static void sendCmd(const uint8_t* imuMac, uint8_t cmd, const char* tag) {
  if (macAllZero(imuMac)) {
    Serial.printf("[WROOM] No IMU MAC for %s\n", tag);
    return;
  }
  CmdPacket p = {cmd};
  esp_err_t e = esp_now_send(imuMac, (uint8_t*)&p, sizeof(p));
  Serial.printf("[WROOM] %s cmd %u: %s\n", tag, (unsigned)cmd, e == ESP_OK ? "queued" : "fail");
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== WROOM both legs ===");

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) {
    Serial.println("[WROOM] esp_now_init FAIL");
    while (true) {
      delay(1000);
    }
  }
  esp_now_register_recv_cb(onRecv);

  tryAddPeer("LEG1_IMU", LEG1_IMU_MAC);
  tryAddPeer("LEG1_EMG", LEG1_EMG_MAC);
  tryAddPeer("LEG2_IMU", LEG2_IMU_MAC);
  tryAddPeer("LEG2_EMG", LEG2_EMG_MAC);

  Serial.println("[WROOM] Serial: 1=L1 cal 2=L1 enc 3=L2 cal 4=L2 enc");
  lastSerial = millis();
}

void loop() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '1') sendCmd(LEG1_IMU_MAC, 1, "L1");
    if (c == '2') sendCmd(LEG1_IMU_MAC, 2, "L1");
    if (c == '3') sendCmd(LEG2_IMU_MAC, 1, "L2");
    if (c == '4') sendCmd(LEG2_IMU_MAC, 2, "L2");
  }

  unsigned long t = millis();
  if (SERIAL_THROTTLE_MS > 0 && (t - lastSerial) < (unsigned long)SERIAL_THROTTLE_MS) {
    return;
  }
  if (!haveImu1 && !haveImu2 && !haveEmg1 && !haveEmg2) {
    return;
  }

  lastSerial = t;
  if (haveImu1) {
    Serial.printf(
        "L1 IMU knee=%.2f raw=%.2f enc=%.2f ec=%ld cal=%u off=%.2f er=%u\n",
        lastImu1.knee, lastImu1.raw, lastImu1.enc, (long)lastImu1.encCount,
        (unsigned)lastImu1.calDone, lastImu1.calOffset, (unsigned)lastImu1.encReset);
    haveImu1 = false;
  }
  if (haveImu2) {
    Serial.printf(
        "L2 IMU knee=%.2f raw=%.2f enc=%.2f ec=%ld cal=%u off=%.2f er=%u\n",
        lastImu2.knee, lastImu2.raw, lastImu2.enc, (long)lastImu2.encCount,
        (unsigned)lastImu2.calDone, lastImu2.calOffset, (unsigned)lastImu2.encReset);
    haveImu2 = false;
  }
  if (haveEmg1) {
    Serial.printf("L1 EMG e1=%.3f e2=%.3f c1=%u c2=%u\n",
                  lastEmg1.emg1, lastEmg1.emg2, (unsigned)lastEmg1.conn1, (unsigned)lastEmg1.conn2);
    haveEmg1 = false;
  }
  if (haveEmg2) {
    Serial.printf("L2 EMG e1=%.3f e2=%.3f c1=%u c2=%u\n",
                  lastEmg2.emg1, lastEmg2.emg2, (unsigned)lastEmg2.conn1, (unsigned)lastEmg2.conn2);
    haveEmg2 = false;
  }
}
