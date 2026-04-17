
# FlexCANFlasher

An ESP32 Over-The-Air (OTA) firmware update system. A Python-based HTTPS server hosts firmware binaries, an MQTT broker notifies ESP32 devices of new builds, and the ESP32 firmware downloads the .bin (next step flash over CAN bus)

## Architecture

```
┌──────────────┐       MQTT notification        ┌──────────────┐
│  mqtt_server  │ ──────────────────────────────► │    ESP32      │ ──────────────────────────────►SPIFFS
│  (watchdog)   │   "firmware/update" topic       │  OTA_Firmware │
└──────────────┘                                  └──────┬───────┘
                                                         │
┌──────────────┐       HTTPS download                    │
│firmware_server│ ◄──────────────────────────────────────┘
│  (HTTPS/TLS) │   fetches .bin via URL
└──────────────┘
```

1. **mqtt_server.py** – Watches the build directory for new `.bin` files and publishes the firmware path to an MQTT topic.
2. **firmware_server.py** – Serves firmware binaries over HTTPS with TLS (self-signed certificates).
3. **OTA_Firmware** – ESP-IDF firmware that connects to Wi-Fi, subscribes to the MQTT topic, downloads the new firmware from the HTTPS server, and writes it to SPIFFS / OTA partition.
4. **BlinkFirmware** – A simple LED blink example used as the initial or test firmware to be replaced via OTA.

## Project Structure

```
FlexCANFlasher/
├── config.json              # Central configuration (MQTT, server, Wi-Fi, project)
├── firmware_server.py       # HTTPS firmware server
├── mqtt_server.py           # MQTT build-watcher & notifier
├── requirements.txt         # Python dependencies
├── wokwi.toml               # Wokwi simulator config
├── diagram.json             # Wokwi board diagram
├── Helper/                  # Python config helper classes
│   ├── ConfigLoader.py      # JSON config loader
│   ├── MQTTConfig.py        # MQTT settings wrapper
│   ├── ProjectConfig.py     # Project settings wrapper
│   └── ServerConfig.py      # Server settings wrapper
├── OTA_Firmware/            # ESP-IDF OTA firmware project
│   ├── main/
│   │   ├── main.cpp                 # App entry point
│   │   ├── ConfigBuilder.h          # Config access from embedded config.json
│   │   ├── JSON_Parser.h            # cJSON-based config parser
│   │   ├── WifiConnectionBuilder.h  # Wi-Fi STA setup
│   │   ├── MQTTConnectionBuilder.h  # MQTT client + OTA queue
│   │   └── OTAConnectionBuilder.h   # HTTPS OTA download logic
│   └── partitions.csv               # Custom partition table (OTA + SPIFFS)
├── OTA_Firmware_tests/      # Unit / integration tests (ESP-IDF)
├── BlinkFirmware/           # Simple blink firmware (test payload)
│   └── ESP/                # ESP Blink
│       └── main/
│           └── blink_example_main.c
|       STM will be soon added
├── certs/                   # TLS certificates (generated)
└── scripts/
    ├── create_certs.sh      # Generate self-signed TLS certs
    ├── setup_py_venv.sh     # Create Python venv & install deps
    ├── kill_firmware_server.sh  # Stop the HTTPS server by PID
    └── start_tests.sh       # Build, flash & monitor tests
```

## Prerequisites

- **ESP-IDF** v5.4+ installed and sourced (`export.sh`)
- **Python 3** with `venv` support
- **Mosquitto** MQTT broker
- **OpenSSL** (for certificate generation)
- **jq** (used by `kill_firmware_server.sh`)

## Configuration


All current settings are centralized in a `config.json`:
(Use the example.config.json)
```json
{
    "mqtt": {
        "broker": "XXX.XXX.XXX.XXX",
        "port": 1883,
        "topic": "firmware/update"
    },
    "server": {
        "external_ip": "XXX.XXX.XXX.XXX",
        "ip": "0.0.0.0",
        "port": 12345,
        "pid": 22365,
        "cert_dir": "certs"
    },
    "project": {
        "platform": "ESP",
        "idf_path": "YOUR IDF PATH",
        "build_dir": "BlinkFirmware/ESP/build",
        "project_name": "BlinkFirmware_ESP"
    },
    "router": {
        "ssid": "YOUR SSID",
        "password": "YOUR PASSWORD"
    }
}
```

The same `config.json` is read by:
- Python servers (via `Helper/ConfigLoader.py`)
- ESP32 firmware (embedded at build time and parsed with cJSON)
- Shell scripts (via `python3 -c` / `jq`)

## Setup

### 1. Install Mosquitto MQTT Broker

```bash
sudo apt install mosquitto mosquitto-clients
sudo systemctl start mosquitto
```

### 2. Create Python Virtual Environment

```bash
cd scripts
./setup_py_venv.sh
```

This creates a `venv_<platform>/` directory and installs all packages from `requirements.txt`.

### 3. Generate TLS Certificates

```bash
cd scripts
./create_certs.sh
```

Generates `server_cert.pem` and `server_key.pem` in the directory specified by `config.json` → `server.cert_dir`. The certificate CN and SAN are set to the configured `external_ip`.

### 4. Build the OTA Firmware

```bash
source /path/to/esp-idf/export.sh
cd OTA_Firmware
idf.py build
```

### 5. Build the Blink Firmware (Test Payload)

```bash
cd BlinkFirmware/ESP
idf.py build
```

## Usage

### Start the HTTPS Firmware Server

```bash
source venv_esp/bin/activate
python firmware_server.py
```

Serves files over `https://0.0.0.0:PORT` (1234 in config.json, set like you wish) using the generated TLS certificates.
HINT: Use your external IP also, because your Controller will

### Start the MQTT Build Watcher

```bash
source venv_esp/bin/activate
python mqtt_server.py
```

Watches the configured `build_dir` for new/modified `.bin` files and publishes their path to the MQTT topic `firmware/update`.

### Flash the OTA Firmware to ESP32

```bash
cd OTA_Firmware
idf.py flash -p /dev/ttyUSB0
idf.py monitor
```

The ESP32 will:
1. Connect to the configured Wi-Fi network
2. Subscribe to the MQTT topic
3. Wait for a firmware update notification
4. Download the new firmware over HTTPS
5. Write it to SPIFFS

NEXT: Will send it over CAN 

### Stop the Firmware Server

```bash
cd scripts
./kill_firmware_server.sh
```

## Running Tests, does not cover much, is more for easier development

```bash
cd scripts
./start_tests.sh
```
Logs are saved to `OTA_Firmware_tests/logs/`.

## Wokwi Simulation

The project includes a Wokwi configuration (`wokwi.toml` + `diagram.json`) for simulating the ESP32 DevKit V1 with the test firmware.
Not realy used at the moment

## Partition Table

The OTA firmware uses a custom partition layout (`OTA_Firmware/partitions.csv`):

| Name    | Type | SubType | Offset     | Size       |
|---------|------|---------|------------|------------|
| nvs     | data | nvs     | 0x9000     | 0x5000     |
| otadata | data | ota     | 0xe000     | 0x2000     |
| app0    | app  | ota_0   | 0x10000    | 0x140000   |
| spiffs  | data | spiffs  | 0x150000   | 0x2B0000   |