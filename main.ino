#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <FastLED.h>
#include <esp_task_wdt.h>
#include <numeric>
#include <DNSServer.h>

#define DEBUG false

const esp_task_wdt_config_t wdt_config = {
  .timeout_ms = 10000,
  .idle_core_mask = 0,
  .trigger_panic = true
};

#define MAIN_SWITCH_PIN 26
#define LED1_DATA_PIN 33
#define LED2_DATA_PIN 5
#define LED1_SENSOR_START_PIN 17
#define LED1_SENSOR_END_PIN 16
#define LED2_SENSOR_START_PIN 32
#define LED2_SENSOR_END_PIN 27

enum class TrackState {
  OFF = 0,
  WIPE_IN,
  EFFECT,
  WIPE_OUT
};

enum class LedType {
  NONE,
  WS2811,
  WS2812,
  WS2812B,
  WS2813,
  SK6812,
  WS2811_WHITE
};

enum class ColorOrder {
  RGB,
  GRB,
  BGR,
  RBG,
  GBR,
  BRG
};

struct Track {
  TrackState state = TrackState::OFF;
  bool reverse = false;
  unsigned long lastUpdate = 0;
  uint16_t step = 0;
  unsigned long effectStartTime = 0;
};

struct StepSegment {
  TrackState state;
  bool reverse;
  unsigned long lastUpdate;
  uint16_t step;
  unsigned long effectStartTime;
  int indexStart;
  int indexCount;
};

WebServer server(80);
DNSServer dnsServer;
JsonDocument configDoc;
int maxCurrent = 2000;

CRGB* leds1 = nullptr;
CRGB* leds2 = nullptr;
int gNumLeds1 = 0;
int gNumLeds2 = 0;

LedType gLed1Type = LedType::NONE;
LedType gLed2Type = LedType::NONE;
ColorOrder gLed1ColorOrder = ColorOrder::GRB;
ColorOrder gLed2ColorOrder = ColorOrder::GRB;

int gBrojStepenica1 = 3;
int gDefaultLedsPerStep1 = 30;
bool gVariableSteps1 = false;
int* gStepLengths1 = nullptr;
int gTotalLedsStepenice1 = 0;
StepSegment* stepsArray1 = nullptr;

int gBrojStepenica2 = 3;
int gDefaultLedsPerStep2 = 30;
bool gVariableSteps2 = false;
int* gStepLengths2 = nullptr;
int gTotalLedsStepenice2 = 0;
StepSegment* stepsArray2 = nullptr;

String gInstallationType1 = "linija";
String gEfektKreniOd1 = "sredina";
bool gRotacijaStepenica1 = false;
String gStepeniceMode1 = "allAtOnce";

String gInstallationType2 = "linija";
String gEfektKreniOd2 = "sredina";
bool gRotacijaStepenica2 = false;
String gStepeniceMode2 = "allAtOnce";

uint8_t gEffect1 = 0;
uint8_t gEffect2 = 0;
bool gWipeAll1 = true;
bool gWipeAll2 = true;
uint16_t gOnTimeSec1 = 2;
uint16_t gWipeInSpeedMs1 = 50;
uint16_t gWipeOutSpeedMs1 = 50;
uint8_t gColorR1 = 0, gColorG1 = 42, gColorB1 = 255;
uint8_t gBrightness1 = 100;

uint16_t gOnTimeSec2 = 2;
uint16_t gWipeInSpeedMs2 = 50;
uint16_t gWipeOutSpeedMs2 = 50;
uint8_t gColorR2 = 0, gColorG2 = 42, gColorB2 = 255;
uint8_t gBrightness2 = 100;

String gMainSwitch = "both";

bool gManualOverride = false;
unsigned long gLastButtonDebounce = 0;
bool gLastButtonState = HIGH;
const unsigned long BTN_DEBOUNCE_MS = 250;

Track track1, track2;
volatile bool sensorTriggered[4] = { false, false, false, false };
unsigned long gIrLast[4] = { 0, 0, 0, 0 };
const unsigned long IR_DEBOUNCE = 500;

static uint8_t sHue1 = 0, sHue2 = 0;
static uint16_t sMet1 = 0, sMet2 = 0;
static int sFadeVal1 = 0, sFadeDir1 = 1;
static int sFadeVal2 = 0, sFadeDir2 = 1;

static bool seqActive1 = false;
static bool seqForward1 = true;
static int seqState1 = 0;
static int seqCurrentStep1 = 0;
static unsigned long seqWaitStart1 = 0;

static bool seqActive2 = false;
static bool seqForward2 = true;
static int seqState2 = 0;
static int seqCurrentStep2 = 0;
static unsigned long seqWaitStart2 = 0;

bool isAPMode = false;
unsigned long apStartTime = 0;
const unsigned long AP_TIMEOUT_MS = 300000;

void nonBlockingDelay(unsigned long ms) {
  unsigned long start = millis();
  while (millis() - start < ms) {
    server.handleClient();
    esp_task_wdt_reset();
    delay(1);
  }
}

void signalAPActive() {
  if (leds1 && gLed1Type != LedType::NONE) {
    for (int i = 0; i < 3; i++) {
      for (int j = 0; j < 10 && j < gNumLeds1; j++) leds1[j] = CRGB::White;
      FastLED.show();
      delay(200);
      for (int j = 0; j < 10 && j < gNumLeds1; j++) leds1[j] = CRGB::Black;
      FastLED.show();
      delay(200);
    }
  }
  if (leds2 && gLed2Type != LedType::NONE) {
    for (int i = 0; i < 3; i++) {
      for (int j = 0; j < 10 && j < gNumLeds2; j++) leds2[j] = CRGB::White;
      FastLED.show();
      delay(200);
      for (int j = 0; j < 10 && j < gNumLeds2; j++) leds2[j] = CRGB::Black;
      FastLED.show();
      delay(200);
    }
  }
  if (leds1 && gLed1Type != LedType::NONE) fill_solid(leds1, gNumLeds1, CRGB::Black);
  if (leds2 && gLed2Type != LedType::NONE) fill_solid(leds2, gNumLeds2, CRGB::Black);
  FastLED.show();
}

void applyLightningEffect(CRGB* arr, int count) {
  fill_solid(arr, count, CRGB::Black);
  int flashes = random(2, 4);
  for (int f = 0; f < flashes; f++) {
    int start = random(count);
    int length = random(10, 30);
    for (int i = 0; i < length && (start + i) < count; i++) arr[start + i] = CRGB::White;
    FastLED.show();
    delay(random(30, 80));
    for (int i = 0; i < length && (start + i) < count; i++) arr[start + i] = CRGB::Black;
    FastLED.show();
    delay(random(60, 150));
  }
}

void applyGlobalEffect(CRGB* arr, int count, bool isLed1) {
  if (!arr || count <= 0) return;
  uint8_t effect = isLed1 ? gEffect1 : gEffect2;
  uint8_t r = isLed1 ? gColorR1 : gColorR2;
  uint8_t g = isLed1 ? gColorG1 : gColorG2;
  uint8_t b = isLed1 ? gColorB1 : gColorB2;
  uint8_t brightness = map(isLed1 ? gBrightness1 : gBrightness2, 0, 100, 0, 255);

  switch (effect) {
    case 0:
      fill_solid(arr, count, CRGB(r, g, b).nscale8(brightness));
      break;
    case 1:
      for (int i = 0; i < count; i++) arr[i].nscale8(200);
      arr[random(count)] = CHSV(random8(), 200, 255);
      for (int i = 0; i < count; i++) arr[i].nscale8(brightness);
      break;
    case 2:
      for (int i = 0; i < count; i++) {
        CHSV hsv((isLed1 ? sHue1 : sHue2) + i * 2, 255, 255);
        arr[i] = hsv;
        arr[i].nscale8(brightness);
      }
      if (isLed1) sHue1++;
      else sHue2++;
      break;
    case 3:
      for (int i = 0; i < count; i++) arr[i].fadeToBlackBy(40);
      if (isLed1) {
        CHSV hsv(30, 255, 255);
        arr[sMet1] = hsv;
        arr[sMet1].nscale8(brightness);
        sMet1 = (sMet1 + 1) % count;
      } else {
        CHSV hsv(180, 255, 255);
        arr[sMet2] = hsv;
        arr[sMet2].nscale8(brightness);
        sMet2 = (sMet2 + 1) % count;
      }
      break;
    case 4:
      if (isLed1) {
        sFadeVal1 += (5 * sFadeDir1);
        if (sFadeVal1 > 255) {
          sFadeVal1 = 255;
          sFadeDir1 = -1;
        }
        if (sFadeVal1 < 0) {
          sFadeVal1 = 0;
          sFadeDir1 = 1;
        }
        CHSV hsv(160, 200, 255);
        CRGB rgb = hsv;
        rgb.nscale8_video((uint8_t)sFadeVal1);
        rgb.nscale8(brightness);
        fill_solid(arr, count, rgb);
      } else {
        sFadeVal2 += (5 * sFadeDir2);
        if (sFadeVal2 > 255) {
          sFadeVal2 = 255;
          sFadeDir2 = -1;
        }
        if (sFadeVal2 < 0) {
          sFadeVal2 = 0;
          sFadeDir2 = 1;
        }
        CHSV hsv(60, 200, 255);
        CRGB rgb = hsv;
        rgb.nscale8_video((uint8_t)sFadeVal2);
        rgb.nscale8(brightness);
        fill_solid(arr, count, rgb);
      }
      break;
    case 5:
      for (int i = 0; i < count; i++) arr[i] = CRGB(random(150, 256), random(0, 100), 0).nscale8(brightness);
      break;
    case 6:
      applyLightningEffect(arr, count);
      for (int i = 0; i < count; i++) arr[i].nscale8(brightness);
      break;
    default:
      fill_solid(arr, count, CRGB(r, g, b).nscale8(brightness));
      break;
  }
}

