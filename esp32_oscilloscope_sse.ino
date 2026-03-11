// ============================================================
//  ESP32 WROOM-32 - Web Oscilloscope
//  Features: Real-time waveform, Auto-scale, Screenshot,
//            Dark/Light theme, Mobile + Desktop UI
//  Mode: WiFi Access Point (AP)
//  Libraries: Built-in only (WiFi.h, WebServer.h) - no external libs!
// ============================================================

#include <WiFi.h>
#include <WebServer.h>

// ============================================================
//  SECTION 1: CONFIGURATION
// ============================================================
#define ADC_PIN        34      // Input signal pin (0-3.3V)
#define SAMPLE_RATE_HZ 10000     // Samples per second sent to browser
#define BATCH_SIZE     100      // Samples grouped per SSE event

const char* AP_SSID     = "ESP32_Oscilloscope";
const char* AP_PASSWORD = "12345678";

// ============================================================
//  SECTION 2: GLOBALS
// ============================================================
WebServer server(80);

WiFiClient sseClient;
bool       sseClientConnected = false;

unsigned long lastSampleUs = 0;
const unsigned long sampleIntervalUs = 1000000UL / SAMPLE_RATE_HZ;

uint16_t batchBuf[BATCH_SIZE];
int      batchIdx = 0;

// ============================================================
//  SECTION 3: HTML PAGE (in flash)
// ============================================================
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0">
<title>ESP32 Oscilloscope</title>
<style>
  :root {
    --bg:         #0a0a0f;
    --surface:    #111118;
    --border:     #1e1e2e;
    --grid:       rgba(0,255,180,0.12);
    --center:     rgba(255,60,60,0.6);
    --wave:       #00ffe0;
    --accent:     #00ffe0;
    --text:       #c0ffd8;
    --subtext:    #5a8a70;
    --btn-bg:     rgba(0,255,180,0.08);
    --btn-border: rgba(0,255,180,0.35);
    --shadow:     0 0 20px rgba(0,255,180,0.15);
  }
  .light {
    --bg:         #f0f4f0;
    --surface:    #ffffff;
    --border:     #c8dcd0;
    --grid:       rgba(0,120,80,0.12);
    --center:     rgba(200,0,0,0.5);
    --wave:       #007a50;
    --accent:     #007a50;
    --text:       #1a3a2a;
    --subtext:    #5a8a70;
    --btn-bg:     rgba(0,120,80,0.08);
    --btn-border: rgba(0,120,80,0.4);
    --shadow:     0 0 20px rgba(0,120,80,0.1);
  }
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body {
    background: var(--bg); color: var(--text);
    font-family: 'Courier New', monospace;
    min-height: 100vh; display: flex;
    flex-direction: column; align-items: center;
    padding: 10px; transition: background 0.3s, color 0.3s;
  }
  #header {
    width: 100%; max-width: 900px;
    display: flex; justify-content: space-between; align-items: center;
    padding: 8px 4px; margin-bottom: 8px;
  }
  #header h1 {
    font-size: clamp(13px, 3vw, 19px);
    letter-spacing: 0.15em; color: var(--accent);
    text-shadow: 0 0 12px var(--accent);
  }
  #header-right { display: flex; gap: 8px; align-items: center; }
  #statusDot {
    width: 9px; height: 9px; border-radius: 50%;
    background: #ff4444; box-shadow: 0 0 6px #ff4444;
    transition: background 0.3s, box-shadow 0.3s;
  }
  #statusDot.on { background: var(--accent); box-shadow: 0 0 8px var(--accent); }
  #scopeWrapper { width: 100%; max-width: 900px; }
  #scope {
    width: 100%; height: auto; display: block;
    border: 1px solid var(--border); border-radius: 10px;
    background: var(--surface); box-shadow: var(--shadow);
  }
  #bottomBar {
    width: 100%; max-width: 900px;
    display: flex; flex-wrap: wrap; gap: 10px;
    margin-top: 10px; justify-content: space-between;
  }
  #controls {
    background: var(--surface); border: 1px solid var(--border);
    border-radius: 12px; padding: 10px 14px;
    display: flex; flex-wrap: wrap; gap: 10px; align-items: center;
    box-shadow: var(--shadow); flex: 1; min-width: 260px;
  }
  .ctrl-group { display: flex; align-items: center; gap: 6px; }
  .ctrl-label { font-size: 11px; color: var(--subtext); min-width: 85px; text-align: center; }
  button {
    background: var(--btn-bg); border: 1px solid var(--btn-border);
    color: var(--accent); padding: 5px 11px; border-radius: 7px;
    font-size: 13px; font-family: 'Courier New', monospace;
    cursor: pointer; transition: background 0.15s, box-shadow 0.15s;
    user-select: none;
  }
  button:hover  { background: rgba(0,255,180,0.18); box-shadow: 0 0 8px var(--accent); }
  button:active { background: rgba(0,255,180,0.28); }
  .light button:hover { box-shadow: 0 0 8px rgba(0,120,80,0.4); }
  #info {
    background: var(--surface); border: 1px solid var(--border);
    border-radius: 12px; padding: 10px 16px; font-size: 13px;
    display: grid; grid-template-columns: 1fr 1fr;
    gap: 4px 18px; min-width: 200px; box-shadow: var(--shadow);
  }
  .stat-key { color: var(--subtext); }
  .stat-val  { color: var(--text); font-weight: bold; }
  #actions {
    width: 100%; max-width: 900px;
    display: flex; gap: 8px; margin-top: 8px; flex-wrap: wrap;
  }
  #actions button { font-size: 12px; padding: 5px 14px; }
