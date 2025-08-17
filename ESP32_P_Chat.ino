/************ ESP32 CHAT HUB (AP + Web UI + WebSocket + Peer TCP) ************/
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>

#define AP_SSID      "ESPChat"
#define AP_PASS      "12345678"    // set 8+ chars; or "" for open
#define AP_CHANNEL   1
#define MAX_HISTORY  50

// Peer link for ESP8266
WiFiServer peerServer(5000);
WiFiClient peerClient;

WebServer http(80);
WebSocketsServer ws(81);

struct Msg {
  String from;
  String text;
  uint32_t ts;
};
Msg history[MAX_HISTORY];
int histCount = 0;

// ------------- Inline UI (single page app) -------------
const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<title>ESP Chat</title>
<style>
  :root{--bg:#0f172a;--card:#111827;--muted:#94a3b8;--me:#2563eb;--them:#374151;--accent:#22c55e}
  *{box-sizing:border-box}
  body{margin:0;font-family:system-ui,-apple-system,Segoe UI,Roboto,Ubuntu,Helvetica,Arial,sans-serif;background:linear-gradient(180deg,#0b1220,#0f172a);}
  .wrap{max-width:900px;margin:0 auto;min-height:100vh;display:flex;flex-direction:column;padding:16px}
  .nav{display:flex;align-items:center;gap:12px;color:#e5e7eb}
  .logo{width:42px;height:42px;border-radius:14px;background:linear-gradient(135deg,var(--me),#9333ea);display:grid;place-items:center;color:white;font-weight:800}
  .title{font-size:20px;font-weight:700}
  .status{margin-left:auto;font-size:12px;color:var(--muted)}
  .card{margin-top:12px;background:rgba(17,24,39,.7);backdrop-filter:blur(6px);border:1px solid rgba(255,255,255,.07);border-radius:18px;display:flex;flex-direction:column;min-height:65vh}
  .msgs{flex:1;overflow:auto;padding:16px;scroll-behavior:smooth}
  .bubble{max-width:75%;margin:8px 0;padding:10px 12px;border-radius:16px;line-height:1.3;color:#e5e7eb;word-wrap:break-word;white-space:pre-wrap}
  .me{background:var(--me);margin-left:auto;border-bottom-right-radius:6px}
  .them{background:var(--them);margin-right:auto;border-bottom-left-radius:6px}
  .meta{font-size:11px;color:#cbd5e1;margin-top:4px;opacity:.8}
  .inputbar{display:flex;gap:8px;padding:10px;border-top:1px solid rgba(255,255,255,.07);background:rgba(2,6,23,.6);border-radius:0 0 18px 18px}
  input,button{border:0;outline:0}
  .name{width:140px;border-radius:12px;padding:10px;background:#0b1220;color:#e5e7eb;border:1px solid #1f2937}
  .text{flex:1;border-radius:12px;padding:12px;background:#0b1220;color:#e5e7eb;border:1px solid #1f2937}
  .send{border-radius:12px;padding:12px 16px;background:var(--accent);color:#062312;font-weight:700;cursor:pointer}
  .pill{display:inline-block;padding:6px 10px;border-radius:999px;background:#0b1220;border:1px solid #1f2937;color:#cbd5e1;font-size:12px}
  .tip{margin-left:8px;color:#94a3b8;font-size:12px}
</style>
</head>
<body>
  <div class="wrap">
    <div class="nav">
      <div class="logo">ESP</div>
      <div class="title">Offline Chat</div>
      <span class="pill">AP: ESPChat · 192.168.4.1</span>
      <span class="status" id="status">connecting…</span>
    </div>
    <div class="card">
      <div id="msgs" class="msgs"></div>
      <div class="inputbar">
        <input id="name" class="name" placeholder="Your name" maxlength="20">
        <input id="text" class="text" placeholder="Type a message and press Enter">
        <button id="send" class="send">Send</button>
      </div>
    </div>
    <div class="tip">Connect your phone/laptop to <b>ESPChat</b> Wi-Fi (password: 12345678) then open <b>http://192.168.4.1/</b>.</div>
  </div>
<script>
(() => {
  const elMsgs = document.getElementById('msgs');
  const elName = document.getElementById('name');
  const elText = document.getElementById('text');
  const elSend = document.getElementById('send');
  const status = document.getElementById('status');

  let ws;
  let reconnectTimer;

  function appendBubble(from, text, ts){
    const wrap = document.createElement('div');
    const me = (from === (elName.value || 'Me'));
    wrap.className = 'bubble ' + (me ? 'me' : 'them');
    wrap.textContent = text;
    const meta = document.createElement('div');
    meta.className = 'meta';
    const time = new Date(ts || Date.now()).toLocaleTimeString();
    meta.textContent = from + ' · ' + time;
    wrap.appendChild(meta);
    elMsgs.appendChild(wrap);
    elMsgs.scrollTop = elMsgs.scrollHeight;
  }

  function connect(){
    ws = new WebSocket('ws://' + location.host + ':81');
    ws.onopen = () => { status.textContent = 'online'; };
    ws.onclose = () => {
      status.textContent = 'reconnecting…';
      clearTimeout(reconnectTimer);
      reconnectTimer = setTimeout(connect, 1000);
    };
    ws.onmessage = (ev) => {
      try {
        const msg = JSON.parse(ev.data);
        if(Array.isArray(msg)){ // history
          msg.forEach(m => appendBubble(m.from, m.text, m.ts));
        } else {
          appendBubble(msg.from, msg.text, msg.ts);
        }
      } catch(e){ /* ignore */ }
    };
  }

  function send(){
    const text = elText.value.trim();
    const from = (elName.value.trim() || 'Me').slice(0, 20);
    if(!text) return;
    const payload = JSON.stringify({from, text, ts: Date.now()});
    if(ws && ws.readyState === 1) ws.send(payload);
    elText.value = '';
  }

  elSend.onclick = send;
  elText.addEventListener('keydown', e => { if(e.key === 'Enter') send(); });

  connect();
})();
</script>
</body>
</html>
)HTML";

// ---------------- Utility ----------------
String msgToJson(const Msg& m){
  String s = "{\"from\":\"";
  String f = m.from; f.replace("\"","\\\"");
  String t = m.text; t.replace("\"","\\\"");
  s += f; s += "\",\"text\":\""; s += t; s += "\",\"ts\":";
  s += String(m.ts); s += "}";
  return s;
}

void pushHistory(const Msg& m){
  if(histCount < MAX_HISTORY){
    history[histCount++] = m;
  } else {
    // simple ring buffer
    for(int i=1;i<MAX_HISTORY;i++) history[i-1] = history[i];
    history[MAX_HISTORY-1] = m;
  }
}

void broadcastWS(const Msg& m){
  String json = msgToJson(m);
  ws.broadcastTXT(json);
}

void sendHistory(uint8_t num){
  // pack as JSON array for efficiency
  String arr = "[";
  for(int i=0;i<histCount;i++){
    if(i) arr += ",";
    arr += msgToJson(history[i]);
  }
  arr += "]";
  ws.sendTXT(num, arr);
}

// ---------------- WebSocket events ----------------
void onWsEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t len){
  switch(type){
    case WStype_CONNECTED:
      sendHistory(num);
      break;
    case WStype_TEXT: {
      // Browser sent a message (JSON)
      String s = String((const char*)payload, len);
      // Tiny parse (assumes {"from":"...","text":"...","ts":...})
      Msg m;
      int i1 = s.indexOf("\"from\":\""); int i2 = s.indexOf("\"", i1+8);
      m.from = (i1>=0 && i2>i1) ? s.substring(i1+8, i2) : "User";
      int j1 = s.indexOf("\"text\":\""); int j2 = s.indexOf("\"", j1+8);
      m.text = (j1>=0 && j2>j1) ? s.substring(j1+8, j2) : s;
      int k1 = s.indexOf("\"ts\":"); int k2 = s.indexOf("}", k1);
      m.ts = (k1>=0) ? (uint32_t) strtoul(s.substring(k1+5, k2).c_str(), NULL, 10) : millis();

      pushHistory(m);
      broadcastWS(m);

      // Forward to ESP8266 peer if connected (newline-terminated)
      if(peerClient && peerClient.connected()){
        String line = m.from + "|" + m.text + "|" + String(m.ts) + "\n";
        peerClient.print(line);
      }
      break;
    }
    default: break;
  }
}

// ---------------- HTTP ----------------
void handleRoot(){
  http.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

void handleNotFound(){
  http.send(404, "text/plain", "Not found");
}

// ---------------- Setup/Loop ----------------
void setup(){
  Serial.begin(115200);
  delay(300);

  WiFi.mode(WIFI_AP);
  bool ok = WiFi.softAP(AP_SSID, AP_PASS, AP_CHANNEL, false, 8);
  IPAddress ip = WiFi.softAPIP();
  Serial.println();
  Serial.println("AP started: " + String(AP_SSID));
  Serial.print("AP IP: "); Serial.println(ip);
  Serial.println("Open http://" + ip.toString() + " in a browser.");
  Serial.println("Peer server port: 5000");

  // HTTP
  http.on("/", handleRoot);
  http.onNotFound(handleNotFound);
  http.begin();

  // WebSocket
  ws.begin();
  ws.onEvent(onWsEvent);

  // Peer TCP server
  peerServer.begin();
  peerServer.setNoDelay(true);
}

void loop(){
  http.handleClient();
  ws.loop();

  // Accept peer connection (ESP8266)
  if(!peerClient || !peerClient.connected()){
    WiFiClient newc = peerServer.available();
    if(newc){
      Serial.println("Peer connected: " + newc.remoteIP().toString());
      peerClient.stop();
      peerClient = newc;
      // Send greeting
      peerClient.print("ESP32|peer-connected|" + String(millis()) + "\n");
    }
  } else {
    // Read peer lines and relay to browsers
    while(peerClient.available()){
      String line = peerClient.readStringUntil('\n'); // format: from|text|ts\n
      line.trim();
      if(line.length()==0) break;
      int a=line.indexOf('|'); int b=line.indexOf('|', a+1);
      Msg m;
      if(a>0 && b>a){
        m.from = line.substring(0,a);
        m.text = line.substring(a+1,b);
        m.ts   = (uint32_t) strtoul(line.substring(b+1).c_str(), NULL, 10);
      } else {
        m.from = "ESP8266";
        m.text = line;
        m.ts   = millis();
      }
      pushHistory(m);
      broadcastWS(m);
      Serial.println("Peer->WS: " + m.from + ": " + m.text);
    }
  }
}