void updateTrack_line(Track& trk, CRGB* arr, int count, bool isLed1) {
  if (!arr || count <= 0) return;
  switch (trk.state) {
    case TrackState::OFF: break;
    case TrackState::WIPE_IN: updateWipeIn_line(trk, arr, count, isLed1); break;
    case TrackState::EFFECT:
      applyGlobalEffect(arr, count, isLed1);
      updateEffect_line(trk, isLed1);
      break;
    case TrackState::WIPE_OUT: updateWipeOut_line(trk, arr, count, isLed1); break;
  }
}

void updateWipeIn_line(Track& trk, CRGB* arr, int count, bool isLed1) {
  unsigned long now = millis();
  if (now - trk.lastUpdate < (isLed1 ? gWipeInSpeedMs1 : gWipeInSpeedMs2)) return;
  trk.lastUpdate += (isLed1 ? gWipeInSpeedMs1 : gWipeInSpeedMs2);

  CRGB wipeColor = (isLed1 ? gEffect1 : gEffect2) == 0 ? CRGB(isLed1 ? gColorR1 : gColorR2, isLed1 ? gColorG1 : gColorG2, isLed1 ? gColorB1 : gColorB2) : CHSV(isLed1 ? sHue1++ : sHue2++, 255, 255);
  wipeColor.nscale8(map(isLed1 ? gBrightness1 : gBrightness2, 0, 100, 0, 255));

  int idx = trk.reverse ? (count - 1 - trk.step) : trk.step;
  if (idx >= 0 && idx < count) {
    if (trk.reverse)
      for (int i = count - 1; i >= idx; i--) arr[i] = wipeColor;
    else
      for (int i = 0; i <= idx; i++) arr[i] = wipeColor;
    trk.step++;
  } else {
    trk.state = TrackState::EFFECT;
    trk.effectStartTime = now;
  }
}

void updateEffect_line(Track& trk, bool isLed1) {
  unsigned long now = millis();
  if ((now - trk.effectStartTime) >= ((isLed1 ? gOnTimeSec1 : gOnTimeSec2) * 1000UL)) {
    if (isLed1 ? gWipeAll1 : gWipeAll2) {
      trk.state = TrackState::WIPE_OUT;
      trk.lastUpdate = now;
      trk.step = 0;
    } else trk.state = TrackState::OFF;
  }
}

void updateWipeOut_line(Track& trk, CRGB* arr, int count, bool isLed1) {
  unsigned long now = millis();
  if (now - trk.lastUpdate < (isLed1 ? gWipeOutSpeedMs1 : gWipeOutSpeedMs2)) return;
  trk.lastUpdate += (isLed1 ? gWipeOutSpeedMs1 : gWipeOutSpeedMs2);

  int idx = trk.reverse ? trk.step : (count - 1 - trk.step);
  if (idx >= 0 && idx < count) {
    if (trk.reverse)
      for (int i = 0; i <= idx; i++) arr[i] = CRGB::Black;
    else
      for (int i = count - 1; i >= idx; i--) arr[i] = CRGB::Black;
    trk.step++;
  } else trk.state = TrackState::OFF;
}

