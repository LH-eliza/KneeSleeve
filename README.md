# KneeSleeve

KneeSleeve is a lightweight web app that runs directly on an ESP32 and serves as the device’s built-in user interface. The ESP32 hosts the web page and exposes a WebSocket interface so users can connect to the device from a phone, tablet, or computer and interact with it in real time. Focuses on recovery and rehabilitation using IMUs, Wireless Shield for Muscle Sensor Data Transmission, and ESP32 WROOM. 

This project is designed specifically to fit within the ESP32’s limited memory/flash constraints while still providing a responsive, modern-feeling UI.

- `data/` – files served by the ESP32 (if using SPIFFS/LittleFS)
- `src/` – ESP32 firmware source
- `web/` – web UI source (if built separately)

## License

Add your license here (e.g., MIT, Apache-2.0, proprietary).

## Contributing

Contributions, issues, and feature requests are welcome (adjust based on your preference).
