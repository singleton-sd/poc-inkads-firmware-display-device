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
    padding:var(--ssd-space-400);margin-bottom:var(--ssd-space-400)}.meta{display:grid;
    grid-template-columns:repeat(2,1fr);gap:var(--ssd-space-300);
    margin-bottom:var(--ssd-space-400)}.meta div{background:var(--ssd-color-background-muted);
    padding:var(--ssd-space-300);border-radius:var(--ssd-radius-lg)}.meta small{display:block;
    color:var(--ssd-color-gray-700)}form{display:grid;gap:var(--ssd-space-300)}
    input,select,button,textarea{width:100%;min-height:44px;font:inherit;padding:12px;
    border-radius:var(--ssd-radius-md)}input,select{border:1px solid
    var(--ssd-color-border-default);background:var(--ssd-color-background-default)}
    textarea{border:1px solid var(--ssd-color-border-default);background:
    var(--ssd-color-background-default);min-height:7rem;font-family:ui-monospace,SFMono-Regular,Menlo,monospace}
    button{border:0;background:var(--ssd-color-background-brand);color:
    var(--ssd-color-text-on-brand);font-weight:var(--ssd-font-weight-bold);cursor:pointer}
    button.secondary{background:var(--ssd-color-background-default);color:
    var(--ssd-color-gray-900);border:1px solid var(--ssd-color-border-default)}
    button.danger{background:var(--ssd-color-feedback-danger-text);color:
    var(--ssd-color-background-default)}progress{width:100%;accent-color:
    var(--ssd-color-background-brand)}.status{min-height:1.5em;color:
    var(--ssd-color-gray-700)}.warning{border-left:4px solid
    var(--ssd-color-feedback-danger-border);background:
    var(--ssd-color-feedback-danger-background);color:
    var(--ssd-color-feedback-danger-text);padding:var(--ssd-space-300);
    border-radius:var(--ssd-radius-md)}#other-wrap[hidden]{display:none}
    @media(max-width:520px){.meta{grid-template-columns:1fr}}
  </style>
