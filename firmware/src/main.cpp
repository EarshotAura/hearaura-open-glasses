// HearAura open glasses: firmware for a Seeed XIAO ESP32-S3 (Sense), and for
// the XIAO ESP32-S3 on a Seeed reSpeaker Flex XVF3800 board.
//
// One program for every stage of the build. At start-up it looks for each
// part and uses whatever it finds, so nothing here has to be edited as the
// glasses grow:
//   screen (SSD1306 OLED on I2C)      shows captions, and a status page
//   motion sensor (BNO085 on I2C)     reports which way the head points
//   four I2S microphones              finds where a voice is
//   camera (on the Sense board)       finds the speaker's height and distance
//   reSpeaker Flex strip (XVF3800)    finds where a voice is by itself; when
//                                     it is there, the four microphones and
//                                     the camera are not looked for
// It talks to the HearAura iPhone app over the open link in docs/PROTOCOL.md.
//
// Wiring (XIAO pin = GPIO):
//   I2C   D4=5 SDA, D5=6 SCL          screen and motion sensor share these
//   Mics  D0=1 clock (SCK), D1=2 word select (WS), all four microphones
//         D2=3 data from the front pair, D10=9 data from the rear pair
//         left microphone of a pair: L/R to GND; right: L/R to 3V3
//
// On a reSpeaker Flex board the XIAO is already joined to the strip's chip:
//   I2C   the same D4/D5 (this is Wire's default; Seeed does not print it)
//   I2S   D9=8 clock, D8=7 word select, D6=43 sound from the chip
// so a motion sensor added to D4/D5 shares the bus with the chip (0x2C).
//
// Camera pictures never leave the board: only angles and a distance do.
// Status: builds cleanly; not yet run on hardware (see README.md).

#include <Arduino.h>
#include <Wire.h>
#include <NimBLEDevice.h>
#include <U8g2lib.h>
#include <Adafruit_BNO08x.h>
#include <driver/i2s.h>
#include <soc/gpio_sig_map.h>
#include <soc/io_mux_reg.h>
#include <rom/gpio.h>
#include "doa.h"
#include "eye.h"

// ---- Things you may want to change -----------------------------------------

// Measure your build (metres) once the microphones are on the glasses.
static const float MIC_WIDTH = 0.14f;      // left microphones to right microphones
static const float MIC_DEPTH = 0.09f;      // front microphones to rear microphones
// If "ahead" and "behind" come out swapped or skewed, clap from exactly one
// side, read the "front/rear delay" line on the serial monitor, and put that
// number here so it reads 0.
static const int REAR_OFFSET_SAMPLES = 0;
// The camera lens angle is CAMERA_HFOV in eye.cpp.

// ---- Pins ------------------------------------------------------------------

static const int PIN_SDA = 5, PIN_SCL = 6;
static const int PIN_MIC_CLOCK = 1, PIN_MIC_WS = 2, PIN_MIC_FRONT = 3, PIN_MIC_REAR = 9;
// reSpeaker Flex: fixed by that board.
static const int PIN_STRIP_CLOCK = 8, PIN_STRIP_WS = 7, PIN_STRIP_SOUND = 43;
static const uint8_t STRIP_ADDRESS = 0x2C;

// ---- The link (see docs/PROTOCOL.md) --------------------------------------------

static const char *SERVICE  = "8E7F1A50-0871-4A2C-9D4B-6A1B5C0E0001";
static const char *CAPTIONS = "8E7F1A50-0871-4A2C-9D4B-6A1B5C0E0002";
static const char *SENSORS  = "8E7F1A50-0871-4A2C-9D4B-6A1B5C0E0003";

static U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);
static Adafruit_BNO08x imu(-1);
static NimBLECharacteristic *sensorPort = nullptr;

// What was found at start-up.
static bool hasScreen = false, hasMotion = false, hasFrontMics = false, hasRearMics = false, hasCamera = false;
static bool hasStrip = false;
static volatile bool phoneConnected = false;

// Captions from the phone.
static String pendingText, liveText, lastFinal;
static uint8_t speaker = 0;
static int captionBearing = 0;
static bool captionBearingKnown = false;
static volatile bool dirty = true;
static unsigned long lastCaptionAt = 0;

