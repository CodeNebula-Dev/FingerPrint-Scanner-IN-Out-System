# Fingerprint Scanner Prototype — Hardware Selection & Assembly Guide
 
This document compares low-cost fingerprint sensor and microcontroller options for bench-testing the existing C++/Python fingerprint software, and walks through physical assembly, wiring, and firmware setup.
 
---
 
## 1. Fingerprint Sensor Options
 
| Sensor | Image | Price (India) | Templates | Interface | Notes |
|---|---|---|---|---|---|
| **R307** | ![R307](https://www.electronicscomp.com/image/cache/catalog/r307-fingerprint-sensor-module-india-2-800x800.jpg) | ~₹750–800 | 1,000 | TTL UART (+ optional USB 2.0) | Optical sensor + DSP processor, default baud 57600, works directly with 3.3V or 5V logic |
| **AS608** | ![AS608](https://www.electronicscomp.com/image/cache/catalog/as608-optical-fingerprint-sensor-fingerprint-module-800x800.jpg) | ~₹700–800 | 1,000 | UART/TTL | Pin- and protocol-compatible with R307; same Adafruit Fingerprint library works for both |
| **GT521F32** | — | ~₹1,994 | 3,000 | UART (JST SH connector) | Higher template capacity, overkill for bench testing |
 
**Recommendation:** Keep the **R307** (or its twin, the AS608) — no need to switch. Both use the same UART packet protocol, so your existing driver code should work unmodified on either.
 
---
 
## 2. Microcontroller Options
 
| Board | Image | Price | RAM | Wireless | Deep-sleep current | Best for |
|---|---|---|---|---|---|---|
| **ESP32 (WROOM dev board)** | ![ESP32](https://commons.wikimedia.org/wiki/Special:FilePath/ESP32%20Dev%20Board.jpg) | ~$5 | ~520KB | WiFi + BLE | ~5µA (C3 variant) | Mature libraries, WiFi-heavy testing, cheapest general-purpose board |
| **Raspberry Pi Pico 2 W** | ![Pico](https://commons.wikimedia.org/wiki/Special:FilePath/Raspberry%20Pi%20Pico%20top%20and%20bottom.jpg) | ~$7–8 | 520KB | WiFi + BLE | ~25µA | PIO state machines, TrustZone security, RP2350 dual-arch |
| Plain ESP32 / bare Pico (no wireless) | — | Cheapest of all | Same core specs | None | — | Pure bench testing where WiFi isn't needed yet |
 
**Recommendation:** Price has converged between ESP32 and Pico 2W, so pick based on your firmware stack, not cost. For pure UART sensor bring-up (no wireless needed yet), a bare ESP32 dev board is usually the cheapest available option in India.
 
---
 
## 3. Tools & Materials Needed for Assembly
 
- Fingerprint sensor module (R307 or AS608)
- Microcontroller board (ESP32 or Pico 2W)
- Breadboard (half-size is enough)
- 4–6 male-to-female jumper wires
- Micro-USB or USB-C cable (per board) for power + programming
- Multimeter (to verify voltage before first power-up)
- Optional: 100–220µF electrolytic capacitor (decoupling, reduces brown-out risk during scans)
- Optional: small enclosure/3D-printed mount to hold the sensor steady during repeated testing
---
 
## 4. Wiring / Connection Process
 
The R307 and AS608 both expose the same functional pins: **VCC, GND, TXD, RXD**, and an optional **WAKEUP/touch** pin.
 
> ⚠️ **TX/RX are crossed**: sensor TXD → microcontroller RX, and sensor RXD → microcontroller TX.
 
### 4a. R307/AS608 → Raspberry Pi Pico 2W
 
| Sensor Pin | Pico 2W Pin | Purpose |
|---|---|---|
| VCC | VBUS (5V) or 3V3 — check sensor's rated voltage | Power |
| GND | GND | Ground |
| TXD | GP1 (UART0 RX) | Sensor → Pico data |
| RXD | GP0 (UART0 TX) | Pico → Sensor data |
| WAKEUP (optional) | Any free GPIO (input) | Touch-detect interrupt |
 
### 4b. R307/AS608 → ESP32
 
| Sensor Pin | ESP32 Pin | Purpose |
|---|---|---|
| VCC | 5V (VIN) or 3V3 | Power |
| GND | GND | Ground |
| TXD | GPIO16 (RX2) | Sensor → ESP32 data |
| RXD | GPIO17 (TX2) | ESP32 → Sensor data |
| WAKEUP (optional) | Any free GPIO (input) | Touch-detect interrupt |
 
### Assembly Steps
 
1. **Mount the board on the breadboard** — place the ESP32/Pico 2W so its two pin rows straddle the breadboard's central gap.
2. **Power off** — always wire with the USB cable unplugged.
3. **Connect GND first** — sensor GND to board GND. Establishing a common ground first avoids voltage-reference glitches.
4. **Connect VCC** — double-check the sensor's rated voltage against the pin you're using (5V vs 3.3V) with a multimeter before powering on.
5. **Cross-connect TX/RX** — sensor TXD → board RX pin, sensor RXD → board TX pin.
6. **(Optional) Add the decoupling capacitor** across VCC/GND close to the sensor — helps with current spikes during fingerprint image capture.
7. **Power up via USB** and confirm the sensor's LED illuminates (most R307/AS608 modules flash blue/red on power-up).
8. **Flash firmware** (see below) and check the serial monitor for a successful handshake with the sensor before wiring in the rest of the circuit.
---
 
## 5. Firmware / Software Setup Notes
 
- **Baud rate:** Default is `57600` for both R307 and AS608 — confirm this matches your UART init in code.
- **UART pins:**
  - Pico 2W: `UART0` on `GP0 (TX)` / `GP1 (RX)` is the simplest default; can be remapped in the SDK if those pins are needed elsewhere.
  - ESP32: Use `Serial2` (`GPIO16/17`) to keep `Serial` (USB) free for debug logging.
- **Library:** The Adafruit Fingerprint Sensor library (Arduino/C++) works for both R307 and AS608 without modification, since they share the same packet protocol. For Python (e.g., on a Pico via MicroPython, or ESP32 via MicroPython/CircuitPython), use a UART-based fingerprint driver matching the same protocol.
- **First test:** Send a `GetImage` command and confirm the sensor responds with an acknowledgement packet before attempting enrollment/match routines — this confirms wiring and baud rate are correct before debugging higher-level logic.
- **Power stability:** If the sensor resets or disconnects during a scan, it's usually a power-supply/current-spike issue, not a code bug — check the decoupling capacitor and wire gauge first.
---
 
## 6. Quick Reference Summary
 
- **Sensor:** Stick with R307 (or AS608 as a drop-in equivalent) — cheapest and best-supported.
- **MCU:** ESP32 dev board is generally the lowest-cost path for bench testing; Pico 2W is a fine alternative if you're already invested in the RP2350 toolchain.
- **Wiring:** 4 wires (VCC, GND, TX↔RX crossed) is all that's required for a first working test.
- **Watch for:** voltage mismatch (confirm sensor's rated VCC), crossed TX/RX, and current spikes during scanning.
 
