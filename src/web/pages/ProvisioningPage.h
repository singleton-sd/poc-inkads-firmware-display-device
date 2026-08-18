#pragma once

#include <Arduino.h>

const char PROVISIONING_PAGE[] PROGMEM = R"html(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Set up InkAds</title>
  <style>
    {{DESIGN_TOKENS}}
    * { box-sizing: border-box; }
    body { margin: 0; background: var(--ssd-color-background-muted);
           color: var(--ssd-color-gray-900);
           font-family: var(--ssd-font-family-body);
           line-height: var(--ssd-line-height-body); }
    main { max-width: 28rem; margin: 8vh auto; padding: var(--ssd-space-300); }
    form { display: grid; gap: var(--ssd-space-300);
           background: var(--ssd-color-background-default);
           padding: var(--ssd-space-400); border: 1px solid var(--ssd-color-border-default);
           border-radius: var(--ssd-radius-xl); }
    h1, p { margin: 0; }
    label { display: grid; gap: 0.4rem; font-weight: 650; }
    input, button { width: 100%; padding: 0.8rem;
                    border: 1px solid var(--ssd-color-border-default);
                    border-radius: var(--ssd-radius-md);
                    font: inherit; }
    input:focus { outline: 2px solid var(--ssd-color-border-focus); }
    button { border: 0; background: var(--ssd-color-background-brand);
             color: var(--ssd-color-text-on-brand); font-weight: 700;
             cursor: pointer; }
    small { color: var(--ssd-color-gray-700); }
  </style>
</head>
<body>
  <main>
    <form method="post" action="/save">
      <h1>Set up InkAds</h1>
      <p>Enter the 2.4 GHz Wi-Fi network this device should use.</p>
      <label>
        Network name
        <input name="ssid" maxlength="32" autocomplete="off" required>
      </label>
      <label>
        Wi-Fi password
        <input name="password" type="password" maxlength="63"
               autocomplete="new-password">
      </label>
      <small>After the device joins Wi-Fi, administration uses Microsoft Entra
      sign-in. That step requires internet access.</small>
      <button type="submit">Save and connect</button>
      <small>The device will restart after saving.</small>
    </form>
  </main>
</body>
</html>
)html";
