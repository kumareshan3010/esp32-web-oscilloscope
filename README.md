# 📟 ESP32 Web Oscilloscope

[![Platform](https://img.shields.io/badge/platform-ESP32-blue)](https://www.espressif.com/en/products/socs/esp32)
[![Language](https://img.shields.io/badge/language-C++-orange)](https://isocpp.org/)
[![Docs](https://img.shields.io/badge/docs-Wiki-blue)](https://github.com/kumareshan3010/esp32-web-oscilloscope/wiki)
[![License](https://img.shields.io/badge/license-MIT-green)](LICENSE)
[![Status](https://img.shields.io/badge/status-Completed-brightgreen)]()

> A real-time web-based oscilloscope built on ESP32 WROOM-32, accessible from any browser on your phone or PC — no app, no drivers, no external libraries.

**Author:** Kumareshan V — [@kumareshan3010](https://github.com/kumareshan3010)

---

## 📌 Overview

This project turns an ESP32 into a fully functional oscilloscope that streams live waveform data to a browser over WiFi. The ESP32 creates its own Access Point hotspot — connect your phone or laptop to it and open the oscilloscope UI instantly at `http://192.168.4.1`.

A custom **resistor attenuator circuit** extends the input range from the ESP32's native 0–3.3V all the way to **±20V**, making it suitable for real electronics lab work — not just low-voltage signals.

Data is streamed using **Server-Sent Events (SSE)** over the built-in `WebServer.h` library — zero external dependencies.

---

## 📖 Documentation

Detailed explanations and design documentation are available in the Wiki:

👉 https://github.com/kumareshan3010/esp32-web-oscilloscope/wiki

---

## ✨ Features

| Feature | Details |
|---|---|
| **Real-time Waveform** | Live canvas plot, 10000 samples/sec, 600-point circular buffer |
| **Input Range** | ±20V via resistor attenuator (native ADC: 0–3.3V) |
| **Frequency Display** | Zero-crossing detection, live Hz readout |
| **Vpp / Vmin / Vmax** | Peak-to-peak and absolute min/max in real volts |
| **Vrms** | True RMS calculation |
| **Duty Cycle** | Percentage high/low time |
| **V/div Control** | Adjustable from 0.5V to any range, 1V steps |
| **ms/div Control** | Time base label control |
| **Auto-scale** | One tap to auto-fit signal on screen |
| **Pause / Resume** | Freeze waveform for inspection |
| **Dark / Light Theme** | Toggle between oscilloscope green-on-black and clean light mode |
| **Screenshot** | Save current waveform as PNG to your device |
| **Status Indicator** | Green dot = connected, Red = disconnected |
| **Auto-reconnect** | Browser reconnects automatically if WiFi drops |
| **Mobile + Desktop** | Fully responsive UI, works on any screen size |
| **No Libraries** | Only built-in `WiFi.h` and `WebServer.h` — compiles anywhere |

---

## 🛠 Hardware

| Component | Specification |
|---|---|
| Microcontroller | ESP32 WROOM-32 |
| ADC Input Pin | GPIO 34 (input-only, ideal for ADC) |
| Input Resistor R1 | 100kΩ (series, input protection + voltage divider) |
| Divider Resistor R2 | 8.2kΩ (to bias node) |
| Bias Resistors R3, R4 | 10kΩ each (3.3V → bias → GND) |
| Decoupling Capacitor C1 | 100nF (bias node to GND) |
| Protection Diodes D1, D2 | BAT54 Schottky (clamp to 3.3V and GND) |
| Input Range | ±20V |
| ADC Range | 0–3.3V (after attenuator) |

---

## 🔌 Attenuator Circuit

The attenuator scales ±20V input down to 0.13V–3.17V for the ESP32 ADC, centred at 1.65V bias.

### Schematic

```
                    R1 (100kΩ)
INPUT (±20V) ────────┤├─────────┬─────────► GPIO 34 (ADC)
                                │
                              R2 (8.2kΩ)
                                │
                   ┌────────────┘
                   │         (BIAS NODE = 1.65V)
3.3V ──── R3(10kΩ) ┤
                   └──── R4(10kΩ) ──── GND

Bias decoupling:
BIAS NODE ──── C1(100nF) ──── GND

ADC Protection:
GPIO 34 ──── D1(BAT54) ──► 3.3V    (clamp high)
GPIO 34 ──── D2(BAT54) ──► GND     (clamp low)
```

### How It Works

The voltage divider (R1 + R2) attenuates the input, while the bias network (R3 + R4) shifts the output up by 1.65V so negative voltages map above 0V for the ADC.

**Attenuation math:**
```
Vout = (Vin × R2/(R1+R2)) + Vbias

At +20V input:  Vout = (20 × 8.2/108.2) + 1.65 = 3.17V  ✓
At   0V input:  Vout = 0 + 1.65            = 1.65V  ✓
At -20V input:  Vout = (-20 × 8.2/108.2) + 1.65 = 0.13V  ✓
```

All values safely within 0–3.3V ADC range ✓

**Reverse conversion in firmware:**
```
Vin = (Vadc - Vbias) × (R1+R2)/R2
    = (Vadc - 1.65)  × 13.195
```

### Component Values Summary

| Component | Value | Purpose |
|---|---|---|
| R1 | 100kΩ | Series input resistor + main attenuator |
| R2 | 8.2kΩ | Lower leg of divider to bias node |
| R3 | 10kΩ | Bias: 3.3V to bias node |
| R4 | 10kΩ | Bias: bias node to GND |
| C1 | 100nF | Decouples bias node (reduces noise) |
| D1 | BAT54 | Clamps ADC pin to max 3.3V |
| D2 | BAT54 | Clamps ADC pin to min 0V |

---

## 🚀 Quick Start

### 1. Hardware
- Build the attenuator circuit on a breadboard or PCB
- Connect attenuator output to ESP32 GPIO 34
- Power ESP32 via USB

### 2. Software
- Open `ESP32_Oscilloscope_SSE.ino` in Arduino IDE
- Select **ESP32 Dev Module** as board
- Upload — no library installation needed
- Open Serial Monitor at 115200 baud to confirm boot

### 3. Connect
- On your phone/PC, go to WiFi settings
- Connect to: `ESP32_Oscilloscope`
- Password: `12345678`
- Open browser at: `http://192.168.4.1`

---

## 💻 How It Works

### Firmware Architecture

```
loop()
  ├── server.handleClient()     ← serves HTML page on request
  ├── Sample ADC at 10 kHz       ← precise micros() timing
  ├── Batch 10 samples          ← reduces WiFi overhead
  └── Send via SSE              ← "event: adc\ndata: v1,v2,...\n\n"
```

### Server-Sent Events (SSE)

SSE is a lightweight HTTP streaming protocol — the browser opens one persistent connection to `/data` and the ESP32 pushes data continuously. Unlike WebSockets, SSE uses plain HTTP and works with the built-in `WebServer.h` — no external libraries needed.

### Voltage Reconstruction

Every raw ADC value received by the browser is converted to real input voltage:
```javascript
const vadc = (raw / 4095) * 3.3;         // ADC pin voltage
const vin  = (vadc - 1.65) * 13.195;     // Real input voltage
```

---

## 📐 Technical Specifications

| Parameter | Value |
|---|---|
| Sampling Rate | 10 kHz |
| Buffer Size | 600 samples |
| Time Window | 1.2 seconds |
| ADC Resolution | 12-bit (0–4095) |
| ADC Reference | 3.3V |
| Input Impedance | ~108.2kΩ (R1+R2 in series) |
| Bias Voltage | 1.65V |
| Attenuation Ratio | 1:13.195 |
| Max Safe Input | ±20V |
| Absolute Max Input | ±24V (diode clamp limit) |
| WiFi Mode | Access Point (AP) |
| IP Address | 192.168.4.1 |
| Protocol | SSE (Server-Sent Events) |

---

## 🔭 Future Improvements

- [ ] Dual channel input (two ADC pins)
- [ ] Trigger level control (rising/falling edge)
- [ ] FFT frequency spectrum display
- [ ] Horizontal zoom (variable time base affecting actual sample window)
- [ ] CSV data export
- [ ] Station mode (connect to existing WiFi)
- [ ] Input coupling switch (AC/DC)
- [ ] Adjustable sample rate from browser
- [ ] Cursor measurements (ΔV, Δt between two points)
- [ ] Signal generator output on DAC pin

---

## ⚠️ Safety Warning

> This oscilloscope is for **low-voltage signal measurement only**. While the attenuator supports ±20V, it is **not isolated** from the ESP32 power supply. Never connect this to mains voltage (230V AC) or any high-voltage source. The BAT54 diodes provide protection only against transient spikes beyond ±20V — sustained overvoltage will damage the ESP32.

---

## 📄 License

MIT License — free to use, modify, and distribute with attribution.

---

## 🙏 Acknowledgements

- ESP32 Arduino Core by Espressif Systems
- Server-Sent Events (W3C specification)
- HTML5 Canvas API