</style>
</head>
<body>

<div id="header">
  <h1>~ ESP32 OSCILLOSCOPE</h1>
  <div id="header-right">
    <div id="statusDot"></div>
    <button id="themeBtn">LIGHT</button>
  </div>
</div>

<div id="scopeWrapper">
  <canvas id="scope" width="900" height="400"></canvas>
</div>

<div id="bottomBar">
  <div id="controls">
    <div class="ctrl-group">
      <button id="vMinus">-</button>
      <span class="ctrl-label" id="scaleLabel">1.00 V/div</span>
      <button id="vPlus">+</button>
    </div>
    <div class="ctrl-group">
      <button id="tMinus">-</button>
      <span class="ctrl-label" id="timeLabel">1.00 ms/div</span>
      <button id="tPlus">+</button>
    </div>
    <div class="ctrl-group">
      <button id="pauseBtn">|| Pause</button>
    </div>
    <div class="ctrl-group">
      <button id="autoBtn">Auto</button>
    </div>
  </div>

  <div id="info">
    <span class="stat-key">Frequency</span><span class="stat-val"><span id="freq">-</span> Hz</span>
    <span class="stat-key">Vpp</span>       <span class="stat-val"><span id="vpp">-</span></span>
    <span class="stat-key">Vmin</span>      <span class="stat-val"><span id="vmin">-</span></span>
    <span class="stat-key">Vmax</span>      <span class="stat-val"><span id="vmax">-</span></span>
    <span class="stat-key">Vrms</span>      <span class="stat-val"><span id="vrms">-</span></span>
    <span class="stat-key">Duty</span>      <span class="stat-val"><span id="duty">-</span> %</span>
  </div>
</div>

<div id="actions">
  <button id="screenshotBtn">[ Save Screenshot ]</button>
</div>

<script>
const VREF        = 3.3;
const ADC_MAX     = 4095;
const MAX_POINTS  = 600;
const SAMPLE_RATE = 10000;

const data   = new Float32Array(MAX_POINTS);
let writeIdx = 0;
let paused   = false;
let lightMode= false;
let autoScale= false;
let voltsPerDiv      = 1.0;
let timePerDiv       = 1.0;
let displayFullScale = 2.0;

const canvas = document.getElementById('scope');
const ctx    = canvas.getContext('2d');
let cw = 900, ch = 400;

function resizeCanvas() {
  const rect = canvas.getBoundingClientRect();
  const dpr  = window.devicePixelRatio || 1;
  canvas.width  = rect.width  * dpr;
  canvas.height = rect.height * dpr;
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  cw = rect.width;
  ch = rect.height;
  draw();
}
window.addEventListener('resize', resizeCanvas);
setTimeout(resizeCanvas, 100);

function adcToVolt(v) { return (v / ADC_MAX) * VREF; }

function addSamples(values) {
  if (paused) return;
  for (let v of values) {
    data[writeIdx] = adcToVolt(v);
    writeIdx = (writeIdx + 1) % MAX_POINTS;
  }
  if (autoScale) applyAutoScale();
  draw();
  updateStats();
}

function applyAutoScale() {
  let mn = Infinity, mx = -Infinity;
  for (let i = 0; i < MAX_POINTS; i++) {
    if (data[i] < mn) mn = data[i];
    if (data[i] > mx) mx = data[i];
  }
  const range = mx - mn;
  if (range < 0.01) return;
  voltsPerDiv      = Math.max(0.05, parseFloat((range / 6).toFixed(2)));
  displayFullScale = voltsPerDiv * 4;
  updateLabels();
}

