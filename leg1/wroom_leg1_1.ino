/*
 * WROOM hub — LEG 1 | Receives ESP-NOW from leg1_imu + leg1_emg, optional cal commands to IMU.
 * Board: ESP32 Dev Module (or ESP32-S3) | Channel 1 must match leg sketches.
 *
 * 1) Flash this on the WROOM whose STA MAC is 08:3A:F2:52:88:E8 (see leg1_* wroomMAC[]).
 * 2) Run ESP_MAC_FINDER on the Leg 1 IMU S3 and Leg 1 EMG S3; paste bytes below.
 * 3) Serial 115200: send "1" or "c" → cal cmd to IMU; "2" or "r" → encoder reset cmd.
 */
#include <WiFi.h>
#include <cstring>
#include <esp_now.h>
#include <esp_wifi.h>

// Leg 1 sender boards (STA MAC from each ESP32-S3)
static uint8_t LEG1_IMU_MAC[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static uint8_t LEG1_EMG_MAC[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

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

static ImuPacket lastImu{};
static EmgPacket lastEmg{};
static volatile bool haveImu;
static volatile bool haveEmg;
static unsigned long lastSerial;

static bool macAllZero(const uint8_t* m) {
  for (int i = 0; i < 6; i++) {
    if (m[i] != 0) return false;
  }
  return true;
}

static void tryAddPeer(const char* label, uint8_t* mac) {
  if (macAllZero(mac)) {
    Serial.printf("[WROOM L1] Set %s MAC in sketch (ESP_MAC_FINDER on that board).\n", label);
    return;
  }
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, mac, 6);
  peer.channel = 1;
  peer.encrypt = false;
  esp_err_t e = esp_now_add_peer(&peer);
  if (e == ESP_OK) {
    Serial.printf("[WROOM L1] Peer OK: %s\n", label);
  } else if (e == ESP_ERR_ESPNOW_EXIST) {
    /* already added */
  } else {
    Serial.printf("[WROOM L1] add_peer %s err %d\n", label, (int)e);
  }
}

static bool macEq(const uint8_t* a, const uint8_t* b) {
  return memcmp(a, b, 6) == 0;
}

static void onRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  const uint8_t* src = info->src_addr;
  if (len == (int)sizeof(ImuPacket) && macEq(src, LEG1_IMU_MAC)) {
    memcpy(&lastImu, data, sizeof(ImuPacket));
    haveImu = true;
  } else if (len == (int)sizeof(EmgPacket) && macEq(src, LEG1_EMG_MAC)) {
    memcpy(&lastEmg, data, sizeof(EmgPacket));
    haveEmg = true;
  }
}

static void sendCmdToImu(uint8_t cmd) {
  if (macAllZero(LEG1_IMU_MAC)) {
    Serial.println("[WROOM L1] Cannot send cmd — LEG1_IMU_MAC not set.");
    return;
  }
  CmdPacket p = {cmd};
  esp_err_t e = esp_now_send(LEG1_IMU_MAC, (uint8_t*)&p, sizeof(p));
  Serial.printf("[WROOM L1] Cmd %u → IMU: %s\n", (unsigned)cmd, e == ESP_OK ? "queued" : "fail");
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== WROOM LEG1 hub ===");

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) {
    Serial.println("[WROOM L1] esp_now_init FAIL");
    while (true) {
      delay(1000);
    }
  }
  esp_now_register_recv_cb(onRecv);

  tryAddPeer("LEG1_IMU", LEG1_IMU_MAC);
  tryAddPeer("LEG1_EMG", LEG1_EMG_MAC);

  Serial.println("[WROOM L1] Ready. Serial: 1/c = cal, 2/r = enc reset");
  lastSerial = millis();
}

void loop() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '1' || c == 'c' || c == 'C') sendCmdToImu(1);
    if (c == '2' || c == 'r' || c == 'R') sendCmdToImu(2);
  }

  unsigned long t = millis();
  if (SERIAL_THROTTLE_MS > 0 && (t - lastSerial) < (unsigned long)SERIAL_THROTTLE_MS) {
    return;
  }
  if (!haveImu && !haveEmg) {
    return;
  }

  lastSerial = t;
  if (haveImu) {
    Serial.printf(
        "L1 IMU knee=%.2f raw=%.2f enc=%.2f ec=%ld cal=%u off=%.2f er=%u\n",
        lastImu.knee, lastImu.raw, lastImu.enc, (long)lastImu.encCount,
        (unsigned)lastImu.calDone, lastImu.calOffset, (unsigned)lastImu.encReset);
    haveImu = false;
  }
  if (haveEmg) {
    Serial.printf("L1 EMG e1=%.3f e2=%.3f c1=%u c2=%u\n",
                  lastEmg.emg1, lastEmg.emg2, (unsigned)lastEmg.conn1, (unsigned)lastEmg.conn2);
    haveEmg = false;
  }
}