// What the glasses sense.
static float yawDeg = 0, pitchDeg = 0, rollDeg = 0;
static volatile float voiceDeg = NAN;
static volatile unsigned long voiceAt = 0;
// Faces the camera sees are kept in eye.cpp (faceBearing and friends).

// ---- Bluetooth -------------------------------------------------------------

class LinkEvents : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer *) override { phoneConnected = true; dirty = true; }
  void onDisconnect(NimBLEServer *) override {
    phoneConnected = false;
    liveText = ""; lastFinal = ""; speaker = 0;
    dirty = true;
    NimBLEDevice::startAdvertising();          // be findable again at once
  }
};

class CaptionWrites : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *c) override {
    std::string v = c->getValue();
    if (v.size() < 4) return;
    uint8_t kind = v[0];
    speaker = v[1];
    int8_t raw = (int8_t)v[2];
    captionBearingKnown = raw != -128;
    captionBearing = raw * 2;
    pendingText += String(v.substr(4).c_str());
    if (v[3] & 1) return;                      // more frames of this caption follow
    if (kind == 1) liveText = pendingText;
    else if (kind == 2) { lastFinal = pendingText; liveText = ""; }
    else if (kind == 3) { liveText = ""; lastFinal = ""; speaker = 0; }
    pendingText = "";
    lastCaptionAt = millis();
    dirty = true;
  }
};

static void startLink() {
  NimBLEDevice::init("HearAura Glasses");
  NimBLEDevice::setMTU(247);
  NimBLEServer *server = NimBLEDevice::createServer();
  server->setCallbacks(new LinkEvents());
  NimBLEService *service = server->createService(SERVICE);
  NimBLECharacteristic *captions =
      service->createCharacteristic(CAPTIONS, NIMBLE_PROPERTY::WRITE_NR | NIMBLE_PROPERTY::WRITE);
  captions->setCallbacks(new CaptionWrites());
  sensorPort = service->createCharacteristic(SENSORS, NIMBLE_PROPERTY::NOTIFY);
  service->start();
  NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();
  advertising->addServiceUUID(SERVICE);
  advertising->setScanResponse(true);          // room for the name beside the long service ID
  advertising->start();
}

// ---- Screen ----------------------------------------------------------------

static const char *arrowFor(float degrees) {
  float a = fabsf(degrees);
  if (a > 135) return "behind";
  if (a < 25) return "ahead";
  return degrees < 0 ? "left" : "right";
}

// With no captions to show, the screen says what the glasses have found.
// This is what the build lessons check against.
static void drawStatus() {
  oled.setFont(u8g2_font_6x12_tf);
  oled.setCursor(0, 10);  oled.print("HearAura Glasses");
  oled.setCursor(0, 24);  oled.print(phoneConnected ? "phone: connected" : "phone: waiting");
  oled.setCursor(0, 36);  oled.print("motion: "); oled.print(hasMotion ? "yes" : "no");
  oled.setCursor(0, 48);  oled.print("ears: ");
  oled.print(hasStrip ? "strip" : hasFrontMics && hasRearMics ? "4" : hasFrontMics ? "2 (front)" : "none");
  oled.setCursor(72, 36); oled.print("eye: "); oled.print(hasCamera ? "yes" : "no");
  oled.setCursor(0, 60);
  if (millis() - voiceAt < 1500 && !isnan(voiceDeg)) {
    oled.print("voice: "); oled.print(arrowFor(voiceDeg)); oled.print(" "); oled.print((int)voiceDeg);
  } else if (hasFrontMics || hasStrip) {
    oled.print("voice: (listening)");
  }
}

static void drawCaption() {
  oled.setFont(u8g2_font_6x12_tf);
  String text = liveText.length() ? liveText : lastFinal;
  const int cols = 21, rows = 4;
  if ((int)text.length() > cols * rows) text = text.substring(text.length() - cols * rows);
  if (speaker) {
    oled.setCursor(0, 10);
    oled.print(speaker);
    if (captionBearingKnown) { oled.print(" "); oled.print(arrowFor(captionBearing)); }
  }
  for (int row = 0; row < rows && row * cols < (int)text.length(); row++) {
    oled.setCursor(0, 24 + row * 12);
    oled.print(text.substring(row * cols, min((int)text.length(), (row + 1) * cols)));
  }
}

