# Low-Cost IoT Water Quality Monitoring Architecture

A low-cost, low-power sensing architecture for monitoring water salinity and pH in communities facing water-quality crises and unreliable electricity infrastructure. Designed, simulated, and validated entirely using free tools — no physical hardware purchase required.

---

## 1. Motivation & Scope

Many water-crisis regions face two compounding problems: contaminated or saline water, and no reliable grid electricity to power conventional monitoring equipment. This project designs a sensing architecture around that constraint — prioritizing low power draw, low per-unit cost, and telemetry methods that don't depend on existing internet/cellular infrastructure.

**Sensing scope:**

| Parameter | Role | Why it matters |
|---|---|---|
| TDS / Conductivity (salinity) | Primary | Directly reflects the salinity-crisis framing |
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

**Estimated total: ~$95–110** for a full 4-sensor, wirelessly-telemetered prototype. This is a paper BOM — no hardware was purchased; pricing exists so the design is checkable and realistic.

**Telemetry decision — LoRa over WiFi:** given the "no reliable electricity/infrastructure" framing, LoRa is the intended real-deployment radio: long-range, low-power, and independent of existing WiFi/cellular coverage. WiFi remains a viable fallback for sites near existing infrastructure.

---

## 3. Circuit & Firmware Simulation

Simulated in [Wokwi](https://wokwi.com), a free browser/VS-Code-based ESP32/Arduino simulator, and compiled locally with [Arduino CLI](https://arduino.github.io/arduino-cli/).

**Note on simulation target:** the circuit and firmware were built and validated on an **Arduino Uno** rather than the intended ESP32/LoRa32, due to persistent free-tier compile-queue congestion on Wokwi's ESP32 toolchain. All sensor-reading and analysis logic is architecture-agnostic — the same formulas and structure apply directly to the ESP32/LoRa32 target; only pin numbers and ADC resolution/reference voltage would change (documented inline in the firmware).

**Sensor simulation approach:** Wokwi does not provide native virtual models of the DFRobot TDS/pH/turbidity sensors. Each is stood in for by a potentiometer producing an adjustable analog voltage — a standard, widely-used simulation technique. Each potentiometer's full sweep is remapped in firmware onto the real sensor's actual documented operating voltage window (rather than the raw 0–5V range), so that the full dial turn produces physically meaningful readings across the sensor's real calibration curve. The DS18B20 temperature sensor has a native Wokwi model and is simulated directly.

**Firmware behavior:**
- Reads all 4 sensors on a 10-second cycle (production default would be 60s+ to conserve power — see Section 5)
- Converts raw ADC values into real units (ppm, pH, NTU, °C) using each sensor's published datasheet formula
- Applies temperature compensation to the TDS reading
- Flags contamination via threshold checks (TDS > 500 ppm; pH outside 6.5–8.5)
- Outputs a structured JSON line per cycle over Serial

**Wireless telemetry:** implemented using the `RF24` library and standard nRF24L01+ SPI wiring (CE/CSN + shared SPI bus), transmitting a packed struct of the sensor readings each cycle. **Limitation:** live radio initialization could not be verified inside Wokwi's simulator — its built-in nRF24L01 model does not emulate the chip's internal registers closely enough for the RF24 library's handshake to succeed. This is a documented simulator constraint, not a code defect; the transmit implementation follows the same pattern used in verified physical nRF24L01 deployments.

---

## 4. Python Analysis Layer

Built in Google Colab (free, no local install) using pandas and matplotlib.

- **Data ingestion:** loads exported sensor readings (CSV) captured across three manually-driven scenarios: clean water, contaminated water, and a borderline case sitting at the alert threshold.
- **Trend visualization:** per-parameter scatter plots with threshold lines overlaid (TDS 500 ppm limit; pH 6.5–8.5 safe band).
- **Threshold-based contamination alerts:** automated flagging of any reading exceeding TDS or pH limits, with a plain-language explanation generated per flagged row (e.g., *"TDS 664.8 ppm > 500 ppm limit"*).
- **Adaptive smoothing:** a rolling-average filter (window=3, grouped per scenario) applied to dampen sensor jitter, demonstrated on the turbidity signal.

**Result summary:** across 15 genuine (non-idle) test readings, 5 (33%) correctly triggered an alert — 3 clear contamination cases (both TDS and pH violated) and 2 borderline cases (TDS only), with zero false positives or missed detections against the defined thresholds.

---

## 5. Power Budget Estimate (real deployment target)

Illustrative estimate for the ESP32/LoRa32 + 4-sensor configuration, assuming a 60-second wake cycle:

| State | Duration | Approx. current draw |
|---|---|---|
| Active (sensors + MCU + LoRa TX) | ~3 s | ~150 mA |
| Deep sleep | ~57 s | ~0.02 mA |

Average current ≈ **7.5 mA**. On a single 2500 mAh 18650 Li-ion cell, this projects to roughly **330 hours (~14 days)** of runtime without recharging — extendable indefinitely with a small solar trickle-charge setup.

*These figures are datasheet-derived estimates, not measured on physical hardware, and are intended to demonstrate the design's power-budgeting methodology rather than serve as a guaranteed spec.*

---

## 6. Known Limitations & Honest Caveats

- TDS and pH calibration constants are representative datasheet values; a physical deployment would calibrate against known reference solutions.
- Simulation was performed on Arduino Uno, not the intended ESP32/LoRa32 target, due to Wokwi free-tier build-queue constraints.
- Wireless (nRF24L01) transmission logic is implemented but not live-verified in simulation, due to a Wokwi platform limitation (see Section 3).
- The Python-analysis dataset was manually captured across a small number of representative scenarios, not continuously logged over time.

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


