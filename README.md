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
5. Scan nearby 2.4 GHz networks, save submitted credentials to NVS, and restart.
6. Connect to Wi-Fi, synchronize time over NTP, and enter normal mode.

After credentials are saved, the browser displays a 15-second countdown and
redirects to the stable device-derived mDNS address
`http://inkads-xxxxxx.local/`. The ESP32 advertises that hostname after joining
the configured network. The suffix is derived from the device identity, so it
remains stable across reboots and avoids collisions between InkAds devices.

Administration is served only over HTTPS at `/admin`. The trusted URL is
`https://inkads-xxxxxx.devices.singletonsd.com/admin`, where `xxxxxx` is the
device suffix from `ESP.getEfuseMac()`. Sign-in uses Microsoft Entra Device
Code Flow. The device talks to the tenant authority directly; it does not host
a password and does not use a client secret.

The admin page can scan and change the 2.4 GHz Wi-Fi network, erase saved
settings, rotate the TLS certificate, and install an Arduino application
`.bin` through the ESP32 OTA partition. Setup and admin both load
`/networks` for a scanned SSID list, with an Other option for hidden or
missing names. Password values are never logged.

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

## TLS certificate storage and rotation

TLS certificate/key material is stored in writable flash (SPIFFS), not in the
application binary:

- active certificate: `/tls_active_cert.pem`
- active key: `/tls_active_key.pem`
- staged certificate: `/tls_staged_cert.pem`
- staged key: `/tls_staged_key.pem`

At boot, HTTPS starts only if a valid active bundle is present. If no flash
bundle exists, the firmware attempts one-time bootstrap from
`src/config/TlsCredentials.local.h` (for development provisioning), validates
it, then writes it to flash.

### Admin-page certificate rotation (phase 1)

1. Sign in to `https://inkads-xxxxxx.devices.singletonsd.com/admin`.
2. Upload renewed certificate PEM + private key PEM in the **TLS certificate
   rotation** form.
3. The device validates parseability, key/cert pairing, expiry, and hostname
   match against `inkads-xxxxxx.devices.singletonsd.com`.
4. On success, staged files are promoted to active and used on next HTTPS
   restart.

If validation fails, the current active certificate remains unchanged.

### Operator scripts

Use Node scripts under `scripts/device-tls`:

- `publish-device-dns.mjs` (Route 53 A record upsert)
- `issue-device-cert.mjs` (Let's Encrypt DNS-01 issuance)
- `upload-device-cert.mjs` (authenticated upload to `/admin/tls-certificate`)

Environment variables:

- `HOSTED_ZONE_ID`, `DEVICE_HOSTNAME`, `DEVICE_IP` for DNS publish
- `ACME_EMAIL`, `HOSTED_ZONE_ID`, `DEVICE_HOSTNAME` for cert issuance
- `DEVICE_BASE_URL`, `SESSION_COOKIE`, `CSRF_TOKEN` for upload

### Server-pull follow-up (phase 2)

Automated device pull from a trusted service is tracked separately in
[POC-249 follow-up](https://app.clickup.com/t/86d42hdwk). It is not implemented
in this firmware change.

### Physical recovery

There is no remote password bypass. If Entra IDs, TLS certificates, or Wi-Fi
settings are wrong:

1. Connect USB and flash corrected firmware if Entra config is wrong.
2. If TLS material in flash is invalid/expired and remote rotation cannot be
   completed, re-provision via admin upload after bootstrapping a temporary
   local cert (`TlsCredentials.local.h`) if needed.
3. Use the factory reset action on the local admin page to clear saved Wi-Fi
   and reboot back into setup mode.

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
- When a version bump is due, the release workflow updates `version.json` and `src/config/DeviceConfig.h`, prepends `CHANGELOG.md` using `git-cliff`, compiles every `targets.json` row, tags `vX.Y.Z`, and attaches `inkads-{filename}-{suffix}-v{version}.bin`, `inkads-manifest.json`, and remaining `dist/*.bin` extras to the GitHub Release.
- Unversioned pushes to `main` (`docs`, `ci`, `chore`, and similar) do not compile. Pull requests still compile in Firmware build.
- To republish an existing tag, run Actions → Firmware release → Run workflow.

To keep release behavior predictable, use Conventional Commit titles for squash-merge commits, including a ticket reference as described in [CONTRIBUTING.md](CONTRIBUTING.md).

## Arduino IDE

Open `display-device.ino`, select `MH ET LIVE ESP32MiniKit` (or `ESP32 Dev
Module`), select an OTA-capable partition scheme, and upload. Local IDE
builds use the committed defaults in `src/config/InkAdsFeatures.h`. CI
overwrites that header per `targets.json` row before `arduino-cli compile`.

For first-time bootstrap only, copy `src/config/TlsCredentials.local.example.h`
to `src/config/TlsCredentials.local.h` and provision a device certificate whose
subject contains `inkads-xxxxxx.devices.singletonsd.com`.

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
- Authenticated admin can refresh networks, change Wi-Fi, and factory reset.