function draw() {
  const style     = getComputedStyle(document.body);
  const gridCol   = style.getPropertyValue('--grid').trim();
  const centreCol = style.getPropertyValue('--center').trim();
  const waveCol   = style.getPropertyValue('--wave').trim();
  const textCol   = style.getPropertyValue('--subtext').trim();

  ctx.clearRect(0, 0, cw, ch);

  const H_DIVS = 10, V_DIVS = 8;
  ctx.strokeStyle = gridCol;
  ctx.lineWidth   = 1;
  for (let i = 0; i <= H_DIVS; i++) {
    const x = (i / H_DIVS) * cw;
    ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, ch); ctx.stroke();
  }
  ctx.fillStyle    = textCol;
  ctx.font         = Math.max(10, cw * 0.013) + 'px monospace';
  ctx.textAlign    = 'left';
  ctx.textBaseline = 'middle';
  for (let i = 0; i <= V_DIVS; i++) {
    const y     = (i / V_DIVS) * ch;
    const volts = displayFullScale - i * voltsPerDiv;
    ctx.strokeStyle = gridCol;
    ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(cw, y); ctx.stroke();
    ctx.fillText(volts.toFixed(2) + 'V', 4, y);
  }
  ctx.strokeStyle = centreCol;
  ctx.lineWidth   = 1.2;
  ctx.setLineDash([4, 4]);
  ctx.beginPath(); ctx.moveTo(0, ch/2); ctx.lineTo(cw, ch/2); ctx.stroke();
  ctx.beginPath(); ctx.moveTo(cw/2, 0); ctx.lineTo(cw/2, ch); ctx.stroke();
  ctx.setLineDash([]);

  ctx.beginPath();
  ctx.strokeStyle = waveCol;
  ctx.lineWidth   = 1.8;
  const step = cw / MAX_POINTS;
  for (let i = 0; i < MAX_POINTS; i++) {
    const di   = (writeIdx + i) % MAX_POINTS;
    const volt = data[di] - (VREF / 2);
    const y    = (ch / 2) - (volt / displayFullScale) * (ch / 2);
    const yc   = Math.max(0, Math.min(ch, y));
    if (i === 0) ctx.moveTo(0, yc);
    else ctx.lineTo(i * step, yc);
  }
  ctx.stroke();

  ctx.fillStyle    = textCol;
  ctx.textAlign    = 'right';
  ctx.textBaseline = 'bottom';
  ctx.fillText(timePerDiv.toFixed(2) + ' ms/div', cw - 6, ch - 4);
}

function updateStats() {
  let mn = Infinity, mx = -Infinity, sumSq = 0;
  for (let i = 0; i < MAX_POINTS; i++) {
    const v = data[i];
    if (v < mn) mn = v;
    if (v > mx) mx = v;
    sumSq += v * v;
  }
  const vrms = Math.sqrt(sumSq / MAX_POINTS);
  const mid  = (mx + mn) / 2;
  let crossings = 0;
  for (let i = 1; i < MAX_POINTS; i++) {
    const prev = data[(writeIdx + i - 1) % MAX_POINTS];
    const curr = data[(writeIdx + i)     % MAX_POINTS];
    if (prev < mid && curr >= mid) crossings++;
  }
  const freq = crossings / (2 * (MAX_POINTS / SAMPLE_RATE));
  let high = 0;
  for (let i = 0; i < MAX_POINTS; i++) { if (data[i] > mid) high++; }
  const duty = (high / MAX_POINTS) * 100;

  document.getElementById('freq').textContent = freq.toFixed(1);
  document.getElementById('vpp').textContent  = (mx - mn).toFixed(3) + ' V';
  document.getElementById('vmin').textContent = mn.toFixed(3) + ' V';
  document.getElementById('vmax').textContent = mx.toFixed(3) + ' V';
  document.getElementById('vrms').textContent = vrms.toFixed(3) + ' V';
  document.getElementById('duty').textContent = duty.toFixed(1);
}

function updateLabels() {
  document.getElementById('scaleLabel').textContent = voltsPerDiv.toFixed(2) + ' V/div';
  document.getElementById('timeLabel').textContent  = timePerDiv.toFixed(2)  + ' ms/div';
  document.getElementById('pauseBtn').textContent   = paused    ? '> Resume' : '|| Pause';
  document.getElementById('autoBtn').textContent    = autoScale ? 'Auto ON'  : 'Auto';
}

