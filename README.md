# CYD Packet Rain

[![Build and Release Firmware](https://img.shields.io/github/actions/workflow/status/Buzzcola81/cyd-packet-rain/release.yml?label=Build%20and%20Release%20Firmware&logo=github&logoColor=white&style=flat)](https://github.com/Buzzcola81/cyd-packet-rain/actions/workflows/release.yml)
[![Build and Release Web Flasher Page](https://img.shields.io/github/actions/workflow/status/Buzzcola81/cyd-packet-rain/pages/pages-build-deployment?label=Build%20and%20Release%20Web%20Flasher%20Page&logo=github&logoColor=white&style=flat)](https://github.com/Buzzcola81/cyd-packet-rain/actions/workflows/pages/pages-build-deployment)

A multi-screen LVGL UI for the CYD ESP32-2432S028 (Cheap Yellow Display) that controls an OpenWrt router, monitors a WireGuard VPN, shows time and weather, and includes a built-in web admin so you can change credentials without reflashing.

---

## Web Flasher

Flash your CYD directly from the browser — no build tools required:

**[Open Web Flasher](https://buzzcola81.github.io/cyd-packet-rain/)**

Requires Chrome or Edge on desktop with the board connected via USB.

---

## Features

- VPN dashboard: status badge, VPN/public IP, geo-location, RX/TX, uptime, enable/disable toggle (OpenWrt ubus JSON-RPC)
- NTP clock: resizable time/date text (Small/Medium/Large/Extra Large), Melbourne timezone by default (configurable POSIX TZ)
- Weather: current conditions plus 12-hour forecast (4 x 3-hour blocks) from OpenWeatherMap, auto-refresh every 5 minutes
- System info: uptime, heap, PSRAM, WiFi RSSI, IP/MAC, web admin URL
- Matrix screensaver: direct TFT_eSPI rain animation (32 x 24 cells) bypassing LVGL for smooth performance
- Screensaver controls: enable/disable, target screen (Clock/Weather/Matrix), and start delay (30s/1m/2m/5m), all persisted
- Options: display brightness, clock text size, LED max brightness, and LED breathing speed controls (persisted)
- Web admin: built-in HTTP server on port 80 to edit WiFi/OpenWrt/OWM settings without reflashing
- Boot splash: Matrix-style intro animation
- Touch suppression: avoids click-leak between screens
- RGB LED status: blue (connecting), green (VPN up), red (VPN down)

All screens share a unified Matrix-green theme.

---

## Hardware

- Board: ESP32-2432S028 (2.8" 320x240 TFT + resistive touch, ESP32-WROOM-32)
- USB: Micro-USB (CH340 USB-to-serial)
- Display driver: ILI9341 (SPI)
- Touch driver: XPT2046 (HSPI)
- Backlight: GPIO 21 (LEDC PWM @ 20 kHz, 8-bit duty)
- RGB LED: GPIO 4/16/17, active-low

---

## 1. Environment Setup

### Prerequisites

| Tool | Purpose |
|------|---------|
| [VS Code](https://code.visualstudio.com/) | Editor |
| [PlatformIO IDE extension](https://marketplace.visualstudio.com/items?itemName=platformio.platformio-ide) | Build system and upload |
| CH340 USB driver | USB serial communication with the board |

### Step-by-step

1. Install VS Code.
2. Install extensions in VS Code:
	 - PlatformIO IDE
	 - C/C++ Extension Pack
3. Install the CH340 USB driver (Windows usually auto-installs; manual download: <https://www.wch-ic.com/downloads/CH341SER_EXE.html>).
4. Plug in the board and note the COM port in Device Manager.
5. Open this project folder in VS Code. PlatformIO will detect `platformio.ini`.

---

## 2. Project Structure

```text
cyd-packet-rain/
|-- platformio.ini
|-- include/
|   |-- config.h
|   |-- lv_conf.h
|   |-- OpenWrtClient.h
|   |-- RgbLed.h
|   |-- Settings.h
|   |-- UI.h
|   `-- WebAdmin.h
|-- src/
|   |-- main.cpp
|   |-- OpenWrtClient.cpp
|   |-- RgbLed.cpp
|   |-- Settings.cpp
|   |-- UI.cpp
|   `-- WebAdmin.cpp
`-- README.md
```

---

## 3. Configuration

Settings live in two places:

1. Compile-time defaults in `include/config.h` (used on first boot)
2. Runtime values in NVS (updated via web admin)

Edit `include/config.h` before first flash:

```cpp
#define WIFI_SSID     "YourWiFiSSID"
#define WIFI_PASSWORD "YourWiFiPassword"

#define OPENWRT_HOST  "192.168.1.1"
#define OPENWRT_USER  "root"
#define OPENWRT_PASS  "your_password"
#define VPN_INTERFACE "wg0"

#define OWM_API_KEY "your_openweathermap_key"
#define OWM_LAT     "-37.7333"
#define OWM_LON     "145.2167"
#define OWM_LABEL   "Warrandyte, AU"

#define TZ_POSIX    "AEST-10AEDT,M10.1.0,M4.1.0/3"
#define NTP_SERVER  "pool.ntp.org"
```

After first boot, WiFi/OpenWrt/weather settings can be changed via web admin without reflashing.

---

## 4. Building and Uploading

### VS Code

- Build: PlatformIO checkmark (or `Ctrl+Alt+B`)
- Upload: PlatformIO arrow (or `Ctrl+Alt+U`)
- Serial Monitor: PlatformIO plug icon (or `Ctrl+Alt+S`) at 115200 baud

### Terminal

```bash
pio run
pio run --target upload
pio device monitor
```

### Upload troubleshooting

| Problem | Solution |
|---------|----------|
| COM port not found | Install CH340 driver and use a data USB cable |
| Upload timeout | Hold BOOT during connection, release after "Connecting..." |
| Permission denied | Close other serial monitors/apps |
| `min_spiffs.csv` not found | Ensure full repo was cloned |

---

## 5. Screens

Home is a 6-tile carousel. Left/right arrows cycle tiles; center tile opens the selected screen.

| # | Tile | Description |
|---|------|-------------|
| 1 | VPN | Status, VPN/public IP, location, RX/TX, uptime, VPN toggle, connection test |
| 2 | Clock | NTP time and date |
| 3 | Weather | Current conditions + 12-hour forecast, auto-refresh every 5 min |
| 4 | System | Uptime, memory, WiFi info, web admin URL |
| 5 | Matrix | TFT rain screensaver, tap to exit |
| 6 | Options | Backlight and LED controls |

Passive screens (Matrix, Clock, System) suspend blocking network polling to keep UI smooth.

### RGB LED status indicator

| Color | Meaning |
|-------|---------|
| Blue | Booting / WiFi connecting / reconnecting |
| Green | VPN up |
| Red | WiFi up but VPN down |

---

## 6. Web Admin

Once connected to WiFi, the device serves HTTP on port 80:

```text
http://<device-ip>/
```

The IP appears on the System screen and in serial logs:

```text
[WEB] Admin UI: http://192.168.x.y
```

Editable sections:

- WiFi: SSID, password
- OpenWrt: host, username, password, VPN interface
- Weather: OpenWeatherMap API key, latitude, longitude, label

Click Save and Reboot to persist values in NVS (`cfg`) and reboot.

---

## 7. OpenWrt Setup

Enable JSON-RPC support:

```bash
opkg update
opkg install uhttpd-mod-ubus
/etc/init.d/uhttpd restart
```

Default endpoint:

```text
http://<router-ip>/ubus
```

### If you see "Access denied"

1. Create a permissive ACL profile:

```bash
ssh root@192.168.2.1

cat > /usr/share/rpcd/acl.d/superuser.json << 'EOF'
{
	"superuser": {
		"description": "Super user access role",
		"read":  { "ubus": { "*": ["*"] }, "uci": ["*"], "file": {} },
		"write": { "ubus": { "*": ["*"] }, "uci": ["*"] }
	}
}
EOF
```

2. Link root login to all ACL groups:

```bash
cat > /etc/config/rpcd << 'EOF'
config login
		option username 'root'
		option password '$p$root'
		list read '*'
		list write '*'
EOF
```

3. Restart rpcd:

```bash
/etc/init.d/rpcd restart
```

Then reboot/reset CYD and run Test Connection again.

---

## 8. Key Build Notes

- Partition table: `min_spiffs.csv` (1.96 MB app slot)
- LVGL draw buffer: reduced to fit memory with larger fonts
- Touch X axis: mirrored mapping for panel orientation
- Backlight PWM: LEDC channel 0, 20 kHz, 8-bit duty
- NVS namespaces: `cfg` (settings), `ui` (brightness/preferences)

---

## 9. Tag/Release Process

1. Work in feature branch.
2. Open PR to main.
3. Merge PR after CI passes.
4. Bump version in changelog or release notes.
	- Add a short summary of user-facing changes, fixes, and any known issues for the new version.
	- If you maintain a changelog file, you can commit it with:

```bash
git add README.md
git commit -m "docs: update release notes for vX.Y.Z"
git push
```

5. Create tag `vX.Y.Z` and push tag.
	- Example:

```bash
git tag v0.1.0
git push origin v0.1.0
```

	- Or create an annotated tag:

```bash
git tag -a v0.1.0 -m "Release v0.1.0"
git push origin v0.1.0
```

6. Release workflow publishes firmware and updates flasher manifest.
7. Validate by opening the web flasher page and confirming installed version.

---

## License

MIT License

Copyright (c) 2026 Martin Sustaric

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