void initStepSegments(int ledNum) {
  int gBrojStepenica = (ledNum == 1) ? gBrojStepenica1 : gBrojStepenica2;
  int gDefaultLedsPerStep = (ledNum == 1) ? gDefaultLedsPerStep1 : gDefaultLedsPerStep2;
  bool gVariableSteps = (ledNum == 1) ? gVariableSteps1 : gVariableSteps2;
  int* gStepLengths = (ledNum == 1) ? gStepLengths1 : gStepLengths2;
  StepSegment*& stepsArray = (ledNum == 1) ? stepsArray1 : stepsArray2;
  int& gTotalLedsStepenice = (ledNum == 1) ? gTotalLedsStepenice1 : gTotalLedsStepenice2;
  CRGB*& leds = (ledNum == 1) ? leds1 : leds2;
  LedType gLedType = (ledNum == 1) ? gLed1Type : gLed2Type;
  ColorOrder gColorOrder = (ledNum == 1) ? gLed1ColorOrder : gLed2ColorOrder;
  bool gRotacijaStepenica = (ledNum == 1) ? gRotacijaStepenica1 : gRotacijaStepenica2;

  if (gBrojStepenica <= 0 || gDefaultLedsPerStep <= 0) return;

  gTotalLedsStepenice = gVariableSteps && gStepLengths ? std::accumulate(gStepLengths, gStepLengths + gBrojStepenica, 0) : gBrojStepenica * gDefaultLedsPerStep;

  if (stepsArray) {
    delete[] stepsArray;
    stepsArray = nullptr;
  }
  stepsArray = new StepSegment[gBrojStepenica];
  if (!stepsArray) {
    if (DEBUG) Serial.println("Failed to allocate stepsArray");
    return;
  }

  int currentIndex = 0;
  for (int i = 0; i < gBrojStepenica; i++) {
    stepsArray[i] = { TrackState::OFF, gRotacijaStepenica && (i % 2 == 1), 0, 0, 0, currentIndex,
                      gVariableSteps ? gStepLengths[i] : gDefaultLedsPerStep };
    currentIndex += stepsArray[i].indexCount;
  }

  if (leds) {
    delete[] leds;
    leds = nullptr;
  }
  leds = new CRGB[gTotalLedsStepenice];
  if (!leds) {
    delete[] stepsArray;
    stepsArray = nullptr;
    gTotalLedsStepenice = 0;
    if (DEBUG) Serial.println("Failed to allocate leds");
    return;
  }

  if (ledNum == 1) {
    switch (gLedType) {
      case LedType::WS2811:
        switch (gColorOrder) {
          case ColorOrder::RGB: FastLED.addLeds<WS2811, LED1_DATA_PIN, RGB>(leds, gTotalLedsStepenice); break;
          case ColorOrder::GRB: FastLED.addLeds<WS2811, LED1_DATA_PIN, GRB>(leds, gTotalLedsStepenice); break;
          case ColorOrder::BGR: FastLED.addLeds<WS2811, LED1_DATA_PIN, BGR>(leds, gTotalLedsStepenice); break;
          case ColorOrder::RBG: FastLED.addLeds<WS2811, LED1_DATA_PIN, RBG>(leds, gTotalLedsStepenice); break;
          case ColorOrder::GBR: FastLED.addLeds<WS2811, LED1_DATA_PIN, GBR>(leds, gTotalLedsStepenice); break;
          case ColorOrder::BRG: FastLED.addLeds<WS2811, LED1_DATA_PIN, BRG>(leds, gTotalLedsStepenice); break;
        }
        break;
      case LedType::WS2812:
        switch (gColorOrder) {
          case ColorOrder::RGB: FastLED.addLeds<WS2812, LED1_DATA_PIN, RGB>(leds, gTotalLedsStepenice); break;
          case ColorOrder::GRB: FastLED.addLeds<WS2812, LED1_DATA_PIN, GRB>(leds, gTotalLedsStepenice); break;
          case ColorOrder::BGR: FastLED.addLeds<WS2812, LED1_DATA_PIN, BGR>(leds, gTotalLedsStepenice); break;
          case ColorOrder::RBG: FastLED.addLeds<WS2812, LED1_DATA_PIN, RBG>(leds, gTotalLedsStepenice); break;
          case ColorOrder::GBR: FastLED.addLeds<WS2812, LED1_DATA_PIN, GBR>(leds, gTotalLedsStepenice); break;
          case ColorOrder::BRG: FastLED.addLeds<WS2812, LED1_DATA_PIN, BRG>(leds, gTotalLedsStepenice); break;
        }
        break;
      case LedType::WS2812B:
        switch (gColorOrder) {
          case ColorOrder::RGB: FastLED.addLeds<WS2812B, LED1_DATA_PIN, RGB>(leds, gTotalLedsStepenice); break;
          case ColorOrder::GRB: FastLED.addLeds<WS2812B, LED1_DATA_PIN, GRB>(leds, gTotalLedsStepenice); break;
          case ColorOrder::BGR: FastLED.addLeds<WS2812B, LED1_DATA_PIN, BGR>(leds, gTotalLedsStepenice); break;
          case ColorOrder::RBG: FastLED.addLeds<WS2812B, LED1_DATA_PIN, RBG>(leds, gTotalLedsStepenice); break;
          case ColorOrder::GBR: FastLED.addLeds<WS2812B, LED1_DATA_PIN, GBR>(leds, gTotalLedsStepenice); break;
          case ColorOrder::BRG: FastLED.addLeds<WS2812B, LED1_DATA_PIN, BRG>(leds, gTotalLedsStepenice); break;
        }
        break;
      case LedType::WS2813:
        switch (gColorOrder) {
          case ColorOrder::RGB: FastLED.addLeds<WS2813, LED1_DATA_PIN, RGB>(leds, gTotalLedsStepenice); break;
          case ColorOrder::GRB: FastLED.addLeds<WS2813, LED1_DATA_PIN, GRB>(leds, gTotalLedsStepenice); break;
          case ColorOrder::BGR: FastLED.addLeds<WS2813, LED1_DATA_PIN, BGR>(leds, gTotalLedsStepenice); break;
          case ColorOrder::RBG: FastLED.addLeds<WS2813, LED1_DATA_PIN, RBG>(leds, gTotalLedsStepenice); break;
          case ColorOrder::GBR: FastLED.addLeds<WS2813, LED1_DATA_PIN, GBR>(leds, gTotalLedsStepenice); break;
          case ColorOrder::BRG: FastLED.addLeds<WS2813, LED1_DATA_PIN, BRG>(leds, gTotalLedsStepenice); break;
        }
        break;
      case LedType::SK6812:
        switch (gColorOrder) {
          case ColorOrder::RGB: FastLED.addLeds<SK6812, LED1_DATA_PIN, RGB>(leds, gTotalLedsStepenice); break;
          case ColorOrder::GRB: FastLED.addLeds<SK6812, LED1_DATA_PIN, GRB>(leds, gTotalLedsStepenice); break;
          case ColorOrder::BGR: FastLED.addLeds<SK6812, LED1_DATA_PIN, BGR>(leds, gTotalLedsStepenice); break;
          case ColorOrder::RBG: FastLED.addLeds<SK6812, LED1_DATA_PIN, RBG>(leds, gTotalLedsStepenice); break;
          case ColorOrder::GBR: FastLED.addLeds<SK6812, LED1_DATA_PIN, GBR>(leds, gTotalLedsStepenice); break;
          case ColorOrder::BRG: FastLED.addLeds<SK6812, LED1_DATA_PIN, BRG>(leds, gTotalLedsStepenice); break;
        }
        break;
      case LedType::WS2811_WHITE: FastLED.addLeds<WS2811, LED1_DATA_PIN, RGB>(leds, gTotalLedsStepenice); break;
      default: FastLED.addLeds<WS2812B, LED1_DATA_PIN, GRB>(leds, gTotalLedsStepenice); break;
    }
    if (DEBUG) Serial.printf("LED1 initialized: %d LEDs\n", gTotalLedsStepenice);
  } else {
    switch (gLedType) {
      case LedType::WS2811:
        switch (gColorOrder) {
          case ColorOrder::RGB: FastLED.addLeds<WS2811, LED2_DATA_PIN, RGB>(leds, gTotalLedsStepenice); break;
          case ColorOrder::GRB: FastLED.addLeds<WS2811, LED2_DATA_PIN, GRB>(leds, gTotalLedsStepenice); break;
          case ColorOrder::BGR: FastLED.addLeds<WS2811, LED2_DATA_PIN, BGR>(leds, gTotalLedsStepenice); break;
          case ColorOrder::RBG: FastLED.addLeds<WS2811, LED2_DATA_PIN, RBG>(leds, gTotalLedsStepenice); break;
          case ColorOrder::GBR: FastLED.addLeds<WS2811, LED2_DATA_PIN, GBR>(leds, gTotalLedsStepenice); break;
          case ColorOrder::BRG: FastLED.addLeds<WS2811, LED2_DATA_PIN, BRG>(leds, gTotalLedsStepenice); break;
        }
        break;
      case LedType::WS2812:
        switch (gColorOrder) {
          case ColorOrder::RGB: FastLED.addLeds<WS2812, LED2_DATA_PIN, RGB>(leds, gTotalLedsStepenice); break;
          case ColorOrder::GRB: FastLED.addLeds<WS2812, LED2_DATA_PIN, GRB>(leds, gTotalLedsStepenice); break;
          case ColorOrder::BGR: FastLED.addLeds<WS2812, LED2_DATA_PIN, BGR>(leds, gTotalLedsStepenice); break;
          case ColorOrder::RBG: FastLED.addLeds<WS2812, LED2_DATA_PIN, RBG>(leds, gTotalLedsStepenice); break;
          case ColorOrder::GBR: FastLED.addLeds<WS2812, LED2_DATA_PIN, GBR>(leds, gTotalLedsStepenice); break;
          case ColorOrder::BRG: FastLED.addLeds<WS2812, LED2_DATA_PIN, BRG>(leds, gTotalLedsStepenice); break;
        }
        break;
      case LedType::WS2812B:
        switch (gColorOrder) {
          case ColorOrder::RGB: FastLED.addLeds<WS2812B, LED2_DATA_PIN, RGB>(leds, gTotalLedsStepenice); break;
          case ColorOrder::GRB: FastLED.addLeds<WS2812B, LED2_DATA_PIN, GRB>(leds, gTotalLedsStepenice); break;
          case ColorOrder::BGR: FastLED.addLeds<WS2812B, LED2_DATA_PIN, BGR>(leds, gTotalLedsStepenice); break;
          case ColorOrder::RBG: FastLED.addLeds<WS2812B, LED2_DATA_PIN, RBG>(leds, gTotalLedsStepenice); break;
          case ColorOrder::GBR: FastLED.addLeds<WS2812B, LED2_DATA_PIN, GBR>(leds, gTotalLedsStepenice); break;
          case ColorOrder::BRG: FastLED.addLeds<WS2812B, LED2_DATA_PIN, BRG>(leds, gTotalLedsStepenice); break;
        }
        break;
      case LedType::WS2813:
        switch (gColorOrder) {
          case ColorOrder::RGB: FastLED.addLeds<WS2813, LED2_DATA_PIN, RGB>(leds, gTotalLedsStepenice); break;
          case ColorOrder::GRB: FastLED.addLeds<WS2813, LED2_DATA_PIN, GRB>(leds, gTotalLedsStepenice); break;
          case ColorOrder::BGR: FastLED.addLeds<WS2813, LED2_DATA_PIN, BGR>(leds, gTotalLedsStepenice); break;
          case ColorOrder::RBG: FastLED.addLeds<WS2813, LED2_DATA_PIN, RBG>(leds, gTotalLedsStepenice); break;
          case ColorOrder::GBR: FastLED.addLeds<WS2813, LED2_DATA_PIN, GBR>(leds, gTotalLedsStepenice); break;
          case ColorOrder::BRG: FastLED.addLeds<WS2813, LED2_DATA_PIN, BRG>(leds, gTotalLedsStepenice); break;
        }
        break;
      case LedType::SK6812:
        switch (gColorOrder) {
          case ColorOrder::RGB: FastLED.addLeds<SK6812, LED2_DATA_PIN, RGB>(leds, gTotalLedsStepenice); break;
          case ColorOrder::GRB: FastLED.addLeds<SK6812, LED2_DATA_PIN, GRB>(leds, gTotalLedsStepenice); break;
          case ColorOrder::BGR: FastLED.addLeds<SK6812, LED2_DATA_PIN, BGR>(leds, gTotalLedsStepenice); break;
          case ColorOrder::RBG: FastLED.addLeds<SK6812, LED2_DATA_PIN, RBG>(leds, gTotalLedsStepenice); break;
          case ColorOrder::GBR: FastLED.addLeds<SK6812, LED2_DATA_PIN, GBR>(leds, gTotalLedsStepenice); break;
          case ColorOrder::BRG: FastLED.addLeds<SK6812, LED2_DATA_PIN, BRG>(leds, gTotalLedsStepenice); break;
        }
        break;
      case LedType::WS2811_WHITE: FastLED.addLeds<WS2811, LED2_DATA_PIN, RGB>(leds, gTotalLedsStepenice); break;
      default: FastLED.addLeds<WS2812B, LED2_DATA_PIN, GRB>(leds, gTotalLedsStepenice); break;
    }
    if (DEBUG) Serial.printf("LED2 initialized: %d LEDs\n", gTotalLedsStepenice);
  }
  FastLED.clear(true);
}

