#pragma once

// Provisioning/status web page, fully inline (HTML + CSS + JS, no CDN —
// there's no internet in AP mode). Lives as a const char[] in the ESP32's
// memory-mapped flash -> client.print() streams it directly, no heap,
// no String.
//
// Four tabs, always all visible (no mode-dependent show/hide, keeps the JS
// logic simple): Status | Password | WiFi | Sensors. Status is the new
// landing page and works standalone even during live operation (STA,
// provisioned=true) — device name + system info, no completed onboarding
// required. Live sensor values + current calibration values deliberately
// live only in the Sensors tab (no dashboard here, the Companion App will
// handle that later). No separate Finish tab anymore — instead a banner in
// the Status tab that only appears while provisioned=false ("Finish &
// Reboot" would be harmless to click again during live operation anyway,
// but the banner isn't visible then at all).
//
// Data-driven: the JS builds the sensor overview and tabs from GET
// /calibrationinfo, so no sensor knowledge is hardcoded here (same
// principle as the Companion App). New sensor types don't require any
// change to this file.
inline const char PROVISIONING_HTML[] = R"html(<!DOCTYPE html>
<html lang="en"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>SensorNode Setup</title>
<style>
*{box-sizing:border-box}
body{margin:0;font-family:system-ui,-apple-system,Segoe UI,Roboto,sans-serif;background:#eef1ef;color:#1f2b22;display:flex;justify-content:center;padding:24px}
.card{background:#fff;width:100%;max-width:440px;border-radius:14px;box-shadow:0 6px 24px rgba(0,0,0,.08);padding:24px}
h1{font-size:20px;margin:0 0 18px;display:flex;align-items:center;gap:8px}
.tabs{display:flex;gap:4px;background:#eef1ef;border-radius:10px;padding:4px;margin-bottom:20px}
.tabs button{flex:1;border:0;background:transparent;padding:8px;border-radius:7px;font:inherit;cursor:pointer;color:#516056}
.tabs button.active{background:#fff;color:#1f2b22;box-shadow:0 1px 3px rgba(0,0,0,.1);font-weight:600}
.panel{display:none}
.panel.active{display:block}
label{display:block;font-size:13px;margin:12px 0 5px;color:#516056}
input{width:100%;padding:10px;border:1px solid #d3dad5;border-radius:8px;font:inherit;background:#fafbfa}
input:focus{outline:0;border-color:#2e9e5b}
button.primary{width:100%;margin-top:18px;padding:11px;border:0;border-radius:9px;background:#2e9e5b;color:#fff;font:inherit;font-weight:600;cursor:pointer}
button.primary:active{background:#27814b}
button:disabled{opacity:.5;cursor:default}
.sensor{border:1px solid #e3e8e4;border-radius:10px;padding:14px;margin-bottom:14px}
.sensor h3{margin:0 0 2px;font-size:15px}
.sensor .type{font-size:12px;color:#8a978d;margin-bottom:6px}
.step{font-size:13px;margin-top:12px}
.unitrow{display:flex;align-items:center;gap:8px;margin-top:4px}
.unitrow input{flex:1}
.unitrow button{width:auto;margin-top:0}
.unit{font-size:13px;color:#516056}
.captured{display:block;font-size:12px;color:#2e9e5b;margin-top:4px}
.msg{font-size:13px;margin-top:12px;min-height:16px}
.ok{color:#2e9e5b}.err{color:#c0392b}
.muted{color:#8a978d;font-size:13px}
.statusGrid{display:grid;grid-template-columns:1fr auto;gap:6px 12px;font-size:13px;margin-bottom:16px}
.statusGrid div:nth-child(odd){color:#516056}
.statusGrid div:nth-child(even){font-weight:600;text-align:right}
.live{font-weight:600;color:#1f2b22}
.current{font-size:12px;color:#8a978d;margin-top:2px}
.sectionTitle{font-size:11px;text-transform:uppercase;letter-spacing:.05em;color:#8a978d;margin:18px 0 8px;font-weight:600}
.sectionTitle:first-of-type{margin-top:4px}
.headerRefresh{margin-left:auto;padding:4px 10px;border:1px solid #d3dad5;border-radius:8px;background:#fff;color:#516056;font:inherit;font-size:16px;line-height:1;cursor:pointer}
.setupBanner{background:#fff7e6;border:1px solid #f0c36d;border-radius:10px;padding:14px;margin-bottom:18px}
.setupBannerTitle{font-weight:600;margin-bottom:6px}
</style></head><body>
<div class="card">
  <h1>&#127807; <span id="headerName">SensorNode</span><button class="headerRefresh" onclick="manualRefresh()" title="Refresh">&#8635;</button></h1>
  <div class="tabs">
    <button id="t0" class="active" onclick="tab(0)">Status</button>
    <button id="t1" onclick="tab(1)">Password</button>
    <button id="t2" onclick="tab(2)">WiFi</button>
    <button id="t3" onclick="tab(3)">Sensors</button>
  </div>

  <div id="p0" class="panel active">
    <div id="setupBanner" class="setupBanner" style="display:none">
      <div class="setupBannerTitle">Setup not finished</div>
      <div class="muted" id="setupBannerText"></div>
      <button class="primary" id="finishBtn" onclick="finish()" style="display:none">Finish &amp; Reboot</button>
      <div id="finMsg" class="msg"></div>
    </div>
    <label>Device name (max. 31 characters)</label>
    <div class="unitrow">
      <input id="devname" autocomplete="off" maxlength="31">
      <button class="primary" onclick="saveDeviceName()">Save</button>
    </div>
    <div id="nameMsg" class="msg"></div>
    <div class="sectionTitle">System Info</div>
    <div id="statusGrid" class="statusGrid"><div class="muted">Loading&hellip;</div></div>
  </div>

  <div id="p1" class="panel">
    <p class="muted">Set a device password — required before setup can be finished. Also used as the AP's WiFi password.</p>
    <label>New password (8-63 characters)</label>
    <input id="newpw" type="password" autocomplete="new-password">
    <label>Confirm password</label>
    <input id="newpw2" type="password" autocomplete="new-password">
    <button class="primary" onclick="savePassword()">Set password</button>
    <div id="pwMsg" class="msg"></div>
  </div>

  <div id="p2" class="panel">
    <p class="muted">Also usable while running normally, e.g. to move this node to a different MainUnit.</p>
    <div id="wifiStatusGrid" class="statusGrid"><div class="muted">Loading&hellip;</div></div>
    <label>Network name (SSID)</label>
    <input id="ssid" autocomplete="off">
    <label>Password</label>
    <input id="pw" type="password">
    <button class="primary" onclick="saveWifi()">Test &amp; Save</button>
    <div id="wifiMsg" class="msg"></div>
  </div>

  <div id="p3" class="panel">
    <div id="sensors"><div class="muted">Loading sensors&hellip;</div></div>
  </div>

</div>
<script>
// 6s instead of 2s: several simultaneously open browsers/tabs shouldn't keep
// the C3 permanently busy (every poll is a wake cycle, see Light Sleep in
// CLAUDE.md). The manual refresh button in the header covers the "see it
// right now" case, visibility pause stops polling entirely while the
// browser tab is in the background.
var POLL_MS=6000;
var pollTimer=null;
var currentTab=0;
function stopPoll(){if(pollTimer){clearInterval(pollTimer);pollTimer=null;}}
function pollOnce(n){
  if(n==0){loadStatus();}
  else if(n==2){loadWifiStatus();}
  else if(n==3){refreshLiveValues();}
}
function startPollIfNeeded(){
  stopPoll();
  if(document.hidden) return;
  if(currentTab==0||currentTab==2||currentTab==3) pollTimer=setInterval(function(){pollOnce(currentTab);},POLL_MS);
}
function tab(n){
  currentTab=n;
  for(var i=0;i<4;i++){document.getElementById('t'+i).classList.toggle('active',i==n);document.getElementById('p'+i).classList.toggle('active',i==n);}
  pollOnce(n);
  startPollIfNeeded();
}
function manualRefresh(){ pollOnce(currentTab); }
document.addEventListener('visibilitychange', function(){
  if(document.hidden) stopPoll();
  else { pollOnce(currentTab); startPollIfNeeded(); }
});
function msg(el,t,ok){el.textContent=t;el.className='msg '+(ok?'ok':'err');}

async function loadStatus(){
  var g=document.getElementById('statusGrid');
  try{
    var d=await (await fetch('/status')).json();
    var hn=document.getElementById('headerName');
    if(hn) hn.textContent=d.device_name;
    var nameInput=document.getElementById('devname');
    if(nameInput && document.activeElement!==nameInput) nameInput.value=d.device_name;
    var banner=document.getElementById('setupBanner');
    var bannerText=document.getElementById('setupBannerText');
    var finishBtn=document.getElementById('finishBtn');
    if(!d.provisioned){
      banner.style.display='block';
      if(d.password_set){
        bannerText.textContent='Ready to go — finish setup to reboot into normal operation.';
        finishBtn.style.display='block';
      }else{
        bannerText.textContent='Set a device password first (Password tab), then come back here to finish setup.';
        finishBtn.style.display='none';
      }
    }else{
      banner.style.display='none';
    }
    g.innerHTML=
      '<div>Uptime</div><div>'+Math.floor(d.uptime/1000)+' s</div>'+
      '<div>Chip-Temp</div><div>'+d.chip_temp.toFixed(1)+' &deg;C</div>'+
      '<div>Free Heap</div><div>'+d.free_heap+' B</div>'+
      '<div>Version</div><div>'+d.version+'</div>';
  }catch(e){g.innerHTML='<div class="err">Status not reachable</div>';}
}

async function loadWifiStatus(){
  var g=document.getElementById('wifiStatusGrid');
  try{
    var d=await (await fetch('/status')).json();
    g.innerHTML=
      '<div>Connected to</div><div>'+(d.ssid?d.ssid:'&ndash; (not connected)')+'</div>'+
      '<div>RSSI</div><div>'+d.rssi+' dBm</div>';
  }catch(e){g.innerHTML='<div class="err">Status not reachable</div>';}
}

async function saveDeviceName(){
  var name=document.getElementById('devname').value;
  var m=document.getElementById('nameMsg');
  if(!name){msg(m,'Name required',false);return;}
  try{
    var r=await fetch('/provision/name',{method:'POST',body:JSON.stringify({name:name})});
    msg(m,r.ok?'Saved ✓':'Error ('+r.status+')',r.ok);
  }catch(e){msg(m,'Connection failed',false);}
}

async function savePassword(){
  var pw=document.getElementById('newpw').value,pw2=document.getElementById('newpw2').value;
  var m=document.getElementById('pwMsg');
  if(!pw||pw.length<8||pw.length>63){msg(m,'Must be 8-63 characters',false);return;}
  if(pw!==pw2){msg(m,'Passwords do not match',false);return;}
  try{
    var r=await fetch('/provision/password',{method:'POST',body:JSON.stringify({password:pw})});
    msg(m,r.ok?'Password set ✓ — reconnect your WiFi with the new password, then continue here':'Error ('+r.status+')',r.ok);
  }catch(e){msg(m,'Connection lost — reconnect your WiFi with the new password now, then continue here',true);}
}

async function saveWifi(){
  var ssid=document.getElementById('ssid').value,pw=document.getElementById('pw').value;
  var m=document.getElementById('wifiMsg');
  if(!ssid||!pw){msg(m,'SSID and password required',false);return;}
  msg(m,'Testing connection… (up to 5s)',true);
  try{
    var r=await fetch('/provision/wifi',{method:'POST',body:JSON.stringify({ssid:ssid,password:pw})});
    if(r.ok){
      var d=await r.json();
      msg(m,d.connected?'Connected ✓':'Connection failed ✗ — check credentials',d.connected);
    }else{
      msg(m,'Error ('+r.status+')',false);
    }
  }catch(e){msg(m,'Connection interrupted — the node may already have switched networks, check the Status tab',false);}
}

var collected={};
var sensorUnits={}; // cached from /calibrationinfo — /sensors doesn't include the unit on every poll
var expandedSensors={}; // remembers open/closed state across loadCalibrationInfo() rebuilds
async function startCalibration(idx){
  // One-shot action, no longer a toggle: reset + expand. The button doesn't
  // render again afterward (see loadCalibrationInfo) — nothing left to
  // cancel, the reset has already happened.
  var ok=false;
  try{ var r=await fetch('/reset/'+idx,{method:'POST'}); ok=r.ok; }catch(e){}
  expandedSensors[idx]=true;
  await loadCalibrationInfo();
  var m=document.getElementById('sMsg_'+idx); // fetch only AFTER the rebuild, otherwise stale
  if(m) msg(m, ok?'Reset ✓ — sensor now at raw values':'Reset failed', ok);
  refreshLiveValues();
}
async function loadCalibrationInfo(){
  var box=document.getElementById('sensors');
  var info;
  try{info=await (await fetch('/calibrationinfo')).json();}
  catch(e){box.innerHTML='<div class="err">Could not load sensor info</div>';return;}
  box.innerHTML='';
  info.forEach(function(s){
    collected[s.index]={};
    sensorUnits[s.index]=s.unit||'';

    var el=document.createElement('div');el.className='sensor';el.id='sensorcard_'+s.index;
    var h='<h3>Sensor '+s.index+'</h3><div class="type">'+s.type+'</div>';
    h+='<div>Live: <span class="live" id="live_calib_'+s.index+'">&ndash;</span></div>';
    if(!s.steps||!s.steps.length){
      h+='<div class="muted">No calibration needed</div>';
    }else{
      var cur=s.current||{};
      if(Object.keys(cur).length){
        h+='<div class="current">current: '+Object.keys(cur).map(function(k){return k+'='+cur[k];}).join(', ')+'</div>';
      }
      var open=!!expandedSensors[s.index];
      if(!open){
        h+='<button class="primary" onclick="startCalibration('+s.index+')">Reset and Calibrate</button>';
      }
      h+='<div id="calbody_'+s.index+'" style="display:'+(open?'block':'none')+'">';
      s.steps.forEach(function(st,stepIdx){
        h+='<div class="step">'+(stepIdx+1)+'. '+st.instruction;
        if(st.ref){
          h+='<div class="unitrow"><input type="number" id="in_'+s.index+'_'+st.key+'" placeholder="'+st.key+'">';
          if(st.unit) h+='<span class="unit">'+st.unit+'</span>';
          h+='</div>';
        }else{
          h+='<button class="primary" onclick="capture('+s.index+',\''+st.key+'\',this)">Capture raw value</button><span class="captured" id="cap_'+s.index+'_'+st.key+'"></span>';
        }
        h+='</div>';
      });
      h+='<button class="primary" onclick="calibrate('+s.index+')">Calibrate</button><div class="msg" id="sMsg_'+s.index+'"></div>';
      h+='</div>';
    }
    el.innerHTML=h;box.appendChild(el);
  });
}

function setLive(idx,text){
  var el=document.getElementById('live_calib_'+idx);
  if(el) el.textContent=text;
}
async function refreshLiveValues(){
  try{
    var d=await (await fetch('/sensors')).json();
    Object.keys(d).forEach(function(k){
      var idx=k.split(':')[1];
      var s=d[k];
      var unit=sensorUnits[idx];
      setLive(idx, s.valid ? (s.value+(unit?' '+unit:'')) : '–');
    });
  }catch(e){}
}

async function capture(idx,key,btn){
  var cap=document.getElementById('cap_'+idx+'_'+key);
  btn.disabled=true;cap.textContent=' …';
  try{
    var resetR=await fetch('/reset/'+idx,{method:'POST'});      // guarantee raw ADC
    if(!resetR.ok) throw new Error('reset failed');
    var d=await (await fetch('/sensors')).json();
    var v=d['sensor:'+idx].value;
    collected[idx][key]=v;
    cap.textContent=' = '+v;
  }catch(e){cap.textContent=' Error';cap.className='captured err';}
  btn.disabled=false;
  refreshLiveValues();
}

async function calibrate(idx){
  var m=document.getElementById('sMsg_'+idx);
  var body=Object.assign({},collected[idx]);
  document.querySelectorAll('[id^="in_'+idx+'_"]').forEach(function(inp){
    body[inp.id.split('_').slice(2).join('_')]=parseFloat(inp.value);
  });
  try{
    var r=await fetch('/calibrate/'+idx,{method:'POST',body:JSON.stringify(body)});
    if(r.ok){
      // Success: put the card back into its original state (collapsed)
      // instead of leaving it open — the new values are visible in the
      // Live/current value anyway.
      expandedSensors[idx]=false;
      await loadCalibrationInfo(); // reload the "current" read-back, card collapses
    }else{
      msg(m,'Error – check values',false); // card stays open so the inputs can be corrected
    }
  }catch(e){msg(m,'Connection failed',false);}
  refreshLiveValues();
}

async function finish(){
  var m=document.getElementById('finMsg');
  msg(m,'Rebooting…',true);
  try{await fetch('/provision/finish',{method:'POST'});}catch(e){}
  msg(m,'Reboot in progress – you can close this page.',true);
}

loadCalibrationInfo().then(function(){tab(0);});
</script></body></html>)html";