static void draw() {
  if (!hasScreen) return;
  oled.clearBuffer();
  bool captions = (liveText.length() || lastFinal.length()) && millis() - lastCaptionAt < 20000;
  if (captions) drawCaption(); else drawStatus();
  oled.sendBuffer();
}

// ---- Microphones -----------------------------------------------------------

#define MIC_RATE 48000
#define MIC_BLOCK 2048

// All four microphones run from one clock so their samples line up. The
// front pair's port makes the clock; the rear pair's port listens to the
// same two wires.
static void startMics() {
  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
  cfg.sample_rate = MIC_RATE;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.dma_buf_count = 8;
  cfg.dma_buf_len = 512;
  i2s_pin_config_t front = {};
  front.mck_io_num = I2S_PIN_NO_CHANGE;
  front.bck_io_num = PIN_MIC_CLOCK;
  front.ws_io_num = PIN_MIC_WS;
  front.data_out_num = I2S_PIN_NO_CHANGE;
  front.data_in_num = PIN_MIC_FRONT;
  i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &front);

  cfg.mode = (i2s_mode_t)(I2S_MODE_SLAVE | I2S_MODE_RX);
  i2s_pin_config_t rear = {};
  rear.mck_io_num = I2S_PIN_NO_CHANGE;
  rear.bck_io_num = I2S_PIN_NO_CHANGE;
  rear.ws_io_num = I2S_PIN_NO_CHANGE;
  rear.data_out_num = I2S_PIN_NO_CHANGE;
  rear.data_in_num = PIN_MIC_REAR;
  i2s_driver_install(I2S_NUM_1, &cfg, 0, NULL);
  i2s_set_pin(I2S_NUM_1, &rear);
  // Feed the first port's clock pins back in to the second port, without
  // disturbing them as outputs.
  PIN_INPUT_ENABLE(GPIO_PIN_MUX_REG[PIN_MIC_CLOCK]);
  PIN_INPUT_ENABLE(GPIO_PIN_MUX_REG[PIN_MIC_WS]);
  gpio_matrix_in(PIN_MIC_CLOCK, I2S1I_BCK_IN_IDX, false);
  gpio_matrix_in(PIN_MIC_WS, I2S1I_WS_IN_IDX, false);
}

// A wired microphone always shows some hiss; an empty data pin reads as one
// unchanging value.
static bool pairIsAlive(const int16_t *left, const int16_t *right, int n) {
  int16_t lo = 32767, hi = -32768, lo2 = 32767, hi2 = -32768;
  for (int i = 0; i < n; i++) {
    if (left[i] < lo) lo = left[i];
    if (left[i] > hi) hi = left[i];
    if (right[i] < lo2) lo2 = right[i];
    if (right[i] > hi2) hi2 = right[i];
  }
  return hi - lo > 4 && hi2 - lo2 > 4;
}

static void soundTask(void *) {
  static int32_t rawFront[MIC_BLOCK * 2], rawRear[MIC_BLOCK * 2];
  static int16_t fl[MIC_BLOCK], fr[MIC_BLOCK], rl[MIC_BLOCK], rr[MIC_BLOCK];
  float floorRms = 0;
  unsigned long lastPrint = 0;
  for (;;) {
    size_t got = 0;
    i2s_read(I2S_NUM_0, rawFront, sizeof(rawFront), &got, portMAX_DELAY);
    i2s_read(I2S_NUM_1, rawRear, sizeof(rawRear), &got, pdMS_TO_TICKS(60));
    for (int i = 0; i < MIC_BLOCK; i++) {
      // INMP441: 24 bits at the top of a 32-bit word.
      fr[i] = rawFront[2 * i] >> 14;
      fl[i] = rawFront[2 * i + 1] >> 14;
      int j = i + REAR_OFFSET_SAMPLES;
      bool in = j >= 0 && j < MIC_BLOCK;
      rr[i] = in ? rawRear[2 * j] >> 14 : 0;
      rl[i] = in ? rawRear[2 * j + 1] >> 14 : 0;
    }
    bool front = pairIsAlive(fl, fr, MIC_BLOCK), rear = pairIsAlive(rl, rr, MIC_BLOCK);
    if (front != hasFrontMics || rear != hasRearMics) { hasFrontMics = front; hasRearMics = rear; dirty = true; }
    if (!front || !doa_is_voice(fl, MIC_BLOCK, &floorRms)) continue;

    float found = NAN, quality = 0;
    if (rear) {
      found = doa_bearing(fl, fr, rl, rr, MIC_BLOCK, MIC_RATE, MIC_WIDTH, MIC_DEPTH, 0.35f, &quality);
    } else {
      // Two microphones tell left from right, but not ahead from behind:
      // the answer is given as if the voice were ahead.
      int lag = (int)ceilf(MIC_WIDTH / DOA_SPEED_OF_SOUND * MIC_RATE) + 1;
      float delay = doa_delay(fr, fl, MIC_BLOCK, lag, &quality);
      float x = delay / MIC_RATE * DOA_SPEED_OF_SOUND / MIC_WIDTH;
      if (quality >= 0.35f) found = asinf(constrain(x, -1.0f, 1.0f)) * 57.29578f;
    }
    if (isnan(found)) continue;
    voiceDeg = found;
    voiceAt = millis();
    dirty = true;
    if (millis() - lastPrint > 300) {
      lastPrint = millis();
      Serial.printf("voice at %.0f degrees (match %.2f)", found, quality);
      if (rear) {
        float q;
        int lagD = (int)ceilf(MIC_DEPTH / DOA_SPEED_OF_SOUND * MIC_RATE) + 1;
        Serial.printf(", front/rear delay %.1f", doa_delay(fl, rl, MIC_BLOCK, lagD + 8, &q));
      }
      Serial.println();
    }
  }
}