void updateWipeIn_step(StepSegment& seg, CRGB* arr, bool isLed1) {
  unsigned long now = millis();
  if (now - seg.lastUpdate < (isLed1 ? gWipeInSpeedMs1 : gWipeInSpeedMs2)) return;
  seg.lastUpdate += (isLed1 ? gWipeInSpeedMs1 : gWipeInSpeedMs2);

  CRGB wipeColor = (isLed1 ? gEffect1 : gEffect2) == 0 ? CRGB(isLed1 ? gColorR1 : gColorR2, isLed1 ? gColorG1 : gColorG2, isLed1 ? gColorB1 : gColorB2) : CHSV(isLed1 ? sHue1++ : sHue2++, 255, 255);
  wipeColor.nscale8(map(isLed1 ? gBrightness1 : gBrightness2, 0, 100, 0, 255));

  if ((isLed1 ? gEfektKreniOd1 : gEfektKreniOd2) == "sredina") {
    int mid = seg.indexCount / 2;
    int leftIndex = mid - seg.step - 1;
    int rightIndex = mid + seg.step;
    if (leftIndex >= 0) arr[seg.indexStart + leftIndex] = wipeColor;
    if (rightIndex < seg.indexCount) arr[seg.indexStart + rightIndex] = wipeColor;
    seg.step++;
    if (leftIndex < 0 && rightIndex >= seg.indexCount) {
      seg.state = TrackState::EFFECT;
      seg.effectStartTime = now;
    }
  } else {
    int idx = seg.reverse ? (seg.indexCount - 1 - seg.step) : seg.step;
    if (idx >= 0 && idx < seg.indexCount) {
      arr[seg.indexStart + idx] = wipeColor;
      seg.step++;
    } else {
      seg.state = TrackState::EFFECT;
      seg.effectStartTime = now;
    }
  }
}

void updateEffect_step(StepSegment& seg, bool isLed1) {
  if ((isLed1 ? gStepeniceMode1 : gStepeniceMode2) == "sequence") return;
  unsigned long now = millis();
  if ((now - seg.effectStartTime) >= ((isLed1 ? gOnTimeSec1 : gOnTimeSec2) * 1000UL)) {
    if (isLed1 ? gWipeAll1 : gWipeAll2) {
      seg.state = TrackState::WIPE_OUT;
      seg.lastUpdate = now;
      seg.step = 0;
    } else seg.state = TrackState::OFF;
  }
}

void updateWipeOut_step(StepSegment& seg, CRGB* arr, bool isLed1) {
  unsigned long now = millis();
  if (now - seg.lastUpdate < (isLed1 ? gWipeOutSpeedMs1 : gWipeOutSpeedMs2)) return;
  seg.lastUpdate += (isLed1 ? gWipeOutSpeedMs1 : gWipeOutSpeedMs2);

  if ((isLed1 ? gEfektKreniOd1 : gEfektKreniOd2) == "sredina") {
    int mid = seg.indexCount / 2;
    int leftIndex = mid - seg.step - 1;
    int rightIndex = mid + seg.step;
    if (leftIndex >= 0) arr[seg.indexStart + leftIndex] = CRGB::Black;
    if (rightIndex < seg.indexCount) arr[seg.indexStart + rightIndex] = CRGB::Black;
    seg.step++;
    if (leftIndex < 0 && rightIndex >= seg.indexCount) seg.state = TrackState::OFF;
  } else {
    int idx = seg.reverse ? seg.step : (seg.indexCount - 1 - seg.step);
    if (idx >= 0 && idx < seg.indexCount) {
      arr[seg.indexStart + idx] = CRGB::Black;
      seg.step++;
    } else seg.state = TrackState::OFF;
  }
}

void applyEffectSegment(StepSegment& seg, CRGB* arr, bool isLed1) {
  int start = seg.indexStart;
  int count = seg.indexCount;
  uint8_t brightness = map(isLed1 ? gBrightness1 : gBrightness2, 0, 100, 0, 255);

  if ((isLed1 ? gEffect1 : gEffect2) == 0) {
    CRGB color(isLed1 ? gColorR1 : gColorR2, isLed1 ? gColorG1 : gColorG2, isLed1 ? gColorB1 : gColorB2);
    color.nscale8(brightness);
    for (int i = 0; i < count; i++) arr[start + i] = color;
  } else {
    static CRGB temp[2048];
    if (count > 2048) return;
    fill_solid(temp, count, CRGB::Black);
    applyGlobalEffect(temp, count, isLed1);
    for (int i = 0; i < count; i++) arr[start + i] = temp[i];
  }
}

void updateSegment(StepSegment& seg, CRGB* arr, bool isLed1) {
  if (!arr) return;
  switch (seg.state) {
    case TrackState::OFF: break;
    case TrackState::WIPE_IN: updateWipeIn_step(seg, arr, isLed1); break;
    case TrackState::EFFECT:
      applyEffectSegment(seg, arr, isLed1);
      updateEffect_step(seg, isLed1);
      break;
    case TrackState::WIPE_OUT: updateWipeOut_step(seg, arr, isLed1); break;
  }
}

void updateAllStepSegments(int ledNum) {
  StepSegment* stepsArray = (ledNum == 1) ? stepsArray1 : stepsArray2;
  CRGB* leds = (ledNum == 1) ? leds1 : leds2;
  int gBrojStepenica = (ledNum == 1) ? gBrojStepenica1 : gBrojStepenica2;
  int gTotalLedsStepenice = (ledNum == 1) ? gTotalLedsStepenice1 : gTotalLedsStepenice2;
  if (!stepsArray || !leds || gTotalLedsStepenice <= 0) return;
  for (int i = 0; i < gBrojStepenica; i++) updateSegment(stepsArray[i], leds, ledNum == 1);
}

void startSequence(bool forward, int ledNum) {
  StepSegment* stepsArray = (ledNum == 1) ? stepsArray1 : stepsArray2;
  int gBrojStepenica = (ledNum == 1) ? gBrojStepenica1 : gBrojStepenica2;
  int gTotalLedsStepenice = (ledNum == 1) ? gTotalLedsStepenice1 : gTotalLedsStepenice2;
  if (!stepsArray || gTotalLedsStepenice <= 0) return;

  if (ledNum == 1) {
    seqActive1 = true;
    seqForward1 = forward;
    seqState1 = 0;
    seqWaitStart1 = 0;
    seqCurrentStep1 = forward ? 0 : gBrojStepenica - 1;
    for (int i = 0; i < gBrojStepenica; i++) stepsArray[i] = { TrackState::OFF, stepsArray[i].reverse, millis(), 0, 0, stepsArray[i].indexStart, stepsArray[i].indexCount };
  } else {
    seqActive2 = true;
    seqForward2 = forward;
    seqState2 = 0;
    seqWaitStart2 = 0;
    seqCurrentStep2 = forward ? 0 : gBrojStepenica - 1;
    for (int i = 0; i < gBrojStepenica; i++) stepsArray[i] = { TrackState::OFF, stepsArray[i].reverse, millis(), 0, 0, stepsArray[i].indexStart, stepsArray[i].indexCount };
  }
}

