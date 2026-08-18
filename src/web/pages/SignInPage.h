#pragma once

#include <Arduino.h>

const char SIGN_IN_PAGE[] PROGMEM = R"html(
<!doctype html>
<html lang="en" data-theme="light">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Sign in to InkAds</title>
  <style>
    {{DESIGN_TOKENS}}
    *{box-sizing:border-box}body{margin:0;background:var(--ssd-color-background-muted);
    color:var(--ssd-color-gray-900);font-family:var(--ssd-font-family-body);
    line-height:var(--ssd-line-height-body)}main{max-width:32rem;margin:8vh auto;
    padding:var(--ssd-space-300)}.card{background:var(--ssd-color-background-default);
    border:1px solid var(--ssd-color-border-default);border-radius:var(--ssd-radius-xl);
    padding:var(--ssd-space-400)}h1{margin:0 0 var(--ssd-space-200);
    font-size:var(--ssd-font-size-400)}p{color:var(--ssd-color-gray-700)}
    button,.link{display:inline-flex;align-items:center;justify-content:center;
    min-height:44px;width:100%;border:0;border-radius:var(--ssd-radius-md);
    background:var(--ssd-color-background-brand);color:var(--ssd-color-text-on-brand);
    font:inherit;font-weight:var(--ssd-font-weight-bold);text-decoration:none;
    cursor:pointer}button.secondary{margin-top:var(--ssd-space-200);
    background:var(--ssd-color-background-default);color:var(--ssd-color-gray-900);
    border:1px solid var(--ssd-color-border-default)}.code{font-size:1.6rem;
    letter-spacing:.12em;font-weight:var(--ssd-font-weight-bold)}
    .warning{border-left:4px solid var(--ssd-color-feedback-danger-border);
    background:var(--ssd-color-feedback-danger-background);color:
    var(--ssd-color-feedback-danger-text);padding:var(--ssd-space-300);
    border-radius:var(--ssd-radius-md);margin:var(--ssd-space-300) 0}
    .note{border-left:4px solid var(--ssd-color-border-default);
    background:var(--ssd-color-background-muted);padding:var(--ssd-space-300);
    border-radius:var(--ssd-radius-md);margin:var(--ssd-space-300) 0}
    #panel{margin-top:var(--ssd-space-300)}#status{min-height:1.5em}
  </style>
</head>
<body><main><section class="card">
  <h1>InkAds administration</h1>
  <p>Sign in with a Microsoft Entra account assigned <strong>InkAds.Admin</strong>.</p>
  <p class="note">Microsoft sign-in requires internet access from this device.
  Personal Microsoft accounts and users from other tenants are denied.</p>
  <button id="signin" type="button">Sign in with Microsoft</button>
  <div id="panel" hidden>
    <p>Open Microsoft and enter this code. It expires in <span id="expires">-</span>s.</p>
    <p class="code" id="userCode">------</p>
    <a class="link" id="verify" href="https://microsoft.com/devicelogin" target="_blank"
       rel="noopener">Open Microsoft verification page</a>
    <button class="secondary" id="cancel" type="button">Cancel</button>
  </div>
  <p id="status" role="status"></p>
  <p class="warning" id="error" hidden></p>
</section>
<script>
  const csrf='{{LOGIN_CSRF}}';
  const signin=document.querySelector('#signin');
  const panel=document.querySelector('#panel');
  const status=document.querySelector('#status');
  const error=document.querySelector('#error');
  const userCode=document.querySelector('#userCode');
  const verify=document.querySelector('#verify');
  const expires=document.querySelector('#expires');
  let timer=null;
  function headers(){return {'X-CSRF-Token':csrf};}
  function showError(text){error.hidden=!text;error.textContent=text||'';}
  function stateMessage(state){
    if(state==='pending')return 'Waiting for Microsoft sign-in, including MFA.';
    if(state==='denied')return 'Sign-in was denied. The required InkAds.Admin role was missing, or Microsoft rejected the request.';
    if(state==='timeout')return 'The device code expired. Start again.';
    if(state==='offline')return 'This device lost internet access while contacting Microsoft. Check the network and try again.';
    if(state==='cancelled')return 'Sign-in was cancelled.';
    if(state==='misconfigured')return 'Entra tenant or client ID is not configured on this device.';
    return '';
  }
  async function readStatus(){
    const response=await fetch('/admin/session',{credentials:'same-origin'});
    return response.json();
  }
  function render(data){
    if(data.state==='authorized'){location.reload();return;}
    const pending=data.state==='pending';
    panel.hidden=!pending;
    signin.hidden=pending;
    if(pending){
      userCode.textContent=data.user_code||'------';
      expires.textContent=String(data.expires_in||0);
      const href=data.verification_uri_complete||data.verification_uri||
        'https://microsoft.com/devicelogin';
      verify.href=href;
      status.textContent=stateMessage('pending');
      showError('');
    }else{
      status.textContent='';
      showError(stateMessage(data.state));
    }
  }
  async function start(){
    showError('');
    status.textContent='Contacting Microsoft...';
    const response=await fetch('/admin/session',{method:'POST',credentials:'same-origin',
      headers:headers()});
    if(!response.ok){showError('Could not start Microsoft sign-in.');return;}
    render(await readStatus());
    if(timer)clearInterval(timer);
    timer=setInterval(async()=>{try{render(await readStatus());}catch(e){
      showError('Lost contact with this device.');}},2000);
  }
  async function cancel(){
    await fetch('/admin/session/cancel',{method:'POST',credentials:'same-origin',
      headers:headers()});
    if(timer)clearInterval(timer);
    render(await readStatus());
  }
  signin.addEventListener('click',start);
  document.querySelector('#cancel').addEventListener('click',cancel);
</script>
</main></body></html>
)html";