// ---- reSpeaker Flex strip --------------------------------------------------

// The strip's chip is set up to have its sound clocked out by the XIAO. It
// is not known whether the chip keeps listening without that clock, so the
// clock is kept running and the sound is thrown away: only the angle the
// chip works out is used.
static void stripClockTask(void *) {
  static int32_t scrap[512];
  for (;;) {
    size_t got = 0;
    i2s_read(I2S_NUM_0, scrap, sizeof(scrap), &got, portMAX_DELAY);
  }
}

static void startStrip() {
  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
  cfg.sample_rate = 16000;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.dma_buf_count = 4;
  cfg.dma_buf_len = 256;
  i2s_pin_config_t pins = {};
  pins.mck_io_num = I2S_PIN_NO_CHANGE;
  pins.bck_io_num = PIN_STRIP_CLOCK;
  pins.ws_io_num = PIN_STRIP_WS;
  pins.data_out_num = I2S_PIN_NO_CHANGE;
  pins.data_in_num = PIN_STRIP_SOUND;
  i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pins);
  xTaskCreatePinnedToCore(stripClockTask, "strip", 4096, NULL, 1, NULL, 1);
}

// Asks the chip where the voice is (its DOA_VALUE: resource 20, command 18).
// The answer is a status byte, then the angle in degrees and a "someone is
// speaking" flag, each two bytes, low byte first.
static void readStrip() {
  Wire.beginTransmission(STRIP_ADDRESS);
  Wire.write(20);
  Wire.write(18 | 0x80);                       // 0x80: read
  Wire.write(5);
  if (Wire.endTransmission() != 0) return;
  if (Wire.requestFrom(STRIP_ADDRESS, (uint8_t)5) < 5) return;
  uint8_t status = Wire.read(), b[4];
  for (int i = 0; i < 4; i++) b[i] = Wire.read();
  if (status != 0) return;
  int angle = (b[0] | (b[1] << 8)) % 360;
  bool speech = (b[2] | (b[3] << 8)) != 0;
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 300) {
    lastPrint = millis();
    Serial.printf("strip says %d degrees, %s\n", angle, speech ? "speech" : "quiet");
  }
  if (!speech) return;
  // A straight row of microphones measures along itself: 0 at one end, 90
  // straight out, 180 at the other end. It cannot tell ahead from behind, so
  // anything it puts behind is folded to the front. Which end is 0 has not
  // been checked: if left and right come out swapped, turn the strip over
  // so its ends change places.
  if (angle > 180) angle = 360 - angle;
  voiceDeg = 90 - angle;
  voiceAt = millis();
  dirty = true;
}

// ---- Reports to the phone --------------------------------------------------

static void put16(uint8_t *at, float value) {
  int16_t v = (int16_t)lroundf(constrain(value, -32767.0f, 32767.0f));
  at[0] = v & 0xFF;
  at[1] = (v >> 8) & 0xFF;
}

