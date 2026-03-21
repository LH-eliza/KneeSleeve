# KneeSleeve

Documentation for the uOBionics knee monitoring firmware (True North Biomedical Competition, University of Ottawa).

---

## 1. Overview

The distributed firmware reads knee-related motion (dual IMUs, rotary encoder) and muscle activity (MyoWare over BLE) from two legs. Leg nodes are ESP32-S3 boards. A central ESP32 WROOM aggregates data over ESP-NOW and exposes it on USB serial at 115200 baud for logging or downstream software.

Data path: leg sensors → leg ESP32-S3 firmware → WROOM hub → host serial.

---

## 2. Architecture

- **Leg node (IMU):** One ESP32-S3 per leg runs `legN_imu.ino`. Samples IMUs and encoder, sends `ImuPacket` to the hub, receives `CmdPacket` for calibration and encoder reset.
- **Leg node (EMG):** One ESP32-S3 per leg runs `legN_emg.ino`. Connects to two MyoWare devices over BLE, sends `EmgPacket` to the hub.
- **Hub:** ESP32 WROOM runs `wroom_all_legs.ino` when a single hub serves both legs. Per-leg hub variants exist for dual-hub layouts.
- **Radio:** ESP-NOW, fixed Wi-Fi channel **1** on leg and hub firmware so links stay aligned.

---

## 3. Firmware modules

| Path | Description |
|------|-------------|
| `leg1/leg1_imu.ino`, `leg2/leg2_imu.ino` | IMU + encoder; ESP-NOW TX/RX with hub |
| `leg1/leg1_emg.ino`, `leg2/leg2_emg.ino` | BLE MyoWare clients; ESP-NOW TX to hub |
| `wroom_all_legs/wroom_all_legs.ino` | Hub for four senders (L1 IMU, L1 EMG, L2 IMU, L2 EMG) |
| `leg1/wroom_leg1_1.ino`, `leg2/wroom_leg2_1.ino` | Hub restricted to one leg’s IMU + EMG MACs |
| `address finder/ESP_MAC_FINDER.ino` | Prints STA MAC for ESP-NOW addressing |
| `address finder/myoware_MAC_FINDER.ino` | BLE scan output for MyoWare MAC discovery |
| `uobionics_final.ino` | Standalone ESP32: soft AP, HTTP + WebSocket UI, local IMUs; not part of the ESP-NOW leg network |

Sketches are organized as Arduino projects (one `.ino` per directory, except `uobionics_final.ino` at repository root).

---

## 4. Build environment

- **Arduino IDE** (or compatible toolchain) with **esp32** board package installed.
- **Leg IMU / EMG:** Board `ESP32S3 Dev Module`, **USB CDC On Boot: Enabled**.
- **WROOM hub:** Standard ESP32 dev module profile matching the physical board.
- **Serial:** 115200 baud for monitor and calibration commands on the hub.

---

## 5. Configuration

### 5.1 Hub address (`wroomMAC[]`)

Leg sketches contain `wroomMAC[]` pointing at the WROOM STA address. Default in source: `08:3A:F2:52:88:E8`. Replace with the hub’s actual MAC if different (use `ESP_MAC_FINDER.ino` on the hub).

### 5.2 Sender addresses (hub sketch)

The hub sketch lists each leg IMU and EMG STA MAC (`LEG1_IMU_MAC`, `LEG1_EMG_MAC`, `LEG2_IMU_MAC`, `LEG2_EMG_MAC`). Populate using `ESP_MAC_FINDER.ino` on each sender board.

### 5.3 MyoWare BLE (`MAC_A`, `MAC_B`)

`leg1_emg.ino` and `leg2_emg.ino` filter advertisers by MAC string. Set from `myoware_MAC_FINDER.ino` or an external BLE scanner.

---

## 6. Setup procedure

1. Program the WROOM with `wroom_all_legs.ino` (or the appropriate `wroom_legN_1.ino` if using separate hubs).
2. Read the hub MAC with `ESP_MAC_FINDER.ino`; update `wroomMAC[]` in all leg IMU and EMG sketches.
3. Read each leg board MAC with `ESP_MAC_FINDER.ino`; update the hub’s `LEG*_IMU_MAC` / `LEG*_EMG_MAC` arrays.
4. Update `MAC_A` / `MAC_B` in each `legN_emg.ino`.
5. Power leg nodes; open serial on the hub at 115200 and verify IMU/EMG lines.

---

## 7. Hub serial interface

### 7.1 `wroom_all_legs.ino`

| Input | Action |
|-------|--------|
| `1` | Send calibrate command to leg 1 IMU |
| `2` | Send encoder reset to leg 1 IMU |
| `3` | Send calibrate command to leg 2 IMU |
| `4` | Send encoder reset to leg 2 IMU |

### 7.2 `wroom_leg1_1.ino` / `wroom_leg2_1.ino`

| Input | Action |
|-------|--------|
| `1` or `c` | Calibrate IMU for that leg |
| `2` or `r` | Encoder reset for that leg |

Hub output is text lines per packet type. `SERIAL_THROTTLE_MS` in the hub source limits print rate.

---

## 8. Protocol reference

- **ImuPacket:** Knee angle, raw angle, encoder angle, encoder count, six accelerometer floats, calibration flags (`calDone`, `calOffset`, `encReset`).
- **EmgPacket:** `emg1`, `emg2`, `conn1`, `conn2`.
- **CmdPacket (to IMU):** `1` = straight-leg calibration sequence; `2` = encoder reset.
- **EMG BLE service UUID:** `ec3af789-2154-49f4-a9fc-bc6c88e9e930`.

---

## 9. Leg IMU hardware (from sketch)

| Function | GPIO / parameter |
|----------|-------------------|
| Encoder DT / CLK | 10, 11 |
| IMU bus 1 SDA / SCL | 39, 35 |
| IMU bus 2 SDA / SCL | 18, 15 |
| I2C device address | `0x6A` (LSM6DSO-class, WHO_AM_I `0x6C`) |
| LED | 2 |

---

## 10. `uobionics_final.ino`

Single-board firmware: Wi-Fi soft AP, web server on port 80, WebSockets on port 81, IMUs on pins 18/19 and 32/33. SSID, password, and pins are defined at the top of the file. Does not participate in the leg ESP-NOW topology.

---

## 11. License and contributions

Specify a license and contribution policy when distributing the repository.