void updateSequence(int ledNum) {
  StepSegment* stepsArray = (ledNum == 1) ? stepsArray1 : stepsArray2;
  CRGB* leds = (ledNum == 1) ? leds1 : leds2;
  int gBrojStepenica = (ledNum == 1) ? gBrojStepenica1 : gBrojStepenica2;
  int gTotalLedsStepenice = (ledNum == 1) ? gTotalLedsStepenice1 : gTotalLedsStepenice2;
  bool& seqActive = (ledNum == 1) ? seqActive1 : seqActive2;
  bool seqForward = (ledNum == 1) ? seqForward1 : seqForward2;
  int& seqState = (ledNum == 1) ? seqState1 : seqState2;
  int& seqCurrentStep = (ledNum == 1) ? seqCurrentStep1 : seqCurrentStep2;
  unsigned long& seqWaitStart = (ledNum == 1) ? seqWaitStart1 : seqWaitStart2;
  uint16_t gOnTimeSec = (ledNum == 1) ? gOnTimeSec1 : gOnTimeSec2;

  if (!stepsArray || !leds || gTotalLedsStepenice <= 0 || !seqActive) return;

  if (seqState == 0) {
    if (seqForward ? seqCurrentStep < gBrojStepenica : seqCurrentStep >= 0) {
      if (stepsArray[seqCurrentStep].state == TrackState::OFF) {
        stepsArray[seqCurrentStep].state = TrackState::WIPE_IN;
        stepsArray[seqCurrentStep].step = 0;
        stepsArray[seqCurrentStep].lastUpdate = millis();
      }
      updateSegment(stepsArray[seqCurrentStep], leds, ledNum == 1);
      if (stepsArray[seqCurrentStep].state == TrackState::EFFECT) seqCurrentStep += seqForward ? 1 : -1;
    } else {
      seqState = 1;
      seqWaitStart = millis();
    }
  } else if (seqState == 1) {
    unsigned long now = millis();
    if ((now - seqWaitStart) >= (gOnTimeSec * 1000UL)) {
      seqState = 2;
      seqCurrentStep = seqForward ? gBrojStepenica - 1 : 0;
    } else {
      for (int i = 0; i < gBrojStepenica; i++)
        if (stepsArray[i].state == TrackState::EFFECT) updateSegment(stepsArray[i], leds, ledNum == 1);
    }
  } else if (seqState == 2) {
    if (seqForward ? seqCurrentStep >= 0 : seqCurrentStep < gBrojStepenica) {
      if (stepsArray[seqCurrentStep].state == TrackState::EFFECT) {
        stepsArray[seqCurrentStep].state = TrackState::WIPE_OUT;
        stepsArray[seqCurrentStep].step = 0;
        stepsArray[seqCurrentStep].lastUpdate = millis();
      }
      updateSegment(stepsArray[seqCurrentStep], leds, ledNum == 1);
      if (stepsArray[seqCurrentStep].state == TrackState::OFF) seqCurrentStep += seqForward ? -1 : 1;
    } else seqActive = false;
  }
}

void handleIrSensors() {
  unsigned long now = millis();

  for (int i = 0; i < 4; i++) {
    if (sensorTriggered[i] && (now - gIrLast[i] > IR_DEBOUNCE)) {
      gIrLast[i] = now;
      sensorTriggered[i] = false;
      bool isLed1 = (i < 2);
      bool isStart = (i == 0 || i == 2);
      Track& track = isLed1 ? track1 : track2;
      String& gInstallationType = isLed1 ? gInstallationType1 : gInstallationType2;
      String& gStepeniceMode = isLed1 ? gStepeniceMode1 : gStepeniceMode2;
      bool& seqActive = isLed1 ? seqActive1 : seqActive2;
      int gTotalLedsStepenice = isLed1 ? gTotalLedsStepenice1 : gTotalLedsStepenice2;
      LedType gLedType = isLed1 ? gLed1Type : gLed2Type;
      bool gWipeAll = isLed1 ? gWipeAll1 : gWipeAll2;
      StepSegment* stepsArray = isLed1 ? stepsArray1 : stepsArray2;
      int gBrojStepenica = isLed1 ? gBrojStepenica1 : gBrojStepenica2;

      if (gLedType == LedType::NONE) continue;

      if (gInstallationType == "linija" && track.state == TrackState::OFF) {
        if (DEBUG) Serial.printf("Starting WIPE_IN for LED%d linija\n", isLed1 ? 1 : 2);
        track.state = TrackState::WIPE_IN;
        track.reverse = !isStart;
        track.lastUpdate = now;
        track.step = 0;
        track.effectStartTime = 0;
      } else if (gInstallationType == "stepenica" && gTotalLedsStepenice > 0) {
        if (gStepeniceMode == "sequence") {
          if (!seqActive) {
            if (DEBUG) Serial.printf("Starting sequence for LED%d stepenica\n", isLed1 ? 1 : 2);
            startSequence(isStart, isLed1 ? 1 : 2);
          }
        } else {
          for (int j = 0; j < gBrojStepenica; j++) {
            if (stepsArray[j].state == TrackState::OFF) {
              if (DEBUG) Serial.printf("Starting WIPE_IN for LED%d stepenica segment %d\n", isLed1 ? 1 : 2, j);
              stepsArray[j].state = gWipeAll ? TrackState::WIPE_IN : TrackState::EFFECT;
              stepsArray[j].step = 0;
              stepsArray[j].lastUpdate = now;
            }
          }
        }
      }
    }
  }
}

void IRAM_ATTR led1SensorStartISR() {
  sensorTriggered[0] = true;
}
void IRAM_ATTR led1SensorEndISR() {
  sensorTriggered[1] = true;
}
void IRAM_ATTR led2SensorStartISR() {
  sensorTriggered[2] = true;
}
void IRAM_ATTR led2SensorEndISR() {
  sensorTriggered[3] = true;
}