static void readMotion() {
  static sh2_SensorValue_t value;
  if (!hasMotion) return;
  if (imu.wasReset()) imu.enableReport(SH2_ARVR_STABILIZED_RV, 100000);
  if (!imu.getSensorEvent(&value) || value.sensorId != SH2_ARVR_STABILIZED_RV) return;
  float qr = value.un.arvrStabilizedRV.real, qi = value.un.arvrStabilizedRV.i,
        qj = value.un.arvrStabilizedRV.j, qk = value.un.arvrStabilizedRV.k;
  // Yaw grows turning right, as the link expects (the chip's grows left).
  yawDeg = -atan2f(2 * (qi * qj + qk * qr), qi * qi - qj * qj - qk * qk + qr * qr) * 57.29578f;
  pitchDeg = asinf(constrain(-2 * (qi * qk - qj * qr), -1.0f, 1.0f)) * 57.29578f;
  rollDeg = atan2f(2 * (qj * qk + qi * qr), -qi * qi - qj * qj + qk * qk + qr * qr) * 57.29578f;
}

static void sendReport() {
  if (!sensorPort || sensorPort->getSubscribedCount() == 0) return;
  // A phone only numbers speakers by place when it knows where the head
  // points; without a motion sensor nothing useful can be said.
  if (!hasMotion && !hasFrontMics && !hasStrip) return;
  uint8_t report[12];
  put16(report, yawDeg * 100);
  put16(report + 2, pitchDeg * 100);
  put16(report + 4, rollDeg * 100);
  report[6] = 0xFF; report[7] = 0x7F;          // no voice
  report[8] = 0xFF; report[9] = 0x7F;          // elevation unknown
  report[10] = 0; report[11] = 0;              // distance unknown
  float voice = voiceDeg;
  if (millis() - voiceAt < 400 && !isnan(voice)) {
    float bearings[4];
    int n = (millis() - facesAt < 1500) ? faceCount : 0;
    for (int i = 0; i < n; i++) bearings[i] = faceBearing[i];
    int face = doa_match_face(voice, bearings, n, 15.0f);
    // A face in that direction is a better fix than sound alone.
    put16(report + 6, (face >= 0 ? faceBearing[face] : voice) * 100);
    if (face >= 0) {
      put16(report + 8, faceElevation[face] * 100);
      uint16_t cm = (uint16_t)constrain(faceDistance[face] * 100.0f, 0.0f, 65535.0f);
      report[10] = cm & 0xFF;
      report[11] = cm >> 8;
    }
  }
  sensorPort->setValue(report, sizeof(report));
  sensorPort->notify();
}

// ---- Start, and the loop ---------------------------------------------------

static bool onBus(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

void setup() {
  Serial.begin(115200);
  Wire.begin(PIN_SDA, PIN_SCL);
  hasStrip = onBus(STRIP_ADDRESS);
  // The strip's chip has not been checked at the faster speed.
  Wire.setClock(hasStrip ? 100000 : 400000);

  hasScreen = onBus(0x3C);
  if (hasScreen) oled.begin();
  hasMotion = (onBus(0x4A) && imu.begin_I2C(0x4A)) || (onBus(0x4B) && imu.begin_I2C(0x4B));
  if (hasMotion) imu.enableReport(SH2_ARVR_STABILIZED_RV, 100000);
  // A reSpeaker Flex board has no camera and brings its own microphones.
  hasCamera = !hasStrip && startCamera();

  if (hasStrip) {
    startStrip();
  } else {
    startMics();
    xTaskCreatePinnedToCore(soundTask, "sound", 8192, NULL, 2, NULL, 1);
  }
  if (hasCamera) xTaskCreatePinnedToCore(cameraTask, "camera", 16384, NULL, 1, NULL, 0);
  startLink();

  Serial.printf("HearAura Glasses. screen:%d motion:%d camera:%d strip:%d\n", hasScreen, hasMotion, hasCamera, hasStrip);
  draw();
}

void loop() {
  static unsigned long lastReport = 0, lastDraw = 0;
  readMotion();
  if (millis() - lastReport >= 100) {
    lastReport = millis();
    if (hasStrip) readStrip();
    sendReport();
  }
  // The status page refreshes on its own so "voice: left" comes and goes.
  if (dirty || millis() - lastDraw > 500) { dirty = false; lastDraw = millis(); draw(); }
  delay(5);
}
