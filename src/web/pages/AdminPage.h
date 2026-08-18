#pragma once

#include <Arduino.h>

const char ADMIN_PAGE[] PROGMEM = R"html(
<!doctype html>
<html lang="en" data-theme="light">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>InkAds administration</title>
  <style>
    {{DESIGN_TOKENS}}
    *{box-sizing:border-box}body{margin:0;background:var(--ssd-color-background-muted);
    color:var(--ssd-color-gray-900);font-family:var(--ssd-font-family-body);
    line-height:var(--ssd-line-height-body)}main{max-width:720px;margin:0 auto;
    padding:var(--ssd-space-500) var(--ssd-space-300)}header{margin-bottom:var(--ssd-space-400)}
    .eyebrow{color:var(--ssd-color-text-default);font-size:var(--ssd-font-size-200);
    font-weight:var(--ssd-font-weight-bold);text-transform:uppercase;letter-spacing:.12em}
    h1{margin:var(--ssd-space-200) 0;font-size:var(--ssd-font-size-400)}
    .card{background:var(--ssd-color-background-default);border:1px solid
    var(--ssd-color-border-default);border-radius:var(--ssd-radius-xl);
    padding:var(--ssd-space-400)}.meta{display:grid;grid-template-columns:repeat(2,1fr);
    gap:var(--ssd-space-300);margin-bottom:var(--ssd-space-400)}.meta div{
    background:var(--ssd-color-background-muted);padding:var(--ssd-space-300);
    border-radius:var(--ssd-radius-lg)}.meta small{display:block;color:var(--ssd-color-gray-700)}
    form{display:grid;gap:var(--ssd-space-300)}input,button{width:100%;font:inherit;
    padding:12px;border-radius:var(--ssd-radius-md)}input{border:1px solid
    var(--ssd-color-border-default);background:var(--ssd-color-background-default)}
    button{border:0;background:var(--ssd-color-background-brand);color:
    var(--ssd-color-text-on-brand);font-weight:var(--ssd-font-weight-bold);cursor:pointer}
    progress{width:100%;accent-color:var(--ssd-color-background-brand)}
    #status{min-height:1.5em;color:var(--ssd-color-gray-700)}
    .warning{border-left:4px solid var(--ssd-color-feedback-danger-border);
    background:var(--ssd-color-feedback-danger-background);color:
    var(--ssd-color-feedback-danger-text);padding:var(--ssd-space-300);
    border-radius:var(--ssd-radius-md)}@media(max-width:520px){.meta{grid-template-columns:1fr}}
  </style>
</head>
<body><main>
  <header><div class="eyebrow">Local device administration</div>
    <h1>InkAds</h1><p>Manage this device without an internet connection.</p></header>
  <section class="card">
    <div class="meta"><div><small>Firmware</small><strong>{{VERSION}}</strong></div>
      <div><small>Device address</small><strong>{{IP}}</strong></div></div>
    <h2>Firmware update</h2>
    <p class="warning">Keep power connected until the device reboots.</p>
    <form id="ota"><label>Compiled application binary (.bin)
      <input id="firmware" name="firmware" type="file" accept=".bin" required></label>
      <progress id="progress" max="100" value="0"></progress>
      <button type="submit">Install firmware</button><div id="status" role="status"></div>
    </form>
  </section>
  <script>
    const form=document.querySelector('#ota'),file=document.querySelector('#firmware'),
      progress=document.querySelector('#progress'),status=document.querySelector('#status');
    form.addEventListener('submit',event=>{event.preventDefault();const request=new XMLHttpRequest();
      request.open('POST','/admin/update');request.upload.onprogress=e=>{
        if(e.lengthComputable)progress.value=Math.round(e.loaded/e.total*100)};
      request.onload=()=>{status.textContent=request.responseText};
      request.onerror=()=>{status.textContent='Update failed. The device was not changed.'};
      request.setRequestHeader('Content-Type','application/octet-stream');
      status.textContent='Uploading firmware...';request.send(file.files[0])});
  </script>
</main></body></html>
)html";
