# InkAds display-device firmware

Arduino firmware for the MH-ET LIVE ESP32 MiniKit.

Arduino requires the primary `.ino` filename to match its containing sketch
folder, so the thin entrypoint is named `display-device.ino`.

## Current startup flow

1. Read Wi-Fi credentials from ESP32 Preferences/NVS.
2. Attempt to connect for 15 seconds.
3. If configuration is absent or connection fails, create
   `InkAds-Setup-XXXXXX`.
4. Run wildcard DNS and a captive setup page at `http://192.168.4.1`.
5. Save submitted credentials to NVS and restart.
6. Connect to Wi-Fi and enter normal mode.

After credentials are saved, the browser displays a 15-second countdown and
redirects to the stable device-derived mDNS address
`http://inkads-xxxxxx.local/`. The ESP32 advertises that hostname after joining
the configured network. The suffix is derived from the device identity, so it
remains stable across reboots and avoids collisions between InkAds devices.

The provisioning form also captures an admin password. Normal mode serves a
local-only admin page at `/admin` using HTTP Basic authentication. The admin
page accepts an Arduino application `.bin` and installs it through the ESP32
OTA partition before rebooting.

All HTML, JavaScript, CSS, and the required Singleton SD design tokens are
compiled into the firmware. The device does not request internet-hosted assets.
The normal device homepage, provisioning portal, and admin interface all use
the same embedded token variables and local system-font stack.

## Debug logging

With `DeviceConfig::debugLogging` enabled, Serial output records the SSID and
the presence and length of submitted/loaded passwords. Password values are
never printed. Logs are emitted when the provisioning form is submitted and
when configuration is loaded from persistent memory at boot.

## Arduino IDE

Open `display-device.ino`, select `MH ET LIVE ESP32MiniKit` (or `ESP32 Dev
Module`), select an OTA-capable partition scheme, and upload.

No third-party libraries are required. `Preferences`, `DNSServer`, `WebServer`,
and `WiFi` are supplied by the Espressif Arduino core.

## Resetting configuration during development

Call `configStore.clear()` from a temporary, explicitly triggered development
path, upload once, and then remove that call. A deliberate physical reset flow
will be added in a follow-up task.
