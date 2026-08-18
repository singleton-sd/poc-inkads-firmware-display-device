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
    input, select, button { width: 100%; min-height: 44px; padding: 0.8rem;
                    border: 1px solid var(--ssd-color-border-default);
                    border-radius: var(--ssd-radius-md);
                    font: inherit; }
    input:focus, select:focus { outline: 2px solid var(--ssd-color-border-focus); }
    button { border: 0; background: var(--ssd-color-background-brand);
             color: var(--ssd-color-text-on-brand); font-weight: 700;
             cursor: pointer; }
    button.secondary { background: var(--ssd-color-background-default);
             color: var(--ssd-color-gray-900);
             border: 1px solid var(--ssd-color-border-default); }
    small, #status { color: var(--ssd-color-gray-700); }
    #other-wrap[hidden] { display: none; }
  </style>
</head>
<body>
  <main>
    <form id="setup" method="post" action="/save">
      <h1>Set up InkAds</h1>
      <p>Choose the 2.4 GHz Wi-Fi network this device should use.</p>
      <label>
        Available networks
        <select id="network" required>
          <option value="">Scanning...</option>
        </select>
      </label>
      <button class="secondary" type="button" id="refresh">Refresh networks</button>
      <div id="status" role="status">Scanning...</div>
      <label id="other-wrap" hidden>
        Network name
        <input id="ssid" name="ssid" maxlength="32" autocomplete="off">
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
  <script>
    const network=document.querySelector('#network');
    const ssid=document.querySelector('#ssid');
    const otherWrap=document.querySelector('#other-wrap');
    const status=document.querySelector('#status');
    const refresh=document.querySelector('#refresh');
    const form=document.querySelector('#setup');
    const otherValue='__other__';
    function toggleOther(){
      const isOther=network.value===otherValue;
      otherWrap.hidden=!isOther;
      if(isOther){ssid.setAttribute('required','required')}
      else{ssid.removeAttribute('required')}
    }
    async function loadNetworks(){
      status.textContent='Scanning...';
      network.disabled=true;
      refresh.disabled=true;
      try{
        const response=await fetch('/networks');
        const networks=await response.json();
        network.innerHTML='';
        if(!networks.length){
          network.append(new Option('No networks found',''));
        }
        networks.forEach(item=>{
          const label=item.ssid+(item.secure?' (secured)':' (open)')+' '+item.rssi+' dBm';
          network.append(new Option(label,item.ssid));
        });
        network.append(new Option('Other',otherValue));
        status.textContent='';
      }catch(error){
        network.innerHTML='';
        network.append(new Option('Scan failed',''));
        network.append(new Option('Other',otherValue));
        status.textContent='Could not scan networks. Choose Other to type a name.';
      }
      network.disabled=false;
      refresh.disabled=false;
      toggleOther();
    }
    network.addEventListener('change',toggleOther);
    refresh.addEventListener('click',loadNetworks);
    form.addEventListener('submit',()=>{
      if(network.value && network.value!==otherValue){ssid.value=network.value}
    });
    loadNetworks();
  </script>
</body>
</html>
)html";