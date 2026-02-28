# KneeSleeve

KneeSleeve is a lightweight web app that runs directly on an ESP32 and serves as the device’s built-in user interface. The ESP32 hosts the web page and exposes a WebSocket interface so users can connect to the device from a phone, tablet, or computer and interact with it in real time. Focuses on recovery and rehabilitation using IMUs, Wireless Shield for Muscle Sensor Data Transmission, and ESP32 WROOM. 

This project is designed specifically to fit within the ESP32’s limited memory/flash constraints while still providing a responsive, modern-feeling UI.

## What it does

- Hosts a web UI from the ESP32 (the ESP32 is the server/host)
- Lets users connect to the device from their own browser
- Communicates with the ESP32 using WebSockets for low-latency, real-time updates
- Allows users to use the application with the KneeSleeve and get accurate measurements of their shin and thigh movement (connected via KneeSleeve)
- Focuses on rehabilitation and recovery of the elderly, recovering patients, and any user who wants to have data/accuracy
- Provides a way to configure device networking (Wi‑Fi / “internet through settings”)

## Why WebSockets

WebSockets keep a persistent connection between the browser and the ESP32, which makes it well-suited for:

- Live status updates (sensor values, state, connectivity, etc.)
- Instant control actions (toggles, sliders, commands)
- Reduced overhead compared to repeated HTTP polling

## Design goals

- **Small footprint:** minimize asset size and runtime memory usage
- **Fast load times:** optimized for embedded hosting
- **Reliable on constrained hardware:** avoid heavy frameworks and large bundles
- **Simple to extend:** easy to add new messages/commands and UI views

## Typical usage flow

1. Power on the ESP32 running KneeSleeve firmware.
2. Connect to the ESP32 (AP mode or existing Wi‑Fi, depending on configuration).
3. Open the device’s hosted page in a browser.
4. The page connects back to the ESP32 over WebSocket.
5. Use the UI to view status, change settings, and configure networking.

## WebSocket interface (high level)

The browser establishes a WebSocket connection to the ESP32. Messages are exchanged in an application-defined format (commonly JSON or a compact binary format, depending on memory constraints).

You can document your exact protocol here, for example:

- **Inbound (browser → ESP32):** commands like `set_wifi`, `start`, `stop`, `set_mode`, etc.
- **Outbound (ESP32 → browser):** status updates, acknowledgements, errors, telemetry, etc.

> If you’re using JSON: consider keeping keys short and message sizes small to reduce RAM usage.

## Development notes

Because this UI targets an ESP32 environment, keep in mind:

- Prefer small, static assets
- Avoid large dependencies and big icon/font libraries
- Minify/compress where possible
- Be mindful of heap usage on both the ESP32 and in the browser

## Project structure

Describe your repo layout here once finalized, e.g.

- `data/` – files served by the ESP32 (if using SPIFFS/LittleFS)
- `src/` – ESP32 firmware source
- `web/` – web UI source (if built separately)

## License

Add your license here (e.g., MIT, Apache-2.0, proprietary).

## Contributing

Contributions, issues, and feature requests are welcome (adjust based on your preference).
