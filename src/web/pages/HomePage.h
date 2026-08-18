#pragma once

#include <Arduino.h>

const char HOME_PAGE[] PROGMEM = R"html(
<!doctype html>
<html lang="en" data-theme="light">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>InkAds device</title>
  <style>
    {{DESIGN_TOKENS}}
    *{box-sizing:border-box}body{margin:0;min-height:100vh;
    background:var(--ssd-color-background-muted);color:var(--ssd-color-gray-900);
    font-family:var(--ssd-font-family-body);line-height:var(--ssd-line-height-body)}
    main{max-width:880px;margin:0 auto;padding:var(--ssd-space-500) var(--ssd-space-300)}
    .eyebrow{color:var(--ssd-color-text-default);font-size:var(--ssd-font-size-200);
    font-weight:var(--ssd-font-weight-bold);letter-spacing:.14em;text-transform:uppercase}
    h1{max-width:680px;margin:var(--ssd-space-200) 0 var(--ssd-space-300);
    color:var(--ssd-color-gray-900);font-family:var(--ssd-font-family-heading);
    font-size:clamp(2.5rem,9vw,5.5rem);letter-spacing:-.05em;line-height:.95}
    .lead{max-width:620px;margin:0;color:var(--ssd-color-gray-700);
    font-size:1.125rem}.grid{display:grid;grid-template-columns:repeat(3,1fr);
    gap:var(--ssd-space-300);margin-top:var(--ssd-space-500)}.card{
    min-height:150px;padding:var(--ssd-space-400);background:
    var(--ssd-color-background-default);border:1px solid var(--ssd-color-border-default);
    border-radius:var(--ssd-radius-xl)}.card small{display:block;margin-bottom:
    var(--ssd-space-200);color:var(--ssd-color-gray-700);font-size:
    var(--ssd-font-size-200)}.card strong{font-size:1.1rem}.online{display:inline-flex;
    align-items:center;gap:var(--ssd-space-200);color:
    var(--ssd-color-feedback-success-text)}.online::before{content:"";width:10px;
    height:10px;border-radius:var(--ssd-radius-full);background:
    var(--ssd-color-green-400)}.actions{display:flex;flex-wrap:wrap;
    gap:var(--ssd-space-300);margin-top:var(--ssd-space-400)}.button{
    display:inline-flex;align-items:center;justify-content:center;min-height:44px;
    padding:0 var(--ssd-space-300);border:1px solid var(--ssd-color-border-default);
    border-radius:var(--ssd-radius-full);background:
    var(--ssd-color-background-brand);color:var(--ssd-color-text-on-brand);
    font-weight:var(--ssd-font-weight-bold);text-decoration:none;transition:
    background var(--ssd-motion-duration-fast)}.button:hover{background:
    var(--ssd-color-background-brand-hovered)}footer{margin-top:var(--ssd-space-600);
    padding-top:var(--ssd-space-300);border-top:1px solid
    var(--ssd-color-border-default);color:var(--ssd-color-gray-700);
    font-size:var(--ssd-font-size-200)}@media(max-width:700px){.grid{
    grid-template-columns:1fr}main{padding-top:var(--ssd-space-400)}}
  </style>
</head>
<body><main>
  <header>
    <div class="eyebrow">InkAds device</div>
    <h1>Local display control, online.</h1>
    <p class="lead">This device is connected and ready for local administration.
    All interface assets are served directly by the ESP32.</p>
  </header>
  <section class="grid" aria-label="Device status">
    <article class="card"><small>Status</small><strong class="online">Online</strong></article>
    <article class="card"><small>Firmware</small><strong>{{VERSION}}</strong></article>
    <article class="card"><small>Local address</small><strong>{{HOSTNAME}}.local</strong></article>
  </section>
  <nav class="actions" aria-label="Device actions">
    <a class="button" href="/admin">Open administration</a>
  </nav>
  <footer>InkAds · Locally hosted · Microsoft sign-in for administration requires internet access</footer>
</main></body></html>
)html";
