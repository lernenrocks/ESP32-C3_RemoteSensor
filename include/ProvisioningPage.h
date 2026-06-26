#pragma once

// Provisioning-Webseite, vollstaendig inline (HTML + CSS + JS, kein CDN — im
// AP-Modus gibt es kein Internet). Liegt als const char[] im memory-mapped
// Flash des ESP32 -> client.print() streamt es direkt, kein Heap, kein String.
//
// Datengetrieben: Das JS baut die Sensor-Tabs aus GET /calibrationinfo auf,
// damit hier kein Sensor-Wissen hartcodiert ist (gleiches Prinzip wie die
// Companion App). Neue Sensortypen brauchen keine Aenderung an dieser Datei.
//
// Auth-Annahme: Provisioning laeuft unauthentifiziert im isolierten AP-Modus.
// Wenn spaeter Digest Auth dazukommt, bleiben diese Endpoints ausgenommen.
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
button.ghost{margin-top:8px;padding:9px 12px;border:1px solid #2e9e5b;border-radius:8px;background:#fff;color:#2e9e5b;font:inherit;cursor:pointer}
button:disabled{opacity:.5;cursor:default}
.sensor{border:1px solid #e3e8e4;border-radius:10px;padding:14px;margin-bottom:14px}
.sensor h3{margin:0 0 2px;font-size:15px}
.sensor .type{font-size:12px;color:#8a978d;margin-bottom:6px}
.step{font-size:13px;margin-top:12px}
.unitrow{display:flex;align-items:center;gap:8px;margin-top:4px}
.unitrow input{flex:1}
.unit{font-size:13px;color:#516056}
.captured{font-size:12px;color:#2e9e5b;margin-left:6px}
.msg{font-size:13px;margin-top:12px;min-height:16px}
.ok{color:#2e9e5b}.err{color:#c0392b}
.muted{color:#8a978d;font-size:13px}
</style></head><body>
<div class="card">
  <h1>&#127807; SensorNode Setup</h1>
  <div class="tabs">
    <button id="t0" class="active" onclick="tab(0)">WiFi</button>
    <button id="t1" onclick="tab(1)">Sensors</button>
    <button id="t2" onclick="tab(2)">Finish</button>
  </div>

  <div id="p0" class="panel active">
    <label>Network name (SSID)</label>
    <input id="ssid" autocomplete="off">
    <label>Password</label>
    <input id="pw" type="password">
    <button class="primary" onclick="saveWifi()">Save</button>
    <div id="wifiMsg" class="msg"></div>
  </div>

  <div id="p1" class="panel">
    <div id="sensors"><div class="muted">Loading sensors&hellip;</div></div>
  </div>

  <div id="p2" class="panel">
    <p class="muted">WiFi saved and sensors calibrated? Then finish setup
    &mdash; the SensorNode reboots and connects to the network.</p>
    <button class="primary" onclick="finish()">Finish &amp; Reboot</button>
    <div id="finMsg" class="msg"></div>
  </div>
</div>
<script>
function tab(n){for(var i=0;i<3;i++){document.getElementById('t'+i).classList.toggle('active',i==n);document.getElementById('p'+i).classList.toggle('active',i==n);}}
function msg(el,t,ok){el.textContent=t;el.className='msg '+(ok?'ok':'err');}

async function saveWifi(){
  var ssid=document.getElementById('ssid').value,pw=document.getElementById('pw').value;
  var m=document.getElementById('wifiMsg');
  if(!ssid||!pw){msg(m,'SSID and password required',false);return;}
  try{
    var r=await fetch('/provision/wifi',{method:'POST',body:JSON.stringify({ssid:ssid,password:pw})});
    msg(m,r.ok?'Saved ✓':'Error ('+r.status+')',r.ok);
  }catch(e){msg(m,'Connection failed',false);}
}

var collected={};
async function loadSensors(){
  var box=document.getElementById('sensors');
  var info;
  try{info=await (await fetch('/calibrationinfo')).json();}
  catch(e){box.innerHTML='<div class="err">Could not load sensor info</div>';return;}
  box.innerHTML='';
  info.forEach(function(s){
    collected[s.index]={};
    var el=document.createElement('div');el.className='sensor';
    var h='<h3>Sensor '+s.index+'</h3><div class="type">'+s.type+'</div>';
    if(!s.steps||!s.steps.length){
      el.innerHTML=h+'<div class="muted">No calibration needed</div>';
      box.appendChild(el);return;
    }
    s.steps.forEach(function(st){
      h+='<div class="step">'+st.instruction;
      if(st.ref){
        h+='<div class="unitrow"><input type="number" id="in_'+s.index+'_'+st.key+'" placeholder="'+st.key+'">';
        if(st.unit) h+='<span class="unit">'+st.unit+'</span>';
        h+='</div>';
      }else{
        h+='<br><button class="ghost" onclick="capture('+s.index+',\''+st.key+'\',this)">Capture raw value</button><span class="captured" id="cap_'+s.index+'_'+st.key+'"></span>';
      }
      h+='</div>';
    });
    h+='<button class="primary" onclick="calibrate('+s.index+')">Calibrate</button><div class="msg" id="sMsg_'+s.index+'"></div>';
    el.innerHTML=h;box.appendChild(el);
  });
}

async function capture(idx,key,btn){
  var cap=document.getElementById('cap_'+idx+'_'+key);
  btn.disabled=true;cap.textContent=' …';
  try{
    await fetch('/reset/'+idx,{method:'POST'});      // Raw-ADC garantieren
    var d=await (await fetch('/sensors')).json();
    var v=d['sensor:'+idx].value;
    collected[idx][key]=v;
    cap.textContent=' = '+v;
  }catch(e){cap.textContent=' Error';cap.className='captured err';}
  btn.disabled=false;
}

async function calibrate(idx){
  var m=document.getElementById('sMsg_'+idx);
  var body=Object.assign({},collected[idx]);
  document.querySelectorAll('[id^="in_'+idx+'_"]').forEach(function(inp){
    body[inp.id.split('_').slice(2).join('_')]=parseFloat(inp.value);
  });
  try{
    var r=await fetch('/calibrate/'+idx,{method:'POST',body:JSON.stringify(body)});
    msg(m,r.ok?'Calibrated ✓':'Error – check values',r.ok);
  }catch(e){msg(m,'Connection failed',false);}
}

async function finish(){
  var m=document.getElementById('finMsg');
  msg(m,'Rebooting…',true);
  try{await fetch('/provision/finish',{method:'POST'});}catch(e){}
  msg(m,'Reboot in progress – you can close this page.',true);
}

loadSensors();
</script></body></html>)html";