document.getElementById('vMinus').onclick = () => {
  autoScale = false;
  voltsPerDiv = Math.max(0.05, parseFloat((voltsPerDiv - 0.1).toFixed(2)));
  displayFullScale = voltsPerDiv * 4; updateLabels(); draw();
};
document.getElementById('vPlus').onclick = () => {
  autoScale = false;
  voltsPerDiv = parseFloat((voltsPerDiv + 0.1).toFixed(2));
  displayFullScale = voltsPerDiv * 4; updateLabels(); draw();
};
document.getElementById('tMinus').onclick = () => {
  timePerDiv = Math.max(0.1, parseFloat((timePerDiv - 0.1).toFixed(2)));
  updateLabels();
};
document.getElementById('tPlus').onclick = () => {
  timePerDiv = parseFloat((timePerDiv + 0.1).toFixed(2));
  updateLabels();
};
document.getElementById('pauseBtn').onclick = () => {
  paused = !paused; updateLabels();
};
document.getElementById('autoBtn').onclick = () => {
  autoScale = !autoScale;
  if (autoScale) applyAutoScale();
  updateLabels();
};
document.getElementById('themeBtn').onclick = () => {
  lightMode = !lightMode;
  document.body.classList.toggle('light', lightMode);
  document.getElementById('themeBtn').textContent = lightMode ? 'DARK' : 'LIGHT';
  draw();
};
document.getElementById('screenshotBtn').onclick = () => {
  const tmp  = document.createElement('canvas');
  tmp.width  = canvas.width;
  tmp.height = canvas.height;
  const tctx = tmp.getContext('2d');
  tctx.fillStyle = getComputedStyle(document.body).getPropertyValue('--surface').trim();
  tctx.fillRect(0, 0, tmp.width, tmp.height);
  tctx.drawImage(canvas, 0, 0);
  const link = document.createElement('a');
  link.download = 'scope_' + Date.now() + '.png';
  link.href = tmp.toDataURL('image/png');
  link.click();
};

// SSE connection with auto-reconnect
const dot = document.getElementById('statusDot');
let sse, reconnTimer;

function connectSSE() {
  sse = new EventSource('/data');
  sse.onopen = () => {
    dot.classList.add('on');
    if (reconnTimer) { clearTimeout(reconnTimer); reconnTimer = null; }
  };
  sse.addEventListener('adc', (e) => {
    const parts = e.data.split(',');
    const vals  = [];
    for (let p of parts) {
      const n = parseInt(p);
      if (!isNaN(n)) vals.push(n);
    }
    if (vals.length > 0) addSamples(vals);
  });
  sse.onerror = () => {
    dot.classList.remove('on');
    sse.close();
    reconnTimer = setTimeout(connectSSE, 2000);
  };
}

updateLabels();
connectSSE();
</script>
</body>
</html>
)rawliteral";

// ============================================================
//  SECTION 4: HTTP HANDLERS
// ============================================================
void handleRoot() {
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma",        "no-cache");
  server.sendHeader("Expires",       "0");
  server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

void handleData() {
  // Reject if another SSE client already connected
  if (sseClientConnected && sseClient.connected()) {
    server.send(503, "text/plain", "Busy");
    return;
  }

  // Send SSE headers via WebServer first
  server.sendHeader("Cache-Control",                "no-cache");
  server.sendHeader("Access-Control-Allow-Origin",  "*");
  server.sendHeader("Connection",                   "keep-alive");

  // Get the underlying client AFTER headers are sent
  server.send(200, "text/event-stream", "");
  
  WiFiClient client    = server.client();
  client.print("retry: 2000\n\n");
  client.flush();

  sseClient            = client;
  sseClientConnected   = true;
  Serial.println("SSE client connected");
}

void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

// ============================================================
//  SECTION 5: SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 Oscilloscope - Booting...");

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  server.on("/",     HTTP_GET, handleRoot);
  server.on("/data", HTTP_GET, handleData);
  server.onNotFound(handleNotFound);
  server.begin();

  Serial.print("WiFi: ");
  Serial.println(AP_SSID);
  Serial.println("Browser: http://192.168.4.1");
}

// ============================================================
//  SECTION 6: MAIN LOOP
// ============================================================
void loop() {
  server.handleClient();
  delay(1);  // yield to WiFi stack — prevents raw HTML glitch on refresh

  if (!sseClientConnected) return;
  if (!sseClient.connected()) {
    sseClientConnected = false;
    batchIdx = 0;
    Serial.println("SSE client disconnected");
    return;
  }

  unsigned long nowUs = micros();
  if (nowUs - lastSampleUs < sampleIntervalUs) return;
  lastSampleUs = nowUs;

  batchBuf[batchIdx++] = (uint16_t)analogRead(ADC_PIN);

  if (batchIdx >= BATCH_SIZE) {
    String msg = "event: adc\ndata: ";
    for (int i = 0; i < BATCH_SIZE; i++) {
      msg += batchBuf[i];
      if (i < BATCH_SIZE - 1) msg += ',';
    }
    msg += "\n\n";
    sseClient.print(msg);
    sseClient.flush();
    batchIdx = 0;
  }
}