bool handleFileRead(String path) {
  if (path.endsWith("/")) path += "index.html";
  if (SPIFFS.exists(path)) {
    File file = SPIFFS.open(path, "r");
    String contentType = path.endsWith(".css") ? "text/css" : path.endsWith(".js")  ? "application/javascript"
                                                            : path.endsWith(".png") ? "image/png"
                                                                                    : "text/html";
    server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}

void handleNotFound() {
  if (!handleFileRead(server.uri())) {
    server.sendHeader("Location", "/index.html", true);
    server.send(302, "text/plain", "Redirecting to configuration page");
  }
}

bool loadConfig() {
  if (!SPIFFS.exists("/config.json")) return false;
  File file = SPIFFS.open("/config.json", "r");
  if (!file) return false;
  DeserializationError error = deserializeJson(configDoc, file);
  file.close();
  return !error;
}

bool saveConfig() {
  File file = SPIFFS.open("/config.json", "w");
  if (!file || serializeJson(configDoc, file) == 0) {
    file.close();
    return false;
  }
  file.close();
  return true;
}

void setDefaultConfig() {
  JsonObject led1 = configDoc["led1"].to<JsonObject>();
  led1["type"] = "ws2812b";
  led1["colorOrder"] = "GRB";
  led1["mode"] = "linija";
  led1["linijaLEDCount"] = 35;
  led1["numSteps"] = 3;
  led1["defaultLedsPerStep"] = 30;
  led1["variable"] = false;
  led1["efektKreniOd"] = "sredina";
  led1["stepMode"] = "allAtOnce";
  led1["effect"] = "solid";
  led1["color"] = "#002aff";
  led1["wipeSpeed"] = 50;
  led1["onTime"] = 2;
  led1["rotation"] = false;
  led1["brightness"] = 100;

  JsonObject led2 = configDoc["led2"].to<JsonObject>();
  led2["type"] = "ws2812b";
  led2["colorOrder"] = "GRB";
  led2["mode"] = "linija";
  led2["linijaLEDCount"] = 35;
  led2["numSteps"] = 3;
  led2["defaultLedsPerStep"] = 30;
  led2["variable"] = false;
  led2["efektKreniOd"] = "sredina";
  led2["stepMode"] = "allAtOnce";
  led2["effect"] = "solid";
  led2["color"] = "#002aff";
  led2["wipeSpeed"] = 50;
  led2["onTime"] = 2;
  led2["rotation"] = false;
  led2["brightness"] = 100;

  configDoc["maxCurrent"] = 2000;
  configDoc["mainSwitch"] = "both";
}

void applyConfigToLEDStrips() {
  auto applyLedConfig = [&](int ledNum) {
    JsonObject ledObj = configDoc[ledNum == 1 ? "led1" : "led2"];
    String typeStr = ledObj["type"] | "ws2812b";
    LedType& gLedType = (ledNum == 1) ? gLed1Type : gLed2Type;
    CRGB*& leds = (ledNum == 1) ? leds1 : leds2;
    int& gNumLeds = (ledNum == 1) ? gNumLeds1 : gNumLeds2;
    String& gInstallationType = (ledNum == 1) ? gInstallationType1 : gInstallationType2;
    ColorOrder& gColorOrder = (ledNum == 1) ? gLed1ColorOrder : gLed2ColorOrder;

    if (typeStr == "off" || typeStr == "") {
      gLedType = LedType::NONE;
      if (leds) {
        delete[] leds;
        leds = nullptr;
      }
      gNumLeds = 0;
      if (ledNum == 1 && stepsArray1) {
        delete[] stepsArray1;
        stepsArray1 = nullptr;
        gTotalLedsStepenice1 = 0;
      }
      if (ledNum == 2 && stepsArray2) {
        delete[] stepsArray2;
        stepsArray2 = nullptr;
        gTotalLedsStepenice2 = 0;
      }
      if (DEBUG) Serial.printf("LED%d set to OFF\n", ledNum);
    } else {
      gLedType = (typeStr == "ws2811") ? LedType::WS2811 : (typeStr == "ws2812")       ? LedType::WS2812
                                                         : (typeStr == "ws2812b")      ? LedType::WS2812B
                                                         : (typeStr == "ws2813")       ? LedType::WS2813
                                                         : (typeStr == "sk6812")       ? LedType::SK6812
                                                         : (typeStr == "ws2811_white") ? LedType::WS2811_WHITE
                                                                                       : LedType::WS2812B;

      String colorOrderStr = ledObj["colorOrder"] | "GRB";
      gColorOrder = (colorOrderStr == "RGB") ? ColorOrder::RGB : (colorOrderStr == "GRB") ? ColorOrder::GRB
                                                               : (colorOrderStr == "BGR") ? ColorOrder::BGR
                                                               : (colorOrderStr == "RBG") ? ColorOrder::RBG
                                                               : (colorOrderStr == "GBR") ? ColorOrder::GBR
                                                               : (colorOrderStr == "BRG") ? ColorOrder::BRG
                                                                                          : ColorOrder::GRB;

      gInstallationType = ledObj["mode"] | "linija";
      if (gInstallationType == "linija") {
        gNumLeds = ledObj["linijaLEDCount"] | 35;
        if (leds) {
          delete[] leds;
          leds = nullptr;
        }
        leds = new CRGB[gNumLeds];
        if (!leds) {
          if (DEBUG) Serial.printf("Failed to allocate leds%d for linija\n", ledNum);
          return;
        }
        if (ledNum == 1) {
          switch (gLedType) {
            case LedType::WS2811:
              switch (gColorOrder) {
                case ColorOrder::RGB: FastLED.addLeds<WS2811, LED1_DATA_PIN, RGB>(leds, gNumLeds); break;
                case ColorOrder::GRB: FastLED.addLeds<WS2811, LED1_DATA_PIN, GRB>(leds, gNumLeds); break;
                case ColorOrder::BGR: FastLED.addLeds<WS2811, LED1_DATA_PIN, BGR>(leds, gNumLeds); break;
                case ColorOrder::RBG: FastLED.addLeds<WS2811, LED1_DATA_PIN, RBG>(leds, gNumLeds); break;
                case ColorOrder::GBR: FastLED.addLeds<WS2811, LED1_DATA_PIN, GBR>(leds, gNumLeds); break;
                case ColorOrder::BRG: FastLED.addLeds<WS2811, LED1_DATA_PIN, BRG>(leds, gNumLeds); break;
              }
              break;
            case LedType::WS2812:
              switch (gColorOrder) {
                case ColorOrder::RGB: FastLED.addLeds<WS2812, LED1_DATA_PIN, RGB>(leds, gNumLeds); break;
                case ColorOrder::GRB: FastLED.addLeds<WS2812, LED1_DATA_PIN, GRB>(leds, gNumLeds); break;
                case ColorOrder::BGR: FastLED.addLeds<WS2812, LED1_DATA_PIN, BGR>(leds, gNumLeds); break;
                case ColorOrder::RBG: FastLED.addLeds<WS2812, LED1_DATA_PIN, RBG>(leds, gNumLeds); break;
                case ColorOrder::GBR: FastLED.addLeds<WS2812, LED1_DATA_PIN, GBR>(leds, gNumLeds); break;
                case ColorOrder::BRG: FastLED.addLeds<WS2812, LED1_DATA_PIN, BRG>(leds, gNumLeds); break;
              }
              break;
            case LedType::WS2812B:
              switch (gColorOrder) {
                case ColorOrder::RGB: FastLED.addLeds<WS2812B, LED1_DATA_PIN, RGB>(leds, gNumLeds); break;
                case ColorOrder::GRB: FastLED.addLeds<WS2812B, LED1_DATA_PIN, GRB>(leds, gNumLeds); break;
                case ColorOrder::BGR: FastLED.addLeds<WS2812B, LED1_DATA_PIN, BGR>(leds, gNumLeds); break;
                case ColorOrder::RBG: FastLED.addLeds<WS2812B, LED1_DATA_PIN, RBG>(leds, gNumLeds); break;
                case ColorOrder::GBR: FastLED.addLeds<WS2812B, LED1_DATA_PIN, GBR>(leds, gNumLeds); break;
                case ColorOrder::BRG: FastLED.addLeds<WS2812B, LED1_DATA_PIN, BRG>(leds, gNumLeds); break;
              }
              break;
            case LedType::WS2813:
              switch (gColorOrder) {
                case ColorOrder::RGB: FastLED.addLeds<WS2813, LED1_DATA_PIN, RGB>(leds, gNumLeds); break;
                case ColorOrder::GRB: FastLED.addLeds<WS2813, LED1_DATA_PIN, GRB>(leds, gNumLeds); break;
                case ColorOrder::BGR: FastLED.addLeds<WS2813, LED1_DATA_PIN, BGR>(leds, gNumLeds); break;
                case ColorOrder::RBG: FastLED.addLeds<WS2813, LED1_DATA_PIN, RBG>(leds, gNumLeds); break;
                case ColorOrder::GBR: FastLED.addLeds<WS2813, LED1_DATA_PIN, GBR>(leds, gNumLeds); break;
                case ColorOrder::BRG: FastLED.addLeds<WS2813, LED1_DATA_PIN, BRG>(leds, gNumLeds); break;
              }
              break;
            case LedType::SK6812:
              switch (gColorOrder) {
                case ColorOrder::RGB: FastLED.addLeds<SK6812, LED1_DATA_PIN, RGB>(leds, gNumLeds); break;
                case ColorOrder::GRB: FastLED.addLeds<SK6812, LED1_DATA_PIN, GRB>(leds, gNumLeds); break;
                case ColorOrder::BGR: FastLED.addLeds<SK6812, LED1_DATA_PIN, BGR>(leds, gNumLeds); break;
                case ColorOrder::RBG: FastLED.addLeds<SK6812, LED1_DATA_PIN, RBG>(leds, gNumLeds); break;
                case ColorOrder::GBR: FastLED.addLeds<SK6812, LED1_DATA_PIN, GBR>(leds, gNumLeds); break;
                case ColorOrder::BRG: FastLED.addLeds<SK6812, LED1_DATA_PIN, BRG>(leds, gNumLeds); break;
              }
              break;
            case LedType::WS2811_WHITE: FastLED.addLeds<WS2811, LED1_DATA_PIN, RGB>(leds, gNumLeds); break;
            default: FastLED.addLeds<WS2812B, LED1_DATA_PIN, GRB>(leds, gNumLeds); break;
          }
        } else {
          switch (gLedType) {
            case LedType::WS2811:
              switch (gColorOrder) {
                case ColorOrder::RGB: FastLED.addLeds<WS2811, LED2_DATA_PIN, RGB>(leds, gNumLeds); break;
                case ColorOrder::GRB: FastLED.addLeds<WS2811, LED2_DATA_PIN, GRB>(leds, gNumLeds); break;
                case ColorOrder::BGR: FastLED.addLeds<WS2811, LED2_DATA_PIN, BGR>(leds, gNumLeds); break;
                case ColorOrder::RBG: FastLED.addLeds<WS2811, LED2_DATA_PIN, RBG>(leds, gNumLeds); break;
                case ColorOrder::GBR: FastLED.addLeds<WS2811, LED2_DATA_PIN, GBR>(leds, gNumLeds); break;
                case ColorOrder::BRG: FastLED.addLeds<WS2811, LED2_DATA_PIN, BRG>(leds, gNumLeds); break;
              }
              break;
            case LedType::WS2812:
              switch (gColorOrder) {
                case ColorOrder::RGB: FastLED.addLeds<WS2812, LED2_DATA_PIN, RGB>(leds, gNumLeds); break;
                case ColorOrder::GRB: FastLED.addLeds<WS2812, LED2_DATA_PIN, GRB>(leds, gNumLeds); break;
                case ColorOrder::BGR: FastLED.addLeds<WS2812, LED2_DATA_PIN, BGR>(leds, gNumLeds); break;
                case ColorOrder::RBG: FastLED.addLeds<WS2812, LED2_DATA_PIN, RBG>(leds, gNumLeds); break;
                case ColorOrder::GBR: FastLED.addLeds<WS2812, LED2_DATA_PIN, GBR>(leds, gNumLeds); break;
                case ColorOrder::BRG: FastLED.addLeds<WS2812, LED2_DATA_PIN, BRG>(leds, gNumLeds); break;
              }
              break;
            case LedType::WS2812B:
              switch (gColorOrder) {
                case ColorOrder::RGB: FastLED.addLeds<WS2812B, LED2_DATA_PIN, RGB>(leds, gNumLeds); break;
                case ColorOrder::GRB: FastLED.addLeds<WS2812B, LED2_DATA_PIN, GRB>(leds, gNumLeds); break;
                case ColorOrder::BGR: FastLED.addLeds<WS2812B, LED2_DATA_PIN, BGR>(leds, gNumLeds); break;
                case ColorOrder::RBG: FastLED.addLeds<WS2812B, LED2_DATA_PIN, RBG>(leds, gNumLeds); break;
                case ColorOrder::GBR: FastLED.addLeds<WS2812B, LED2_DATA_PIN, GBR>(leds, gNumLeds); break;
                case ColorOrder::BRG: FastLED.addLeds<WS2812B, LED2_DATA_PIN, BRG>(leds, gNumLeds); break;
              }
              break;
            case LedType::WS2813:
              switch (gColorOrder) {
                case ColorOrder::RGB: FastLED.addLeds<WS2813, LED2_DATA_PIN, RGB>(leds, gNumLeds); break;
                case ColorOrder::GRB: FastLED.addLeds<WS2813, LED2_DATA_PIN, GRB>(leds, gNumLeds); break;
                case ColorOrder::BGR: FastLED.addLeds<WS2813, LED2_DATA_PIN, BGR>(leds, gNumLeds); break;
                case ColorOrder::RBG: FastLED.addLeds<WS2813, LED2_DATA_PIN, RBG>(leds, gNumLeds); break;
                case ColorOrder::GBR: FastLED.addLeds<WS2813, LED2_DATA_PIN, GBR>(leds, gNumLeds); break;
                case ColorOrder::BRG: FastLED.addLeds<WS2813, LED2_DATA_PIN, BRG>(leds, gNumLeds); break;
              }
              break;
            case LedType::SK6812:
              switch (gColorOrder) {
                case ColorOrder::RGB: FastLED.addLeds<SK6812, LED2_DATA_PIN, RGB>(leds, gNumLeds); break;
                case ColorOrder::GRB: FastLED.addLeds<SK6812, LED2_DATA_PIN, GRB>(leds, gNumLeds); break;
                case ColorOrder::BGR: FastLED.addLeds<SK6812, LED2_DATA_PIN, BGR>(leds, gNumLeds); break;
                case ColorOrder::RBG: FastLED.addLeds<SK6812, LED2_DATA_PIN, RBG>(leds, gNumLeds); break;
                case ColorOrder::GBR: FastLED.addLeds<SK6812, LED2_DATA_PIN, GBR>(leds, gNumLeds); break;
                case ColorOrder::BRG: FastLED.addLeds<SK6812, LED2_DATA_PIN, BRG>(leds, gNumLeds); break;
              }
              break;
            case LedType::WS2811_WHITE: FastLED.addLeds<WS2811, LED2_DATA_PIN, RGB>(leds, gNumLeds); break;
            default: FastLED.addLeds<WS2812B, LED2_DATA_PIN, GRB>(leds, gNumLeds); break;
          }
        }
        if (DEBUG) Serial.printf("LED%d linija mode: %d LEDs\n", ledNum, gNumLeds);
      } else if (gInstallationType == "stepenica") {
        if (ledNum == 1) {
          gBrojStepenica1 = ledObj["numSteps"] | 3;
          gDefaultLedsPerStep1 = ledObj["defaultLedsPerStep"] | 30;
          gVariableSteps1 = ledObj["variable"] | false;
          if (gVariableSteps1 && !ledObj["stepLengths"].isNull()) {
            JsonArray stepLengths = ledObj["stepLengths"];
            if (gStepLengths1) { delete[] gStepLengths1; }
            gStepLengths1 = new int[gBrojStepenica1];
            for (int i = 0; i < gBrojStepenica1 && i < stepLengths.size(); i++) gStepLengths1[i] = stepLengths[i];
          }
          initStepSegments(1);
        } else {
          gBrojStepenica2 = ledObj["numSteps"] | 3;
          gDefaultLedsPerStep2 = ledObj["defaultLedsPerStep"] | 30;
          gVariableSteps2 = ledObj["variable"] | false;
          if (gVariableSteps2 && !ledObj["stepLengths"].isNull()) {
            JsonArray stepLengths = ledObj["stepLengths"];
            if (gStepLengths2) { delete[] gStepLengths2; }
            gStepLengths2 = new int[gBrojStepenica2];
            for (int i = 0; i < gBrojStepenica2 && i < stepLengths.size(); i++) gStepLengths2[i] = stepLengths[i];
          }
          initStepSegments(2);
        }
      }
    }

    if (ledNum == 1) {
      gRotacijaStepenica1 = ledObj["rotation"] | false;
      gEfektKreniOd1 = ledObj["efektKreniOd"] | "sredina";
      gStepeniceMode1 = ledObj["stepMode"] | "allAtOnce";
      gEffect1 = (ledObj["effect"] == "solid") ? 0 : (ledObj["effect"] == "confetti")  ? 1
                                                   : (ledObj["effect"] == "rainbow")   ? 2
                                                   : (ledObj["effect"] == "meteor")    ? 3
                                                   : (ledObj["effect"] == "fade")      ? 4
                                                   : (ledObj["effect"] == "fire")      ? 5
                                                   : (ledObj["effect"] == "lightning") ? 6
                                                                                       : 0;
      String colorStr = ledObj["color"] | "#002aff";
      if (colorStr.length() == 7) {
        gColorR1 = strtol(colorStr.substring(1, 3).c_str(), NULL, 16);
        gColorG1 = strtol(colorStr.substring(3, 5).c_str(), NULL, 16);
        gColorB1 = strtol(colorStr.substring(5, 7).c_str(), NULL, 16);
      }
      gWipeInSpeedMs1 = gWipeOutSpeedMs1 = ledObj["wipeSpeed"] | 50;
      gOnTimeSec1 = ledObj["onTime"] | 2;
      gBrightness1 = ledObj["brightness"] | 100;
    } else {
      gRotacijaStepenica2 = ledObj["rotation"] | false;
      gEfektKreniOd2 = ledObj["efektKreniOd"] | "sredina";
      gStepeniceMode2 = ledObj["stepMode"] | "allAtOnce";
      gEffect2 = (ledObj["effect"] == "solid") ? 0 : (ledObj["effect"] == "confetti")  ? 1
                                                   : (ledObj["effect"] == "rainbow")   ? 2
                                                   : (ledObj["effect"] == "meteor")    ? 3
                                                   : (ledObj["effect"] == "fade")      ? 4
                                                   : (ledObj["effect"] == "fire")      ? 5
                                                   : (ledObj["effect"] == "lightning") ? 6
                                                                                       : 0;
      String colorStr = ledObj["color"] | "#002aff";
      if (colorStr.length() == 7) {
        gColorR2 = strtol(colorStr.substring(1, 3).c_str(), NULL, 16);
        gColorG2 = strtol(colorStr.substring(3, 5).c_str(), NULL, 16);
        gColorB2 = strtol(colorStr.substring(5, 7).c_str(), NULL, 16);
      }
      gWipeInSpeedMs2 = gWipeOutSpeedMs2 = ledObj["wipeSpeed"] | 50;
      gOnTimeSec2 = ledObj["onTime"] | 2;
      gBrightness2 = ledObj["brightness"] | 100;
    }
  };

  applyLedConfig(1);
  applyLedConfig(2);

  maxCurrent = configDoc["maxCurrent"] | 2000;
  gMainSwitch = configDoc["mainSwitch"] | "both";

  FastLED.setMaxPowerInVoltsAndMilliamps(5, maxCurrent);
  FastLED.clear(true);
  if (DEBUG) Serial.printf("Config applied: mainSwitch = %s\n", gMainSwitch.c_str());
}

void handleRoot() {
  handleFileRead("/index.html");
}

void handleGetConfig() {
  String output;
  serializeJson(configDoc, output);
  server.send(200, "application/json", output);
}

void handleSaveConfig() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"No data received\"}");
    return;
  }
  DeserializationError error = deserializeJson(configDoc, server.arg("plain"));
  if (error) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }
  saveConfig();

  if (leds1 && gLed1Type != LedType::NONE) {
    fill_solid(leds1, min(gNumLeds1, 2000), CRGB::Black);
  }
  if (leds2 && gLed2Type != LedType::NONE) {
    fill_solid(leds2, min(gNumLeds2, 2000), CRGB::Black);
  }
  FastLED.show();

  applyConfigToLEDStrips();
  server.send(200, "application/json", "{\"status\":\"ok\"}");
  apStartTime = millis();
}