</head>
<body><main>
  <header><div class="eyebrow">Local device administration</div>
    <h1>InkAds</h1>
    <p>Signed in with Microsoft Entra. Firmware updates stay on the local device.</p></header>
  <section class="card">
    <div class="meta"><div><small>Firmware</small><strong>{{VERSION}}</strong></div>
      <div><small>Device address</small><strong>{{IP}}</strong></div>
      <div><small>Wi-Fi network</small><strong>{{SSID}}</strong></div></div>
  </section>
  <section class="card">
    <h2>Change Wi-Fi</h2>
    <p>Scan nearby 2.4 GHz networks or choose Other to type a name that is not listed.</p>
    <form id="wifi">
      <label>Available networks
        <select id="network" required><option value="">Scanning...</option></select>
      </label>
      <button class="secondary" type="button" id="refresh">Refresh networks</button>
      <div id="wifi-status" class="status" role="status">Scanning...</div>
      <label id="other-wrap" hidden>Network name
        <input id="ssid" name="ssid" maxlength="32" autocomplete="off">
      </label>
      <label>Wi-Fi password
        <input name="password" type="password" maxlength="63" autocomplete="new-password">
      </label>
      <button type="submit">Save Wi-Fi and restart</button>
    </form>
  </section>
  <section class="card">
    <h2>Factory reset</h2>
    <p class="warning">This erases saved Wi-Fi settings. The device restarts in setup mode.</p>
    <form id="reset">
      <button class="danger" type="submit">Erase saved data and restart</button>
      <div id="reset-status" class="status" role="status"></div>
    </form>
  </section>
  <section class="card">
    <h2>Firmware update</h2>
    <p class="warning">Keep power connected until the device reboots. If this page
    sat idle for more than 10 minutes, sign in again before uploading.</p>
    <form id="ota"><label>Compiled application binary (.bin)
      <input id="firmware" name="firmware" type="file" accept=".bin" required></label>
      <progress id="progress" max="100" value="0"></progress>
      <button type="submit">Install firmware</button><div id="status" class="status" role="status"></div>
    </form>
    <h2>TLS certificate rotation</h2>
    <p>Upload a renewed certificate bundle for this device hostname.</p>
    <form id="tls"><label>Certificate PEM
      <textarea id="certificatePem" required spellcheck="false"></textarea></label>
      <label>Private key PEM
      <textarea id="privateKeyPem" required spellcheck="false"></textarea></label>
      <button type="submit">Activate certificate</button>
      <div id="tlsStatus" class="status" role="status"></div>
    </form>
    <button class="secondary" id="logout" type="button">Sign out</button>
  </section>
  <script>
    const csrf='{{CSRF}}';
    const otherValue='__other__';
    const network=document.querySelector('#network');
    const ssid=document.querySelector('#ssid');
    const otherWrap=document.querySelector('#other-wrap');
    const wifiStatus=document.querySelector('#wifi-status');
    function toggleOther(){
      const isOther=network.value===otherValue;
      otherWrap.hidden=!isOther;
      if(isOther){ssid.setAttribute('required','required')}
      else{ssid.removeAttribute('required')}
    }
    async function loadNetworks(){
      wifiStatus.textContent='Scanning...';
      network.disabled=true;
      document.querySelector('#refresh').disabled=true;
      try{
        const response=await fetch('/networks',{credentials:'same-origin'});
        const networks=await response.json();
        network.innerHTML='';
        if(!networks.length){network.append(new Option('No networks found',''))}
        networks.forEach(item=>{
          const label=item.ssid+(item.secure?' (secured)':' (open)')+' '+item.rssi+' dBm';
          network.append(new Option(label,item.ssid));
        });
        network.append(new Option('Other',otherValue));
        wifiStatus.textContent='';
      }catch(error){
        network.innerHTML='';
        network.append(new Option('Scan failed',''));
        network.append(new Option('Other',otherValue));
        wifiStatus.textContent='Could not scan networks. Choose Other to type a name.';
      }
      network.disabled=false;
      document.querySelector('#refresh').disabled=false;
      toggleOther();
    }
    network.addEventListener('change',toggleOther);
    document.querySelector('#refresh').addEventListener('click',loadNetworks);
    document.querySelector('#wifi').addEventListener('submit',async event=>{
      event.preventDefault();
      if(network.value && network.value!==otherValue){ssid.value=network.value}
      const body=new URLSearchParams(new FormData(event.target)).toString();
      wifiStatus.textContent='Saving Wi-Fi...';
      try{
        const response=await fetch('/admin/wifi',{method:'POST',credentials:'same-origin',
          headers:{'Content-Type':'application/x-www-form-urlencoded','X-CSRF-Token':csrf},body});
        wifiStatus.textContent=await response.text();
        if(response.status===401||response.status===403)location.reload();
      }catch(error){
        wifiStatus.textContent='Could not save Wi-Fi.';
      }
    });
    document.querySelector('#reset').addEventListener('submit',async event=>{
      event.preventDefault();
      const confirmed=window.confirm('Erase saved Wi-Fi settings and restart setup?');
      if(!confirmed) return;
      const resetStatus=document.querySelector('#reset-status');
      resetStatus.textContent='Erasing saved data...';
      try{
        const response=await fetch('/admin/reset',{method:'POST',credentials:'same-origin',
          headers:{'X-CSRF-Token':csrf}});
        resetStatus.textContent=await response.text();
        if(response.status===401||response.status===403)location.reload();
      }catch(error){
        resetStatus.textContent='Reset failed. Saved data was not changed.';
      }
    });
    const form=document.querySelector('#ota'),file=document.querySelector('#firmware'),
      progress=document.querySelector('#progress'),status=document.querySelector('#status');
    const tlsForm=document.querySelector('#tls'),tlsStatus=document.querySelector('#tlsStatus');
    form.addEventListener('submit',event=>{event.preventDefault();const request=new XMLHttpRequest();
      request.open('POST','/admin/update');request.upload.onprogress=e=>{
        if(e.lengthComputable)progress.value=Math.round(e.loaded/e.total*100)};
      request.onload=()=>{status.textContent=request.responseText;
        if(request.status===401||request.status===403)location.reload();};
      request.onerror=()=>{status.textContent='Update failed. The device was not changed.'};
      request.setRequestHeader('Content-Type','application/octet-stream');
      request.setRequestHeader('X-CSRF-Token',csrf);
      status.textContent='Uploading firmware...';request.send(file.files[0])});
    tlsForm.addEventListener('submit',async event=>{event.preventDefault();
      const certificatePem=document.querySelector('#certificatePem').value.trim();
      const privateKeyPem=document.querySelector('#privateKeyPem').value.trim();
      tlsStatus.textContent='Validating and storing certificate...';
      const response=await fetch('/admin/tls-certificate',{method:'POST',credentials:'same-origin',
        headers:{'Content-Type':'application/json','X-CSRF-Token':csrf},
        body:JSON.stringify({certificate_pem:certificatePem,private_key_pem:privateKeyPem})});
      tlsStatus.textContent=await response.text();
      if(response.status===401||response.status===403)location.reload();
    });
    document.querySelector('#logout').addEventListener('click',async()=>{
      await fetch('/admin/logout',{method:'POST',credentials:'same-origin',
        headers:{'X-CSRF-Token':csrf}});
      location.reload();
    });
    loadNetworks();
  </script>
</main></body></html>
)html";