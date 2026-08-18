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
6. Connect to Wi-Fi, synchronize time over NTP, and enter normal mode.

After credentials are saved, the browser displays a 15-second countdown and
redirects to the stable device-derived mDNS address
`http://inkads-xxxxxx.local/`. The ESP32 advertises that hostname after joining
the configured network. The suffix is derived from the device identity, so it
remains stable across reboots and avoids collisions between InkAds devices.

Administration is served only over HTTPS at `/admin`. Sign-in uses Microsoft
Entra Device Code Flow. The device talks to the tenant authority directly; it
does not host a password and does not use a client secret.

All HTML, JavaScript, CSS, and the required Singleton SD design tokens are
compiled into the firmware. The device does not request internet-hosted assets.
The normal device homepage, provisioning portal, and admin interface all use
the same embedded token variables and local system-font stack.

## Microsoft Entra sign-in

Tenant ID, application/client ID, and the `InkAds.Admin` role name live in
`src/config/EntraConfig.h`. Those values are public identifiers, not secrets.

The firmware requests `openid` and `{clientId}/.default`, then validates the ID
token signature against the tenant JWKS. Issuer, audience, expiry, tenant, and
the `InkAds.Admin` role must match. If the ID token has no role claim, the
access token is checked the same way. Refresh tokens are not requested or stored.

Administrator sessions live in RAM only:

- Cookie: `inkads_session`, `HttpOnly`, `Secure`, `SameSite=Strict`
- 10 minute inactivity timeout
- 30 minute absolute timeout
- Cleared on reboot, sign-out, or failed CSRF/session checks
- State-changing routes require the `X-CSRF-Token` header

Microsoft sign-in requires internet access from the ESP32. Firmware upload
after sign-in stays on the local HTTPS connection.

### App registration checklist

Create or verify a single-tenant app registration:

1. Treat it as a public client. Do not create or embed a client secret.
2. Enable Device Code Flow.
3. Define an application role named `InkAds.Admin`.
4. Require assignment on the Enterprise Application.
5. Assign only approved users or groups to `InkAds.Admin`.
6. Apply Conditional Access and MFA where licensing permits.
7. Use the tenant-specific authority, never `/common`.

### Physical recovery

There is no remote password bypass. If Entra IDs, TLS certificates, or Wi-Fi
settings are wrong:

1. Connect USB and flash corrected firmware (`EntraConfig.h` and
   `TlsCredentials.local.h`).
2. To clear Wi-Fi settings during development, call `configStore.clear()` from
   a temporary path, upload once, then remove that call.

A dedicated physical factory-reset control is still a follow-up task.

## Debug logging

With `DeviceConfig::debugLogging` enabled, Serial output records the SSID and
the presence and length of the Wi-Fi password. Password values, device codes,
tokens, cookies, and token subject claims are never printed. Coarse Entra
states and free heap are logged after HTTPS start, JWKS refresh, and
authorization.

On boot, any leftover NVS `admin_pass` value from earlier firmware is deleted.

## Releases

Release automation runs on merges to `main` and does not open version-bump PRs.

- Conventional Commits drive semantic versioning (`feat` -> minor, `fix/perf/refactor/revert` -> patch, breaking change -> major).
- The release workflow updates `version.json` and `src/config/DeviceConfig.h`, prepends `CHANGELOG.md` using `git-cliff`, creates a release commit, and tags it as `vX.Y.Z`.
- Pushing the release tag triggers `.github/workflows/firmware-release.yml`, which builds binaries and publishes the GitHub release assets.

To keep release behavior predictable, use Conventional Commit titles for squash-merge commits, including a ticket reference as described in [CONTRIBUTING.md](CONTRIBUTING.md).

## Arduino IDE

Open `display-device.ino`, select `MH ET LIVE ESP32MiniKit` (or `ESP32 Dev
Module`), select an OTA-capable partition scheme, and upload.

Copy `src/config/TlsCredentials.local.example.h` to
`src/config/TlsCredentials.local.h` and provision a device certificate whose
SAN matches `inkads-xxxxxx.local`.

No third-party libraries are required. `Preferences`, `DNSServer`, `WebServer`,
`WiFi`, ESP-IDF HTTPS, `cJSON`, and `mbedtls` are supplied by the Espressif
Arduino core.

## Manual Entra tests

- Assigned `InkAds.Admin` user: can complete Device Code Flow and open `/admin`.
- Tenant user without the app role: denied.
- User from another tenant or a personal Microsoft account: denied.
- Expired device code: denied, with restart offered.
- User declines Microsoft consent: denied.
- Device loses internet while polling: offline failure, no session.
- Refresh the sign-in page while pending: no session is created.
- Session expires after the OTA file is chosen but before upload: upload
  rejected.
- Tampered session cookie: rejected.
- Reboot: previous session is gone.
- Signing-key rotation: the next validation refreshes JWKS.