void setup() {
  if (DEBUG) Serial.begin(115200);
  delay(100);

  esp_task_wdt_init(&wdt_config);
  esp_task_wdt_add(NULL);

  if (!SPIFFS.begin(true)) {
    if (DEBUG) Serial.println("SPIFFS failed to initialize");
    return;
  }

  if (!loadConfig()) {
    if (DEBUG) Serial.println("Config not found, setting defaults");
    setDefaultConfig();
    saveConfig();
  }
  applyConfigToLEDStrips();

  server.on("/", HTTP_GET, handleRoot);
  server.on("/config", HTTP_GET, []() {
    handleFileRead("/index.html");
  });
  server.on("/api/getConfig", HTTP_GET, handleGetConfig);
  server.on("/api/saveConfig", HTTP_POST, handleSaveConfig);
  server.onNotFound(handleNotFound);

  pinMode(MAIN_SWITCH_PIN, INPUT_PULLUP);
  pinMode(LED1_SENSOR_START_PIN, INPUT);
  pinMode(LED1_SENSOR_END_PIN, INPUT);
  pinMode(LED2_SENSOR_START_PIN, INPUT);
  pinMode(LED2_SENSOR_END_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(LED1_SENSOR_START_PIN), led1SensorStartISR, RISING);
  attachInterrupt(digitalPinToInterrupt(LED1_SENSOR_END_PIN), led1SensorEndISR, RISING);
  attachInterrupt(digitalPinToInterrupt(LED2_SENSOR_START_PIN), led2SensorStartISR, RISING);
  attachInterrupt(digitalPinToInterrupt(LED2_SENSOR_END_PIN), led2SensorEndISR, RISING);

  if (DEBUG) Serial.println("Setup complete");
}

