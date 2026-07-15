#include <OneWire.h>
#include <DallasTemperature.h>
#include <SPI.h>
#include <RF24.h>

// ---------- Pin definitions (Arduino Uno) ----------
#define TDS_PIN        A0   // analog: TDS sensor stand-in (potentiometer)
#define PH_PIN         A1   // analog: pH sensor stand-in (potentiometer)
#define TURBIDITY_PIN  A2   // analog: turbidity sensor stand-in (potentiometer)
#define ONE_WIRE_BUS    2   // digital: DS18B20 temperature sensor
#define RADIO_CE_PIN    9   // nRF24L01 chip-enable
#define RADIO_CSN_PIN   8   // nRF24L01 chip-select

// ---------- Timing ----------
#define READING_INTERVAL_MS 10000

// ---------- ADC reference (Uno: 10-bit ADC, 5V logic) ----------
#define ADC_RESOLUTION 1023

// ---------- Water-quality safety thresholds (WHO-style guidance) ----------
#define TDS_ALERT_PPM   500.0
#define PH_ALERT_LOW    6.5
#define PH_ALERT_HIGH   8.5

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature tempSensor(&oneWire);

RF24 radio(RADIO_CE_PIN, RADIO_CSN_PIN);
const byte radioAddress[6] = "WQM01"; // Water Quality Monitor, node 01

// Compact struct sent over the air instead of a JSON string --
// radio packets are small (max 32 bytes), so we pack raw values
// rather than transmitting text.
struct SensorPacket {
  unsigned long timestampMs;
  float tempC;
  float tdsPpm;
  float ph;
  float turbidityNtu;
  bool tdsAlert;
  bool phAlert;
};

unsigned long lastReading = 0;

float remapToVoltage(int raw, int minMv, int maxMv) {
  long mv = map(raw, 0, ADC_RESOLUTION, minMv, maxMv);
  return mv / 1000.0;
}

float readTDS(float temperatureC) {
  int raw = analogRead(TDS_PIN);
  float voltage = remapToVoltage(raw, 0, 2300);

  float compensationCoefficient = 1.0 + 0.02 * (temperatureC - 25.0);
  float compensatedVoltage = voltage / compensationCoefficient;

  float tdsValue = (133.42 * pow(compensatedVoltage, 3)
                     - 255.86 * pow(compensatedVoltage, 2)
                     + 857.39 * compensatedVoltage) * 0.5;

  return tdsValue < 0 ? 0 : tdsValue;
}

float readPH() {
  int raw = analogRead(PH_PIN);
  float voltage = remapToVoltage(raw, 1288, 3744);

  float slope  = -5.70;
  float offset = 21.34;
  float ph = slope * voltage + offset;

  if (ph < 0) ph = 0;
  if (ph > 14) ph = 14;
  return ph;
}

float readTurbidity() {
  int raw = analogRead(TURBIDITY_PIN);
  float voltage = remapToVoltage(raw, 925, 4201);

  float ntu = -1120.4 * voltage * voltage + 5742.3 * voltage - 4352.9;
  return ntu < 0 ? 0 : ntu;
}

void setup() {
  Serial.begin(115200);
  tempSensor.begin();

  // --- Radio setup ---
  if (!radio.begin()) {
    Serial.println("{\"status\":\"radio_init_failed\"}");
  } else {
    radio.openWritingPipe(radioAddress);
    radio.setPALevel(RF24_PA_LOW);   // low power, appropriate for a
                                      // battery/solar field deployment
    radio.stopListening();           // this node is transmit-only
    Serial.println("{\"status\":\"boot_ok\",\"radio\":\"ready\"}");
  }
}

void loop() {
  if (millis() - lastReading >= READING_INTERVAL_MS) {
    lastReading = millis();

    tempSensor.requestTemperatures();
    float temperatureC = tempSensor.getTempCByIndex(0);
    if (temperatureC == DEVICE_DISCONNECTED_C) temperatureC = 25.0;

    float tds        = readTDS(temperatureC);
    float ph         = readPH();
    float turbidity  = readTurbidity();

    bool tdsAlert = tds > TDS_ALERT_PPM;
    bool phAlert  = (ph < PH_ALERT_LOW) || (ph > PH_ALERT_HIGH);

    // --- Existing Serial/JSON output (unchanged) ---
    Serial.print("{");
    Serial.print("\"timestamp_ms\":");   Serial.print(millis());
    Serial.print(",\"temp_c\":");        Serial.print(temperatureC, 2);
    Serial.print(",\"tds_ppm\":");       Serial.print(tds, 2);
    Serial.print(",\"ph\":");            Serial.print(ph, 2);
    Serial.print(",\"turbidity_ntu\":"); Serial.print(turbidity, 2);
    Serial.print(",\"tds_alert\":");     Serial.print(tdsAlert ? "true" : "false");
    Serial.print(",\"ph_alert\":");      Serial.print(phAlert ? "true" : "false");
    Serial.println("}");

    // --- Wireless transmission (new) ---
    SensorPacket packet;
    packet.timestampMs  = millis();
    packet.tempC        = temperatureC;
    packet.tdsPpm        = tds;
    packet.ph            = ph;
    packet.turbidityNtu  = turbidity;
    packet.tdsAlert      = tdsAlert;
    packet.phAlert        = phAlert;

    bool sendOk = radio.write(&packet, sizeof(packet));
    Serial.print("{\"radio_tx_attempted\":true,\"radio_tx_success\":");
    Serial.print(sendOk ? "true" : "false");
    Serial.println("}");
  }
}
