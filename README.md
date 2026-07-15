# Low-Cost IoT Water Quality Monitoring Architecture

A low-cost, low-power sensing architecture for monitoring water salinity and pH in areas dealing with water-quality crises and unreliable electricity. I designed, simulated, and validated the whole thing using free tools only — no hardware was purchased.

---

## 1. Motivation & Scope

This project targets a specific, common situation: contaminated or saline water, combined with no reliable grid electricity to run conventional monitoring equipment. That constraint shaped every decision below — low power draw, low per-unit cost, and a telemetry method that doesn't depend on existing internet or cellular coverage.

**What it senses:**

| Parameter | Role | Why it matters |
|---|---|---|
| TDS / Conductivity (salinity) | Primary | Ties directly to the salinity-crisis framing |
| pH | Primary | Core drinking-water safety indicator |
| Turbidity | Secondary | Proxy for sediment/contamination/filtration failure |
| Temperature | Secondary | Needed to calibrate/compensate the other three sensors |

---

## 2. Component Selection (Bill of Materials)

| Component | Example part | Approx. price (USD) |
|---|---|---|
| TDS sensor | DFRobot Gravity Analog TDS Sensor (SEN0244) | ~$11.80 |
| pH sensor | DFRobot Gravity Analog pH Sensor/Meter Kit (SEN0161) | ~$29.50 |
| Turbidity sensor | DFRobot Gravity Analog Turbidity Sensor (SEN0189) | ~$9.90 |
| Temperature probe | DFRobot Waterproof DS18B20 Kit (SEN0257) | ~$7.50 |
| Microcontroller + radio | LILYGO TTGO LoRa32 (ESP32 + SX1276 LoRa) | ~$25–36 |
| Wireless telemetry (prototyped) | nRF24L01+ module | ~$1–3 |
| Misc. (wiring, resistors, enclosure) | — | ~$10–15 |

Total: roughly **$95–110** for a full 4-sensor, wirelessly-telemetered prototype. I didn't buy any of this — it's a paper BOM, priced with real current listings so the design is checkable rather than hand-waved.

**Why LoRa instead of WiFi:** given the "no reliable electricity/infrastructure" framing, LoRa is the radio I'd actually deploy — long-range, low-power, and it doesn't need existing WiFi or cell coverage nearby. WiFi is a reasonable fallback if a site happens to be near existing infrastructure, but it's not the honest default here.

---

## 3. Circuit & Firmware Simulation

Built in [Wokwi](https://wokwi.com) (free browser/VS-Code simulator) and compiled locally with [Arduino CLI](https://arduino.github.io/arduino-cli/).

**Why Arduino Uno instead of ESP32:** I originally targeted the ESP32/LoRa32 combo, but Wokwi's free-tier ESP32 compile queue was unreliable enough (repeated "build servers busy" failures, even after switching to local compilation) that I switched the simulation target to an Arduino Uno to get a working, testable circuit. The sensor-reading logic itself is architecture-agnostic — the same formulas and structure carry over directly to ESP32/LoRa32; only pin numbers and the ADC's resolution/reference voltage would need to change.

**How the sensors are simulated:** Wokwi doesn't have virtual models of the actual DFRobot TDS/pH/turbidity sensors, so I substituted potentiometers — turning a dial produces an adjustable analog voltage, the same way the real sensors work. I remap each potentiometer's full turn onto the real sensor's actual operating voltage window (rather than the raw 0–5V range) so the whole dial sweep maps onto physically meaningful readings from each sensor's real calibration curve. The DS18B20 temperature sensor has a native Wokwi model, so that one's simulated directly, no substitution needed.

**What the firmware does:**
- Reads all 4 sensors every 10 seconds (a real deployment would use 60s+ to save power — see Section 5)
- Converts raw ADC readings into real units (ppm, pH, NTU, °C) using each sensor's published datasheet formula
- Applies temperature compensation to the TDS reading
- Flags contamination via threshold checks (TDS > 500 ppm; pH outside 6.5–8.5)
- Prints a structured JSON line per cycle over Serial

**Wireless telemetry:** I implemented this using the `RF24` library and standard nRF24L01+ SPI wiring, transmitting a packed struct of sensor readings each cycle. I hit a wall here, though — Wokwi's built-in nRF24L01 model doesn't emulate the chip's internal registers closely enough for the RF24 library's handshake to succeed, so `radio.begin()` reliably fails in simulation. Rather than fake around this, I'm flagging it directly: the transmit code is correct and follows the same pattern used in real physical nRF24L01 deployments — it just couldn't be live-verified inside this particular simulator.

---

## 4. Python Analysis Layer

Built in Google Colab (free, nothing installed locally) with pandas and matplotlib.

- **Data ingestion:** loads sensor readings I exported from Wokwi across three manually-driven scenarios — clean water, contaminated water, and a borderline case sitting right at the alert threshold.
- **Trend plots:** per-parameter scatter plots with threshold lines overlaid (500 ppm TDS limit; 6.5–8.5 pH safe band).
- **Contamination alerts:** flags any reading over the TDS or pH limits, with a plain-language reason generated per flagged row (e.g. "TDS 664.8 ppm > 500 ppm limit").
- **Smoothing:** a rolling-average filter (window=3, grouped per scenario) to dampen sensor jitter, shown on the turbidity signal.

**Result:** across 15 genuine (non-idle) readings, 5 (33%) correctly triggered an alert — 3 clear contamination cases (both TDS and pH violated) and 2 borderline cases (TDS only) — with no false positives or missed detections against the thresholds I defined.

---

## 5. Power Budget Estimate (real deployment target)

Rough estimate for the ESP32/LoRa32 + 4-sensor setup, assuming a 60-second wake cycle:

| State | Duration | Approx. current draw |
|---|---|---|
| Active (sensors + MCU + LoRa TX) | ~3 s | ~150 mA |
| Deep sleep | ~57 s | ~0.02 mA |

That averages out to about **7.5 mA**. On a single 2500 mAh 18650 Li-ion cell, that's roughly **330 hours (~14 days)** without recharging — longer with a small solar trickle charger.

These numbers come from datasheets, not a physical build, so treat them as a demonstration of the power-budgeting approach rather than a guaranteed spec.

---

## 6. Known Limitations

- TDS/pH calibration constants are representative datasheet values — a physical build would need calibration against known reference solutions.
- The simulation target is Arduino Uno, not the intended ESP32/LoRa32, because of Wokwi's free-tier ESP32 build-queue congestion.
- Wireless (nRF24L01) transmission code is implemented but not live-verified in simulation, for the reason explained in Section 3.
- The analysis dataset was manually captured across a handful of representative scenarios, not logged continuously over time.

---

## 7. Repository Structure

```
├── firmware/
│   ├── water_quality_monitor.ino   # Arduino sketch (sensor reading + wireless TX)
│   ├── diagram.json                 # Wokwi circuit definition
│   ├── wokwi.toml                   # Wokwi simulator config
│   └── libraries.txt
├── data/
│   └── water_quality_readings.csv  # Captured simulation dataset
├── analysis/
│   └── water_quality_analysis.ipynb # Python/Colab notebook (trends + alerts + filtering)
└── README.md
```

---

## 8. License

MIT License.