void loop() {
  server.handleClient();
  esp_task_wdt_reset();
  handleIrSensors();

  bool reading = digitalRead(MAIN_SWITCH_PIN);
  static unsigned long buttonPressStart = 0;

  if (reading != gLastButtonState) {
    if (millis() - gLastButtonDebounce > BTN_DEBOUNCE_MS) {
      gLastButtonDebounce = millis();
      gLastButtonState = reading;

      if (reading == LOW) {
        buttonPressStart = millis();
      } else if (buttonPressStart > 0) {
        unsigned long pressDuration = millis() - buttonPressStart;
        if (pressDuration < 6000) {
          gManualOverride = !gManualOverride;
          if (DEBUG) Serial.println("Manual override: " + String(gManualOverride ? "ON" : "OFF"));
          if (!gManualOverride) {
            track1.state = track2.state = TrackState::OFF;
            if (stepsArray1)
              for (int i = 0; i < gBrojStepenica1; i++) stepsArray1[i].state = TrackState::OFF;
            if (stepsArray2)
              for (int i = 0; i < gBrojStepenica2; i++) stepsArray2[i].state = TrackState::OFF;
            if (leds1 && gLed1Type != LedType::NONE) fill_solid(leds1, gInstallationType1 == "linija" ? gNumLeds1 : gTotalLedsStepenice1, CRGB::Black);
            if (leds2 && gLed2Type != LedType::NONE) fill_solid(leds2, gInstallationType2 == "linija" ? gNumLeds2 : gTotalLedsStepenice2, CRGB::Black);
            FastLED.show();
            if (DEBUG) Serial.println("LEDs cleared");
          }
        }
        buttonPressStart = 0;
      }
    }
  }

  if (reading == LOW && buttonPressStart > 0) {
    unsigned long pressDuration = millis() - buttonPressStart;
    if (pressDuration >= 6000 && !isAPMode) {
      if (DEBUG) Serial.println("Activating AP mode...");
      WiFi.softAP("StairLight_AP", "12345678");
      server.begin();
      dnsServer.start(53, "*", WiFi.softAPIP());
      isAPMode = true;
      apStartTime = millis();
      signalAPActive();
      if (DEBUG) Serial.println("AP activated at 192.168.4.1");
    }
  }

  if (isAPMode && (millis() - apStartTime >= AP_TIMEOUT_MS)) {
    if (DEBUG) Serial.println("AP timeout, restarting ESP32...");
    WiFi.softAPdisconnect(true);
    dnsServer.stop();
    server.close();
    isAPMode = false;
    ESP.restart();
  }

  if (gManualOverride) {
    if ((gMainSwitch == "both" || gMainSwitch == "led1") && gLed1Type != LedType::NONE) {
      if (gInstallationType1 == "linija" && leds1) applyGlobalEffect(leds1, gNumLeds1, true);
      else if (gTotalLedsStepenice1 > 0 && leds1) applyGlobalEffect(leds1, gTotalLedsStepenice1, true);
    }
    if ((gMainSwitch == "both" || gMainSwitch == "led2") && gLed2Type != LedType::NONE) {
      if (gInstallationType2 == "linija" && leds2) applyGlobalEffect(leds2, gNumLeds2, false);
      else if (gTotalLedsStepenice2 > 0 && leds2) applyGlobalEffect(leds2, gTotalLedsStepenice2, false);
    }
  } else {
    if ((gMainSwitch == "both" || gMainSwitch == "led1") && gLed1Type != LedType::NONE) {
      if (gInstallationType1 == "stepenica" && gTotalLedsStepenice1 > 0) {
        if (gStepeniceMode1 == "sequence") updateSequence(1);
        else updateAllStepSegments(1);
      } else if (gInstallationType1 == "linija" && leds1) {
        updateTrack_line(track1, leds1, gNumLeds1, true);
      }
    }
    if ((gMainSwitch == "both" || gMainSwitch == "led2") && gLed2Type != LedType::NONE) {
      if (gInstallationType2 == "stepenica" && gTotalLedsStepenice2 > 0) {
        if (gStepeniceMode2 == "sequence") updateSequence(2);
        else updateAllStepSegments(2);
      } else if (gInstallationType2 == "linija" && leds2) {
        updateTrack_line(track2, leds2, gNumLeds2, false);
      }
    }
  }

  FastLED.show();
  dnsServer.processNextRequest();
  delay(10);
}
