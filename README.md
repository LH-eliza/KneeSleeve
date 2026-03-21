# KneeSleeve

Firmware for a dual-leg knee monitoring setup: each leg uses ESP32-S3 modules for IMU/encoder data and MyoWare EMG over BLE, with an ESP32 WROOM hub collecting streams over ESP-NOW. A separate sketch (`uobionics_final.ino`) implements a self-contained demo on one ESP32 with Wi-Fi access point, HTTP server, WebSockets, and on-board IMUs.

## Repository layout

| Path | Role |
|------|------|
| `leg1/leg1_imu.ino` | Leg 1: dual LSM6DSO (I2C `0x6A`), rotary encoder, ESP-NOW to WROOM, receives calibration / encoder-reset commands |
| `leg1/leg1_emg.ino` | Leg 1: BLE clients to two MyoWare shields, filters samples, ESP-NOW EMG packets to WROOM |
| `leg1/wroom_leg1_1.ino` | Hub firmware scoped to Leg 1 only (use if that leg talks to its own WROOM) |
| `leg2/leg2_imu.ino` | Leg 2: same IMU/encoder/ESP-NOW behavior as Leg 1 |
| `leg2/leg2_emg.ino` | Leg 2: same EMG pipeline as Leg 1 with Leg 2 MyoWare MACs |
| `leg2/wroom_leg2_1.ino` | Hub firmware scoped to Leg 2 only |
| `wroom_all_legs/wroom_all_legs.ino` | **Recommended** if one WROOM receives all four senders (matches shared `wroomMAC[]` in the leg sketches) |
| `address finder/ESP_MAC_FINDER.ino` | Prints STA MAC for ESP-NOW peer setup |
| `address finder/myoware_MAC_FINDER.ino` | BLE scan listing for MyoWare shield addresses |
| `uobionics_final.ino` | Standalone uOBionics UI: soft AP `uOBionics` / password in sketch, web UI on port 80, WebSocket on 81, IMUs on GPIO 18/19 and 32/33 |

There are no `src/`, `web/`, or `data/` trees in this repository; each Arduino sketch lives in its own folder (or at the root for `uobionics_final.ino`).

## Architecture (distributed leg firmware)

- **Per leg, two ESP32-S3 boards** (typical split): one runs `leg*_imu.ino`, one runs `leg*_emg.ino`.
- **One ESP32 WROOM** (or one per leg, if you change `wroomMAC[]` accordingly) runs `wroom_all_legs.ino` or the matching `wroom_leg*_1.ino`.
- **Wi-Fi channel** is fixed to **1** on leg EMG, leg IMU (via `esp_wifi_set_channel`), and WROOM hubs so ESP-NOW stays aligned.
- **ESP-NOW peer**: all leg sketches use the same hub address `08:3A:F2:52:88:E8` in `wroomMAC[]` (adjust if your WROOM’s STA MAC differs).
- **EMG path**: MyoWare BLE devices are selected by MAC strings in `leg1_emg.ino` / `leg2_emg.ino`; service UUID used in code is `ec3af789-2154-49f4-a9fc-bc6c88e9e930`.

Packet shapes are shared between senders and hubs: `ImuPacket` (knee angle, raw angle, encoder, accel vectors, calibration flags) and `EmgPacket` (two channels plus connection flags). The IMU firmware accepts a one-byte `CmdPacket`: `1` runs straight-leg calibration, `2` resets the encoder state.

## Hardware notes (from sketches)

- **Leg IMU / EMG (ESP32-S3)**: board profile **ESP32S3 Dev Module**, **USB CDC On Boot: Enabled**, serial **115200**.
- **Leg IMU**: encoder on GPIO 10/11; IMU buses SDA/SCL 39/35 and 18/15; LSM6DSO WHO_AM_I `0x6C`; LED GPIO 2.
- **WROOM hub**: ESP32 family dev module; STA mode, channel 1, ESP-NOW peer list must include each sender’s STA MAC.

## Configuration checklist

1. Confirm the WROOM STA MAC (e.g. with `ESP_MAC_FINDER.ino` on that board) matches `wroomMAC[]` on every leg sender.
2. On each leg ESP32-S3, run `ESP_MAC_FINDER.ino` and paste those addresses into the hub sketch (`LEG1_IMU_MAC`, `LEG1_EMG_MAC`, etc.).
3. Use `myoware_MAC_FINDER.ino` (or your phone’s BLE tools) to set `MAC_A` / `MAC_B` in the correct `leg*_emg.ino`.
4. If you use **one** hub for both legs, prefer `wroom_all_legs/wroom_all_legs.ino` and configure all four sender MACs.

## WROOM serial commands (`wroom_all_legs.ino`)

At 115200 baud: `1` Leg 1 cal, `2` Leg 1 encoder reset, `3` Leg 2 cal, `4` Leg 2 encoder reset. Per-leg hub sketches use `1`/`c` and `2`/`r` for cal and encoder reset on that leg only.

Hub output is line-oriented text (IMU and EMG fields); `SERIAL_THROTTLE_MS` in the hub sketch limits print rate.

## `uobionics_final.ino`

Independent of the leg/WROOM ESP-NOW network: one ESP32 hosts the full web experience and reads two on-board IMUs. Default AP credentials and pin map are defined at the top of the file. Treat network passwords as sensitive if you deploy hardware.

## License and contributing

Add a license and contribution policy if you publish or share the repo.
