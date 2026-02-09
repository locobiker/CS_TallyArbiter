# Cornerstone Tally Arbiter

[![Arduino](https://img.shields.io/badge/Arduino-00979D?style=flat&logo=Arduino&logoColor=white)](https://www.arduino.cc/)
[![ESP32](https://img.shields.io/badge/ESP32-000000?style=flat&logo=espressif&logoColor=white)](https://www.espressif.com/)
[![M5Stack](https://img.shields.io/badge/M5Stack-00A4E4?style=flat)](https://m5stack.com/)
[![C++](https://img.shields.io/badge/C++-00599C?style=flat&logo=cplusplus&logoColor=white)](https://isocpp.org/)
[![License](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

Enhanced M5StickC firmware for [Tally Arbiter](https://tallyarbiter.com/) optimized for Ross video production environments.

## Overview

This repository contains edits to the Tally Arbiter application and Arduino firmware for M5StickC devices that connects to Tally Arbiter servers to provide wireless camera tally indicators. This fork includes optimizations specifically designed for Ross Ultra switcher environments.

Based on the excellent work by [Joseph Adams](https://github.com/josephdadams/TallyArbiter).

## My Customizations:
 - Resolved https://github.com/josephdadams/TallyArbiter/issues/908 issue with updating Carbonite Ultra Tally lights.
 - Cleaned up M5Stick code.  The version I downloaded seemed to be a proof of concept, so I cleaned it up a bit.

## Hardware Requirements

- **M5StickC**, **M5StickC Plus**, or **M5StickC Plus2**
- WiFi network connectivity
- USB-C cable for programming

## Tech Stack

- **Platform**: Arduino / ESP32
- **Language**: C++
- **Libraries**:
  - M5StickCPlus2
  - WebSocketsClient
  - SocketIOclient
  - Arduino_JSON
  - WiFiManager
  - ArduinoOTA

## Production Setup

Our current production workflow:

```
┌─────────────┐
│ Ross Ultra  │
│  Switcher   │
└──────┬──────┘
       │ TSL Data
       ▼
┌─────────────┐
│  Mac Mini   │
│   (Tally    │
│  Arbiter)   │
└──────┬──────┘
       │ WiFi
       ▼
┌─────────────┐     ┌─────────────┐
│  M5Stick 1  │ ... │  M5Stick N  │
│ (Roam Cam)  │     │ (Roam Cam)  │
└─────────────┘     └─────────────┘

Note: Stationary cameras use GPO directly from switcher
```

### Components

1. **Mac Mini** - Hosts Tally Arbiter server on public WiFi network
2. **Ross Switcher** - Sends TSL data to Mac Mini
3. **M5Stick Devices** - Wireless tally indicators for roaming cameras
4. **Stationary Cameras** - Use existing GPO outputs from switcher

## Features

- Real-time tally status from Tally Arbiter server
- Color-coded tally states (preview/program)
- Device reassignment support
- Battery level monitoring
- Remote flash command support
- WiFi credential storage

## Installation for Tally Arbiter
Please see readme.md in TallyArbiter directory for build instructions.

## Installation for M5Stick

### Prerequisites

1. Install [Arduino IDE](https://www.arduino.cc/en/software)
2. Install ESP32 board support:
   - Add `https://dl.espressif.com/dl/package_esp32_index.json` to Board Manager URLs
   - Install "ESP32 by Espressif Systems"
3. Install required libraries via Library Manager:
   - M5StickCPlus2
   - WebSocketsClient by Markus Sattler
   - SocketIoClient by Markus Sattler
   - Arduino_JSON by Arduino
   - WiFiManager by tzapu

### Upload Firmware

1. Clone this repository
2. Open `TallyArbiter.ino` in Arduino IDE
3. Select your board:
   - **Tools > Board > ESP32 Arduino > M5Stick-C**
   - **Tools > Board > ESP32 Arduino > M5Stick-C-Plus**
   - **Tools > Board > ESP32 Arduino > M5Stick-C-Plus2**
4. Connect M5Stick via USB-C
5. Click Upload

### Initial Configuration

1. After first boot, M5Stick creates a WiFi access point named `m5StickC-XXXXXX`
2. Connect to this AP from your phone/computer
3. Configure:
   - Your WiFi network credentials
   - Tally Arbiter server IP address
   - Tally Arbiter server port (default: 4455)
4. Save settings and device will auto-connect

## Usage

### Button Controls

| Button | Press | Long Press (5s) | Long Press (10s) |
|--------|-------|-----------------|------------------|
| **M5 (Front)** | Toggle Settings/Tally Screen | Reset WiFi Settings | - |
| **Action (Side)** | Adjust Brightness | - | - |
| **Power** | - | Restart Device | Power Off |
| **Reset (Side)** | - | Factory Reset | - |

### Display Modes

**Tally Screen** (Default)
- Shows device name
- Background color indicates tally state
- Black = Off tally
- Green/Red/etc = On tally (color from Tally Arbiter)

**Settings Screen**
- WiFi SSID and IP address
- Tally Arbiter server connection info
- Battery level percentage

### OTA Updates

Devices support over-the-air updates when connected to WiFi:

```bash
# Upload new firmware wirelessly
arduino-cli upload -p net:m5StickC-XXXXXX.local --fqbn esp32:esp32:m5stick-c
```

Default OTA password: `tallyarbiter`

## Configuration

### User Configuration Variables

Edit at the top of the sketch before uploading:

```cpp
// Show last message on tally screen
bool LAST_MSG = false;

// Default Tally Arbiter Server (can be changed via WiFi portal)
char tallyarbiter_host[40] = "172.16.40.25";
char tallyarbiter_port[6] = "4455";
```

### Display Settings

```cpp
#define START_BRIGHTNESS 11      // Initial brightness (0-100)
#define MAX_BRIGHTNESS 100       // Maximum brightness
#define BRIGHTNESS_INCREMENT 10  // Brightness step size
```

## Troubleshooting

### Device Won't Connect to WiFi
- Hold Reset button for 3+ seconds to enter config portal
- Reconnect to `m5StickC-XXXXXX` AP and reconfigure

### Screen is Blank
- Press Action button to cycle brightness
- Check battery level (connect to USB-C)

### Not Receiving Tally Data
- Verify Tally Arbiter server is running
- Check server IP and port in Settings screen
- Ensure device is assigned to a camera in Tally Arbiter web interface

### Device Keeps Restarting
- Check power supply (USB-C must provide adequate current)
- Monitor Serial output at 115200 baud for error messages

## Development

### Building from Source

```bash
git clone https://github.com/locobiker/CS_TallyArbiter.git
cd CS_TallyArbiter
arduino-cli compile --fqbn esp32:esp32:m5stick-c-plus2
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:m5stick-c-plus2
```

### Serial Debugging

Connect via Serial Monitor at **115200 baud** to see:
- Boot sequence
- Network connection status
- WebSocket events
- Tally state changes

## Resources

- **Tally Arbiter Official Site**: [tallyarbiter.com](https://tallyarbiter.com/)
- **Original Repository**: [github.com/josephdadams/TallyArbiter](https://github.com/josephdadams/TallyArbiter)
- **M5Stack Documentation**: [docs.m5stack.com](https://docs.m5stack.com/)
- **Video Tutorials**: Available on the [Tally Arbiter website](https://tallyarbiter.com/) (highly recommended)

## License

This project maintains the same license as the original Tally Arbiter project.

## Acknowledgments

- **Joseph Adams** - Original Tally Arbiter creator
- **M5Stack** - Hardware platform
- The broadcast and video production community

---

**Note**: This is a working fork with modifications specific to our production environment. Your mileage may vary depending on your switcher and network setup.
