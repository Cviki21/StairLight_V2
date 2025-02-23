#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <FastLED.h>
#include <time.h>
#include <WiFiManager.h>
#include <ESPmDNS.h>
#include <esp_task_wdt.h>

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
WiFiManager wifiManager;
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

uint16_t gOnTimeSec2 = 2;
uint16_t gWipeInSpeedMs2 = 50;
uint16_t gWipeOutSpeedMs2 = 50;
uint8_t gColorR2 = 0, gColorG2 = 42, gColorB2 = 255;

String gDayStartStr = "08:00";
String gDayEndStr = "20:00";
int gDayBrightnessPercent = 100;
int gNightBrightnessPercent = 30;

String gMainSwitch = "both";

bool gManualOverride = false;
unsigned long gLastButtonDebounce = 0;
bool gLastButtonState = HIGH;
const unsigned long BTN_DEBOUNCE_MS = 250;

Track track1, track2;
volatile bool sensorTriggered[4] = { false, false, false, false };
unsigned long gIrLast[4] = { 0, 0, 0, 0 };
const unsigned long IR_DEBOUNCE = 300;

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

#define DEBUG 1

void nonBlockingDelay(unsigned long ms) {
  unsigned long start = millis();
  while (millis() - start < ms) {
    server.handleClient();
    esp_task_wdt_reset();
    delay(1);
  }
}

void applyGlobalEffect(CRGB* arr, int count, bool isLed1) {
  if (!arr || count <= 0) return;
  uint8_t effect = isLed1 ? gEffect1 : gEffect2;
  uint8_t r = isLed1 ? gColorR1 : gColorR2;
  uint8_t g = isLed1 ? gColorG1 : gColorG2;
  uint8_t b = isLed1 ? gColorB1 : gColorB2;

  if (effect == 0) {
    doSolid(arr, count, r, g, b);
    return;
  }
  switch (effect) {
    case 1:
      {
        for (int i = 0; i < count; i++) arr[i].nscale8(200);
        arr[random(count)] = CHSV(random8(), 200, 255);
      }
      break;
    case 2:
      {
        for (int i = 0; i < count; i++) {
          if (isLed1) arr[i] = CHSV(sHue1 + i * 2, 255, 255);
          else arr[i] = CHSV(sHue2 + i * 2, 255, 255);
        }
        if (isLed1) sHue1++;
        else sHue2++;
      }
      break;
    case 3:
      {
        for (int i = 0; i < count; i++) arr[i].fadeToBlackBy(40);
        if (isLed1) {
          arr[sMet1] = CHSV(30, 255, 255);
          sMet1++;
          if (sMet1 >= count) sMet1 = 0;
        } else {
          arr[sMet2] = CHSV(180, 255, 255);
          sMet2++;
          if (sMet2 >= count) sMet2 = 0;
        }
      }
      break;
    case 4:
      {
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
          CRGB c = CHSV(160, 200, 255);
          c.nscale8_video((uint8_t)sFadeVal1);
          for (int i = 0; i < count; i++) arr[i] = c;
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
          CRGB c = CHSV(60, 200, 255);
          c.nscale8_video((uint8_t)sFadeVal2);
          for (int i = 0; i < count; i++) arr[i] = c;
        }
      }
      break;
    case 5:
      {
        for (int i = 0; i < count; i++) {
          uint8_t rr = random(150, 256);
          uint8_t gg = random(0, 100);
          arr[i] = CRGB(rr, gg, 0);
        }
      }
      break;
    case 6:
      {
        for (int i = 0; i < count; i++) arr[i] = CRGB::Black;
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
      break;
    default: doSolid(arr, count, r, g, b); break;
  }
}

void doSolid(CRGB* arr, int count, uint8_t r, uint8_t g, uint8_t b) {
  if (!arr || count <= 0) return;
  for (int i = 0; i < count; i++) arr[i] = CRGB(r, g, b);
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

  CRGB wipeColor;
  if ((isLed1 ? gEffect1 : gEffect2) == 0) wipeColor = CRGB(isLed1 ? gColorR1 : gColorR2, isLed1 ? gColorG1 : gColorG2, isLed1 ? gColorB1 : gColorB2);
  else {
    wipeColor = CHSV(isLed1 ? sHue1 : sHue2, 255, 255);
    if (isLed1) sHue1++;
    else sHue2++;
  }
  int idx = trk.reverse ? (count - 1 - trk.step) : trk.step;
  if (idx >= 0 && idx < count) {
    arr[idx] = wipeColor;
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
    } else {
      trk.state = TrackState::OFF;
    }
  }
}

void updateWipeOut_line(Track& trk, CRGB* arr, int count, bool isLed1) {
  unsigned long now = millis();
  if (now - trk.lastUpdate < (isLed1 ? gWipeOutSpeedMs1 : gWipeOutSpeedMs2)) return;
  trk.lastUpdate += (isLed1 ? gWipeOutSpeedMs1 : gWipeOutSpeedMs2);

  int idx = trk.reverse ? trk.step : (count - 1 - trk.step);
  if (idx >= 0 && idx < count) {
    arr[idx] = CRGB::Black;
    trk.step++;
  } else {
    trk.state = TrackState::OFF;
  }
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

  gTotalLedsStepenice = 0;
  if (gVariableSteps && gStepLengths) {
    for (int i = 0; i < gBrojStepenica; i++) {
      if (gStepLengths[i] <= 0) return;
      gTotalLedsStepenice += gStepLengths[i];
    }
  } else {
    gTotalLedsStepenice = gBrojStepenica * gDefaultLedsPerStep;
  }

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
    stepsArray[i].state = TrackState::OFF;
    stepsArray[i].reverse = (gRotacijaStepenica && (i % 2 == 1));
    stepsArray[i].lastUpdate = 0;
    stepsArray[i].step = 0;
    stepsArray[i].effectStartTime = 0;
    stepsArray[i].indexStart = currentIndex;
    stepsArray[i].indexCount = gVariableSteps ? gStepLengths[i] : gDefaultLedsPerStep;
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
      case LedType::WS2811_WHITE:
        FastLED.addLeds<WS2811, LED1_DATA_PIN, RGB>(leds, gTotalLedsStepenice);
        break;
      default:
        FastLED.addLeds<WS2812B, LED1_DATA_PIN, GRB>(leds, gTotalLedsStepenice);
        break;
    }
    if (DEBUG) Serial.printf("LED1 initialized with %d LEDs on pin %d\n", gTotalLedsStepenice, LED1_DATA_PIN);
  } else if (ledNum == 2) {
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
      case LedType::WS2811_WHITE:
        FastLED.addLeds<WS2811, LED2_DATA_PIN, RGB>(leds, gTotalLedsStepenice);
        break;
      default:
        FastLED.addLeds<WS2812B, LED2_DATA_PIN, GRB>(leds, gTotalLedsStepenice);
        break;
    }
    if (DEBUG) Serial.printf("LED2 initialized with %d LEDs on pin %d\n", gTotalLedsStepenice, LED2_DATA_PIN);
  }
  FastLED.clear(true);
}

void updateWipeIn_step(StepSegment& seg, CRGB* arr, bool isLed1) {
  unsigned long now = millis();
  if (now - seg.lastUpdate < (isLed1 ? gWipeInSpeedMs1 : gWipeInSpeedMs2)) return;
  seg.lastUpdate += (isLed1 ? gWipeInSpeedMs1 : gWipeInSpeedMs2);

  CRGB wipeColor = CRGB(isLed1 ? gColorR1 : gColorR2, isLed1 ? gColorG1 : gColorG2, isLed1 ? gColorB1 : gColorB2);
  if ((isLed1 ? gEffect1 : gEffect2) != 0) {
    wipeColor = CHSV(isLed1 ? sHue1 : sHue2, 255, 255);
    if (isLed1) sHue1++;
    else sHue2++;
  }
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
    } else {
      seg.state = TrackState::OFF;
    }
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
    if (leftIndex < 0 && rightIndex >= seg.indexCount) {
      seg.state = TrackState::OFF;
    }
  } else {
    int idx = seg.reverse ? seg.step : (seg.indexCount - 1 - seg.step);
    if (idx >= 0 && idx < seg.indexCount) {
      arr[seg.indexStart + idx] = CRGB::Black;
      seg.step++;
    } else {
      seg.state = TrackState::OFF;
    }
  }
}

void applyEffectSegment(StepSegment& seg, CRGB* arr, bool isLed1) {
  int start = seg.indexStart;
  int count = seg.indexCount;
  if ((isLed1 ? gEffect1 : gEffect2) == 0) {
    for (int i = 0; i < count; i++) {
      arr[start + i] = CRGB(isLed1 ? gColorR1 : gColorR2, isLed1 ? gColorG1 : gColorG2, isLed1 ? gColorB1 : gColorB2);
    }
  } else {
    static CRGB temp[2048];
    if (count > 2048) return;
    for (int i = 0; i < count; i++) temp[i] = CRGB::Black;
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
  if (ledNum == 1) {
    if (!stepsArray1 || !leds1 || gTotalLedsStepenice1 <= 0) return;
    for (int i = 0; i < gBrojStepenica1; i++) updateSegment(stepsArray1[i], leds1, true);
  } else if (ledNum == 2) {
    if (!stepsArray2 || !leds2 || gTotalLedsStepenice2 <= 0) return;
    for (int i = 0; i < gBrojStepenica2; i++) updateSegment(stepsArray2[i], leds2, false);
  }
}

void startSequence(bool forward, int ledNum) {
  if (ledNum == 1) {
    if (!stepsArray1 || !leds1 || gTotalLedsStepenice1 <= 0) return;
    seqActive1 = true;
    seqForward1 = forward;
    seqState1 = 0;
    seqWaitStart1 = 0;
    if (forward) seqCurrentStep1 = 0;
    else seqCurrentStep1 = gBrojStepenica1 - 1;
    for (int i = 0; i < gBrojStepenica1; i++) {
      stepsArray1[i].state = TrackState::OFF;
      stepsArray1[i].step = 0;
      stepsArray1[i].lastUpdate = millis();
    }
  } else if (ledNum == 2) {
    if (!stepsArray2 || !leds2 || gTotalLedsStepenice2 <= 0) return;
    seqActive2 = true;
    seqForward2 = forward;
    seqState2 = 0;
    seqWaitStart2 = 0;
    if (forward) seqCurrentStep2 = 0;
    else seqCurrentStep2 = gBrojStepenica2 - 1;
    for (int i = 0; i < gBrojStepenica2; i++) {
      stepsArray2[i].state = TrackState::OFF;
      stepsArray2[i].step = 0;
      stepsArray2[i].lastUpdate = millis();
    }
  }
}

void updateSequence(int ledNum) {
  if (ledNum == 1) {
    if (!stepsArray1 || !leds1 || gTotalLedsStepenice1 <= 0 || !seqActive1) return;
    if (seqState1 == 0) {
      if (seqForward1) {
        if (seqCurrentStep1 < gBrojStepenica1) {
          if (stepsArray1[seqCurrentStep1].state == TrackState::OFF) {
            stepsArray1[seqCurrentStep1].state = TrackState::WIPE_IN;
            stepsArray1[seqCurrentStep1].step = 0;
            stepsArray1[seqCurrentStep1].lastUpdate = millis();
          }
          updateSegment(stepsArray1[seqCurrentStep1], leds1, true);
          if (stepsArray1[seqCurrentStep1].state == TrackState::EFFECT) seqCurrentStep1++;
        } else {
          seqState1 = 1;
          seqWaitStart1 = millis();
        }
      } else {
        if (seqCurrentStep1 >= 0) {
          if (stepsArray1[seqCurrentStep1].state == TrackState::OFF) {
            stepsArray1[seqCurrentStep1].state = TrackState::WIPE_IN;
            stepsArray1[seqCurrentStep1].step = 0;
            stepsArray1[seqCurrentStep1].lastUpdate = millis();
          }
          updateSegment(stepsArray1[seqCurrentStep1], leds1, true);
          if (stepsArray1[seqCurrentStep1].state == TrackState::EFFECT) seqCurrentStep1--;
        } else {
          seqState1 = 1;
          seqWaitStart1 = millis();
        }
      }
    } else if (seqState1 == 1) {
      unsigned long now = millis();
      if ((now - seqWaitStart1) >= (gOnTimeSec1 * 1000UL)) {
        seqState1 = 2;
        if (seqForward1) seqCurrentStep1 = gBrojStepenica1 - 1;
        else seqCurrentStep1 = 0;
      } else {
        for (int i = 0; i < gBrojStepenica1; i++) {
          if (stepsArray1[i].state == TrackState::EFFECT) updateSegment(stepsArray1[i], leds1, true);
        }
      }
    } else if (seqState1 == 2) {
      if (seqForward1) {
        if (seqCurrentStep1 >= 0) {
          if (stepsArray1[seqCurrentStep1].state == TrackState::EFFECT) {
            stepsArray1[seqCurrentStep1].state = TrackState::WIPE_OUT;
            stepsArray1[seqCurrentStep1].step = 0;
            stepsArray1[seqCurrentStep1].lastUpdate = millis();
          }
          updateSegment(stepsArray1[seqCurrentStep1], leds1, true);
          if (stepsArray1[seqCurrentStep1].state == TrackState::OFF) seqCurrentStep1--;
          else return;
        } else seqActive1 = false;
      } else {
        if (seqCurrentStep1 < gBrojStepenica1) {
          if (stepsArray1[seqCurrentStep1].state == TrackState::EFFECT) {
            stepsArray1[seqCurrentStep1].state = TrackState::WIPE_OUT;
            stepsArray1[seqCurrentStep1].step = 0;
            stepsArray1[seqCurrentStep1].lastUpdate = millis();
          }
          updateSegment(stepsArray1[seqCurrentStep1], leds1, true);
          if (stepsArray1[seqCurrentStep1].state == TrackState::OFF) seqCurrentStep1++;
          else return;
        } else seqActive1 = false;
      }
    }
  } else if (ledNum == 2) {
    if (!stepsArray2 || !leds2 || gTotalLedsStepenice2 <= 0 || !seqActive2) return;
    if (seqState2 == 0) {
      if (seqForward2) {
        if (seqCurrentStep2 < gBrojStepenica2) {
          if (stepsArray2[seqCurrentStep2].state == TrackState::OFF) {
            stepsArray2[seqCurrentStep2].state = TrackState::WIPE_IN;
            stepsArray2[seqCurrentStep2].step = 0;
            stepsArray2[seqCurrentStep2].lastUpdate = millis();
          }
          updateSegment(stepsArray2[seqCurrentStep2], leds2, false);
          if (stepsArray2[seqCurrentStep2].state == TrackState::EFFECT) seqCurrentStep2++;
        } else {
          seqState2 = 1;
          seqWaitStart2 = millis();
        }
      } else {
        if (seqCurrentStep2 >= 0) {
          if (stepsArray2[seqCurrentStep2].state == TrackState::OFF) {
            stepsArray2[seqCurrentStep2].state = TrackState::WIPE_IN;
            stepsArray2[seqCurrentStep2].step = 0;
            stepsArray2[seqCurrentStep2].lastUpdate = millis();
          }
          updateSegment(stepsArray2[seqCurrentStep2], leds2, false);
          if (stepsArray2[seqCurrentStep2].state == TrackState::EFFECT) seqCurrentStep2--;
        } else {
          seqState2 = 1;
          seqWaitStart2 = millis();
        }
      }
    } else if (seqState2 == 1) {
      unsigned long now = millis();
      if ((now - seqWaitStart2) >= (gOnTimeSec2 * 1000UL)) {
        seqState2 = 2;
        if (seqForward2) seqCurrentStep2 = gBrojStepenica2 - 1;
        else seqCurrentStep2 = 0;
      } else {
        for (int i = 0; i < gBrojStepenica2; i++) {
          if (stepsArray2[i].state == TrackState::EFFECT) updateSegment(stepsArray2[i], leds2, false);
        }
      }
    } else if (seqState2 == 2) {
      if (seqForward2) {
        if (seqCurrentStep2 >= 0) {
          if (stepsArray2[seqCurrentStep2].state == TrackState::EFFECT) {
            stepsArray2[seqCurrentStep2].state = TrackState::WIPE_OUT;
            stepsArray2[seqCurrentStep2].step = 0;
            stepsArray2[seqCurrentStep2].lastUpdate = millis();
          }
          updateSegment(stepsArray2[seqCurrentStep2], leds2, false);
          if (stepsArray2[seqCurrentStep2].state == TrackState::OFF) seqCurrentStep2--;
          else return;
        } else seqActive2 = false;
      } else {
        if (seqCurrentStep2 < gBrojStepenica2) {
          if (stepsArray2[seqCurrentStep2].state == TrackState::EFFECT) {
            stepsArray2[seqCurrentStep2].state = TrackState::WIPE_OUT;
            stepsArray2[seqCurrentStep2].step = 0;
            stepsArray2[seqCurrentStep2].lastUpdate = millis();
          }
          updateSegment(stepsArray2[seqCurrentStep2], leds2, false);
          if (stepsArray2[seqCurrentStep2].state == TrackState::OFF) seqCurrentStep2++;
          else return;
        } else seqActive2 = false;
      }
    }
  }
}

void handleIrSensors() {
  unsigned long now = millis();

  if (sensorTriggered[0] && (now - gIrLast[0] > IR_DEBOUNCE)) {
    gIrLast[0] = now;
    sensorTriggered[0] = false;
    if (gInstallationType1 == "linija" && track1.state == TrackState::OFF) {
      track1.state = TrackState::WIPE_IN;
      track1.reverse = false;
      track1.lastUpdate = now;
      track1.step = 0;
      track1.effectStartTime = 0;
    } else if (gInstallationType1 == "stepenica" && gTotalLedsStepenice1 > 0) {
      if (gStepeniceMode1 == "sequence") {
        if (!seqActive1) startSequence(true, 1);
      } else {
        for (int i = 0; i < gBrojStepenica1; i++) {
          if (stepsArray1[i].state == TrackState::OFF) {
            stepsArray1[i].state = gWipeAll1 ? TrackState::WIPE_IN : TrackState::EFFECT;
            stepsArray1[i].step = 0;
            stepsArray1[i].lastUpdate = millis();
          }
        }
      }
    }
  }

  if (sensorTriggered[1] && (now - gIrLast[1] > IR_DEBOUNCE)) {
    gIrLast[1] = now;
    sensorTriggered[1] = false;
    if (gInstallationType1 == "linija" && track1.state == TrackState::OFF) {
      track1.state = TrackState::WIPE_IN;
      track1.reverse = true;
      track1.lastUpdate = now;
      track1.step = 0;
      track1.effectStartTime = 0;
    } else if (gInstallationType1 == "stepenica" && gTotalLedsStepenice1 > 0) {
      if (gStepeniceMode1 == "sequence") {
        if (!seqActive1) startSequence(false, 1);
      } else {
        for (int i = 0; i < gBrojStepenica1; i++) {
          if (stepsArray1[i].state == TrackState::OFF) {
            stepsArray1[i].state = gWipeAll1 ? TrackState::WIPE_IN : TrackState::EFFECT;
            stepsArray1[i].step = 0;
            stepsArray1[i].lastUpdate = millis();
          }
        }
      }
    }
  }

  if (sensorTriggered[2] && (now - gIrLast[2] > IR_DEBOUNCE)) {
    gIrLast[2] = now;
    sensorTriggered[2] = false;
    if (gInstallationType2 == "linija" && track2.state == TrackState::OFF) {
      track2.state = TrackState::WIPE_IN;
      track2.reverse = false;
      track2.lastUpdate = now;
      track2.step = 0;
      track2.effectStartTime = 0;
    } else if (gInstallationType2 == "stepenica" && gTotalLedsStepenice2 > 0) {
      if (gStepeniceMode2 == "sequence") {
        if (!seqActive2) startSequence(true, 2);
      } else {
        for (int i = 0; i < gBrojStepenica2; i++) {
          if (stepsArray2[i].state == TrackState::OFF) {
            stepsArray2[i].state = gWipeAll2 ? TrackState::WIPE_IN : TrackState::EFFECT;
            stepsArray2[i].step = 0;
            stepsArray2[i].lastUpdate = millis();
          }
        }
      }
    }
  }

  if (sensorTriggered[3] && (now - gIrLast[3] > IR_DEBOUNCE)) {
    gIrLast[3] = now;
    sensorTriggered[3] = false;
    if (gInstallationType2 == "linija" && track2.state == TrackState::OFF) {
      track2.state = TrackState::WIPE_IN;
      track2.reverse = true;
      track2.lastUpdate = now;
      track2.step = 0;
      track2.effectStartTime = 0;
    } else if (gInstallationType2 == "stepenica" && gTotalLedsStepenice2 > 0) {
      if (gStepeniceMode2 == "sequence") {
        if (!seqActive2) startSequence(false, 2);
      } else {
        for (int i = 0; i < gBrojStepenica2; i++) {
          if (stepsArray2[i].state == TrackState::OFF) {
            stepsArray2[i].state = gWipeAll2 ? TrackState::WIPE_IN : TrackState::EFFECT;
            stepsArray2[i].step = 0;
            stepsArray2[i].lastUpdate = millis();
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

uint8_t parseHour(const String& hhmm) {
  int idx = hhmm.indexOf(':');
  if (idx < 0) return 8;
  return (uint8_t)hhmm.substring(0, idx).toInt();
}

uint8_t parseMin(const String& hhmm) {
  int idx = hhmm.indexOf(':');
  if (idx < 0) return 0;
  return (uint8_t)hhmm.substring(idx + 1).toInt();
}

uint8_t getDayNightBrightness() {
  struct tm tinfo;
  if (!getLocalTime(&tinfo)) return map(gDayBrightnessPercent, 0, 100, 0, 255);
  int cur = tinfo.tm_hour * 60 + tinfo.tm_min;
  int ds = parseHour(gDayStartStr) * 60 + parseMin(gDayStartStr);
  int de = parseHour(gDayEndStr) * 60 + parseMin(gDayEndStr);
  if (ds < de) {
    if (cur >= ds && cur < de) return map(gDayBrightnessPercent, 0, 100, 0, 255);
    else return map(gNightBrightnessPercent, 0, 100, 0, 255);
  } else {
    if (cur >= ds || cur < de) return map(gDayBrightnessPercent, 0, 100, 0, 255);
    else return map(gNightBrightnessPercent, 0, 100, 0, 255);
  }
}

bool handleFileRead(String path) {
  if (path.endsWith("/")) path += "index.html";
  if (SPIFFS.exists(path)) {
    File file = SPIFFS.open(path, "r");
    String contentType = "text/html";
    if (path.endsWith(".css")) contentType = "text/css";
    else if (path.endsWith(".js")) contentType = "application/javascript";
    else if (path.endsWith(".png")) contentType = "image/png";
    server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}

void handleNotFound() {
  if (!handleFileRead(server.uri())) server.send(404, "text/plain", "Not found");
}

bool loadConfig() {
  if (!SPIFFS.exists("/config.json")) return false;
  File file = SPIFFS.open("/config.json", "r");
  if (!file) return false;
  DeserializationError error = deserializeJson(configDoc, file);
  file.close();
  if (error) return false;
  return true;
}

bool saveConfig() {
  File file = SPIFFS.open("/config.json", "w");
  if (!file) return false;
  if (serializeJson(configDoc, file) == 0) {
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

  configDoc["dayStart"] = "08:00";
  configDoc["dayEnd"] = "20:00";
  configDoc["dayBr"] = 100;
  configDoc["nightBr"] = 30;
  configDoc["maxCurrent"] = 2000;
  configDoc["mainSwitch"] = "both";
}

void applyConfigToLEDStrips() {
  String led1TypeStr = configDoc["led1"]["type"] | "ws2812b";
  gLed1Type = (led1TypeStr == "ws2811") ? LedType::WS2811 : (led1TypeStr == "ws2812")       ? LedType::WS2812
                                                          : (led1TypeStr == "ws2812b")      ? LedType::WS2812B
                                                          : (led1TypeStr == "ws2813")       ? LedType::WS2813
                                                          : (led1TypeStr == "sk6812")       ? LedType::SK6812
                                                          : (led1TypeStr == "ws2811_white") ? LedType::WS2811_WHITE
                                                                                            : LedType::WS2812B;

  String led1ColorOrderStr = configDoc["led1"]["colorOrder"] | "GRB";
  gLed1ColorOrder = (led1ColorOrderStr == "RGB") ? ColorOrder::RGB : (led1ColorOrderStr == "GRB") ? ColorOrder::GRB
                                                                   : (led1ColorOrderStr == "BGR") ? ColorOrder::BGR
                                                                   : (led1ColorOrderStr == "RBG") ? ColorOrder::RBG
                                                                   : (led1ColorOrderStr == "GBR") ? ColorOrder::GBR
                                                                   : (led1ColorOrderStr == "BRG") ? ColorOrder::BRG
                                                                                                  : ColorOrder::GRB;

  gInstallationType1 = configDoc["led1"]["mode"] | "linija";
  if (gInstallationType1 == "linija") {
    gNumLeds1 = configDoc["led1"]["linijaLEDCount"] | 35;
    if (leds1) {
      delete[] leds1;
      leds1 = nullptr;
    }
    leds1 = new CRGB[gNumLeds1];
    if (!leds1) {
      if (DEBUG) Serial.println("Failed to allocate leds1 for linija mode");
      return;
    }
    switch (gLed1Type) {
      case LedType::WS2811:
        switch (gLed1ColorOrder) {
          case ColorOrder::RGB: FastLED.addLeds<WS2811, LED1_DATA_PIN, RGB>(leds1, gNumLeds1); break;
          case ColorOrder::GRB: FastLED.addLeds<WS2811, LED1_DATA_PIN, GRB>(leds1, gNumLeds1); break;
          case ColorOrder::BGR: FastLED.addLeds<WS2811, LED1_DATA_PIN, BGR>(leds1, gNumLeds1); break;
          case ColorOrder::RBG: FastLED.addLeds<WS2811, LED1_DATA_PIN, RBG>(leds1, gNumLeds1); break;
          case ColorOrder::GBR: FastLED.addLeds<WS2811, LED1_DATA_PIN, GBR>(leds1, gNumLeds1); break;
          case ColorOrder::BRG: FastLED.addLeds<WS2811, LED1_DATA_PIN, BRG>(leds1, gNumLeds1); break;
        }
        break;
      case LedType::WS2812:
        switch (gLed1ColorOrder) {
          case ColorOrder::RGB: FastLED.addLeds<WS2812, LED1_DATA_PIN, RGB>(leds1, gNumLeds1); break;
          case ColorOrder::GRB: FastLED.addLeds<WS2812, LED1_DATA_PIN, GRB>(leds1, gNumLeds1); break;
          case ColorOrder::BGR: FastLED.addLeds<WS2812, LED1_DATA_PIN, BGR>(leds1, gNumLeds1); break;
          case ColorOrder::RBG: FastLED.addLeds<WS2812, LED1_DATA_PIN, RBG>(leds1, gNumLeds1); break;
          case ColorOrder::GBR: FastLED.addLeds<WS2812, LED1_DATA_PIN, GBR>(leds1, gNumLeds1); break;
          case ColorOrder::BRG: FastLED.addLeds<WS2812, LED1_DATA_PIN, BRG>(leds1, gNumLeds1); break;
        }
        break;
      case LedType::WS2812B:
        switch (gLed1ColorOrder) {
          case ColorOrder::RGB: FastLED.addLeds<WS2812B, LED1_DATA_PIN, RGB>(leds1, gNumLeds1); break;
          case ColorOrder::GRB: FastLED.addLeds<WS2812B, LED1_DATA_PIN, GRB>(leds1, gNumLeds1); break;
          case ColorOrder::BGR: FastLED.addLeds<WS2812B, LED1_DATA_PIN, BGR>(leds1, gNumLeds1); break;
          case ColorOrder::RBG: FastLED.addLeds<WS2812B, LED1_DATA_PIN, RBG>(leds1, gNumLeds1); break;
          case ColorOrder::GBR: FastLED.addLeds<WS2812B, LED1_DATA_PIN, GBR>(leds1, gNumLeds1); break;
          case ColorOrder::BRG: FastLED.addLeds<WS2812B, LED1_DATA_PIN, BRG>(leds1, gNumLeds1); break;
        }
        break;
      case LedType::WS2813:
        switch (gLed1ColorOrder) {
          case ColorOrder::RGB: FastLED.addLeds<WS2813, LED1_DATA_PIN, RGB>(leds1, gNumLeds1); break;
          case ColorOrder::GRB: FastLED.addLeds<WS2813, LED1_DATA_PIN, GRB>(leds1, gNumLeds1); break;
          case ColorOrder::BGR: FastLED.addLeds<WS2813, LED1_DATA_PIN, BGR>(leds1, gNumLeds1); break;
          case ColorOrder::RBG: FastLED.addLeds<WS2813, LED1_DATA_PIN, RBG>(leds1, gNumLeds1); break;
          case ColorOrder::GBR: FastLED.addLeds<WS2813, LED1_DATA_PIN, GBR>(leds1, gNumLeds1); break;
          case ColorOrder::BRG: FastLED.addLeds<WS2813, LED1_DATA_PIN, BRG>(leds1, gNumLeds1); break;
        }
        break;
      case LedType::SK6812:
        switch (gLed1ColorOrder) {
          case ColorOrder::RGB: FastLED.addLeds<SK6812, LED1_DATA_PIN, RGB>(leds1, gNumLeds1); break;
          case ColorOrder::GRB: FastLED.addLeds<SK6812, LED1_DATA_PIN, GRB>(leds1, gNumLeds1); break;
          case ColorOrder::BGR: FastLED.addLeds<SK6812, LED1_DATA_PIN, BGR>(leds1, gNumLeds1); break;
          case ColorOrder::RBG: FastLED.addLeds<SK6812, LED1_DATA_PIN, RBG>(leds1, gNumLeds1); break;
          case ColorOrder::GBR: FastLED.addLeds<SK6812, LED1_DATA_PIN, GBR>(leds1, gNumLeds1); break;
          case ColorOrder::BRG: FastLED.addLeds<SK6812, LED1_DATA_PIN, BRG>(leds1, gNumLeds1); break;
        }
        break;
      case LedType::WS2811_WHITE:
        FastLED.addLeds<WS2811, LED1_DATA_PIN, RGB>(leds1, gNumLeds1);
        break;
      default:
        FastLED.addLeds<WS2812B, LED1_DATA_PIN, GRB>(leds1, gNumLeds1);
        break;
    }
    if (DEBUG) Serial.printf("LED1 linija mode: %d LEDs\n", gNumLeds1);
  } else if (gInstallationType1 == "stepenica") {
    gBrojStepenica1 = configDoc["led1"]["numSteps"] | 3;
    gDefaultLedsPerStep1 = configDoc["led1"]["defaultLedsPerStep"] | 30;
    gVariableSteps1 = configDoc["led1"]["variable"] | false;
    if (gVariableSteps1 && !configDoc["led1"]["stepLengths"].isNull()) {
      JsonArray stepLengths = configDoc["led1"]["stepLengths"];
      if (gStepLengths1) {
        delete[] gStepLengths1;
        gStepLengths1 = nullptr;
      }
      gStepLengths1 = new int[gBrojStepenica1];
      for (int i = 0; i < gBrojStepenica1 && i < stepLengths.size(); i++) gStepLengths1[i] = stepLengths[i].as<int>();
    }
    initStepSegments(1);
  }

  gRotacijaStepenica1 = configDoc["led1"]["rotation"] | false;
  gEfektKreniOd1 = configDoc["led1"]["efektKreniOd"] | "sredina";
  gStepeniceMode1 = configDoc["led1"]["stepMode"] | "allAtOnce";
  gEffect1 = (configDoc["led1"]["effect"] == "solid") ? 0 : (configDoc["led1"]["effect"] == "confetti")  ? 1
                                                          : (configDoc["led1"]["effect"] == "rainbow")   ? 2
                                                          : (configDoc["led1"]["effect"] == "meteor")    ? 3
                                                          : (configDoc["led1"]["effect"] == "fade")      ? 4
                                                          : (configDoc["led1"]["effect"] == "fire")      ? 5
                                                          : (configDoc["led1"]["effect"] == "lightning") ? 6
                                                                                                         : 0;

  String colorStr1 = configDoc["led1"]["color"] | "#002aff";
  if (colorStr1.length() == 7) {
    gColorR1 = strtol(colorStr1.substring(1, 3).c_str(), NULL, 16);
    gColorG1 = strtol(colorStr1.substring(3, 5).c_str(), NULL, 16);
    gColorB1 = strtol(colorStr1.substring(5, 7).c_str(), NULL, 16);
  }
  gWipeInSpeedMs1 = configDoc["led1"]["wipeSpeed"] | 50;
  gWipeOutSpeedMs1 = configDoc["led1"]["wipeSpeed"] | 50;
  gOnTimeSec1 = configDoc["led1"]["onTime"] | 2;

  String led2TypeStr = configDoc["led2"]["type"] | "ws2812b";
  gLed2Type = (led2TypeStr == "ws2811") ? LedType::WS2811 : (led2TypeStr == "ws2812")       ? LedType::WS2812
                                                          : (led2TypeStr == "ws2812b")      ? LedType::WS2812B
                                                          : (led2TypeStr == "ws2813")       ? LedType::WS2813
                                                          : (led2TypeStr == "sk6812")       ? LedType::SK6812
                                                          : (led2TypeStr == "ws2811_white") ? LedType::WS2811_WHITE
                                                                                            : LedType::WS2812B;

  String led2ColorOrderStr = configDoc["led2"]["colorOrder"] | "GRB";
  gLed2ColorOrder = (led2ColorOrderStr == "RGB") ? ColorOrder::RGB : (led2ColorOrderStr == "GRB") ? ColorOrder::GRB
                                                                   : (led2ColorOrderStr == "BGR") ? ColorOrder::BGR
                                                                   : (led2ColorOrderStr == "RBG") ? ColorOrder::RBG
                                                                   : (led2ColorOrderStr == "GBR") ? ColorOrder::GBR
                                                                   : (led2ColorOrderStr == "BRG") ? ColorOrder::BRG
                                                                                                  : ColorOrder::GRB;

  gInstallationType2 = configDoc["led2"]["mode"] | "linija";
  if (gInstallationType2 == "linija") {
    gNumLeds2 = configDoc["led2"]["linijaLEDCount"] | 35;
    if (leds2) {
      delete[] leds2;
      leds2 = nullptr;
    }
    leds2 = new CRGB[gNumLeds2];
    if (!leds2) {
      if (DEBUG) Serial.println("Failed to allocate leds2 for linija mode");
      return;
    }
    switch (gLed2Type) {
      case LedType::WS2811:
        switch (gLed2ColorOrder) {
          case ColorOrder::RGB: FastLED.addLeds<WS2811, LED2_DATA_PIN, RGB>(leds2, gNumLeds2); break;
          case ColorOrder::GRB: FastLED.addLeds<WS2811, LED2_DATA_PIN, GRB>(leds2, gNumLeds2); break;
          case ColorOrder::BGR: FastLED.addLeds<WS2811, LED2_DATA_PIN, BGR>(leds2, gNumLeds2); break;
          case ColorOrder::RBG: FastLED.addLeds<WS2811, LED2_DATA_PIN, RBG>(leds2, gNumLeds2); break;
          case ColorOrder::GBR: FastLED.addLeds<WS2811, LED2_DATA_PIN, GBR>(leds2, gNumLeds2); break;
          case ColorOrder::BRG: FastLED.addLeds<WS2811, LED2_DATA_PIN, BRG>(leds2, gNumLeds2); break;
        }
        break;
      case LedType::WS2812:
        switch (gLed2ColorOrder) {
          case ColorOrder::RGB: FastLED.addLeds<WS2812, LED2_DATA_PIN, RGB>(leds2, gNumLeds2); break;
          case ColorOrder::GRB: FastLED.addLeds<WS2812, LED2_DATA_PIN, GRB>(leds2, gNumLeds2); break;
          case ColorOrder::BGR: FastLED.addLeds<WS2812, LED2_DATA_PIN, BGR>(leds2, gNumLeds2); break;
          case ColorOrder::RBG: FastLED.addLeds<WS2812, LED2_DATA_PIN, RBG>(leds2, gNumLeds2); break;
          case ColorOrder::GBR: FastLED.addLeds<WS2812, LED2_DATA_PIN, GBR>(leds2, gNumLeds2); break;
          case ColorOrder::BRG: FastLED.addLeds<WS2812, LED2_DATA_PIN, BRG>(leds2, gNumLeds2); break;
        }
        break;
      case LedType::WS2812B:
        switch (gLed2ColorOrder) {
          case ColorOrder::RGB: FastLED.addLeds<WS2812B, LED2_DATA_PIN, RGB>(leds2, gNumLeds2); break;
          case ColorOrder::GRB: FastLED.addLeds<WS2812B, LED2_DATA_PIN, GRB>(leds2, gNumLeds2); break;
          case ColorOrder::BGR: FastLED.addLeds<WS2812B, LED2_DATA_PIN, BGR>(leds2, gNumLeds2); break;
          case ColorOrder::RBG: FastLED.addLeds<WS2812B, LED2_DATA_PIN, RBG>(leds2, gNumLeds2); break;
          case ColorOrder::GBR: FastLED.addLeds<WS2812B, LED2_DATA_PIN, GBR>(leds2, gNumLeds2); break;
          case ColorOrder::BRG: FastLED.addLeds<WS2812B, LED2_DATA_PIN, BRG>(leds2, gNumLeds2); break;
        }
        break;
      case LedType::WS2813:
        switch (gLed2ColorOrder) {
          case ColorOrder::RGB: FastLED.addLeds<WS2813, LED2_DATA_PIN, RGB>(leds2, gNumLeds2); break;
          case ColorOrder::GRB: FastLED.addLeds<WS2813, LED2_DATA_PIN, GRB>(leds2, gNumLeds2); break;
          case ColorOrder::BGR: FastLED.addLeds<WS2813, LED2_DATA_PIN, BGR>(leds2, gNumLeds2); break;
          case ColorOrder::RBG: FastLED.addLeds<WS2813, LED2_DATA_PIN, RBG>(leds2, gNumLeds2); break;
          case ColorOrder::GBR: FastLED.addLeds<WS2813, LED2_DATA_PIN, GBR>(leds2, gNumLeds2); break;
          case ColorOrder::BRG: FastLED.addLeds<WS2813, LED2_DATA_PIN, BRG>(leds2, gNumLeds2); break;
        }
        break;
      case LedType::SK6812:
        switch (gLed2ColorOrder) {
          case ColorOrder::RGB: FastLED.addLeds<SK6812, LED2_DATA_PIN, RGB>(leds2, gNumLeds2); break;
          case ColorOrder::GRB: FastLED.addLeds<SK6812, LED2_DATA_PIN, GRB>(leds2, gNumLeds2); break;
          case ColorOrder::BGR: FastLED.addLeds<SK6812, LED2_DATA_PIN, BGR>(leds2, gNumLeds2); break;
          case ColorOrder::RBG: FastLED.addLeds<SK6812, LED2_DATA_PIN, RBG>(leds2, gNumLeds2); break;
          case ColorOrder::GBR: FastLED.addLeds<SK6812, LED2_DATA_PIN, GBR>(leds2, gNumLeds2); break;
          case ColorOrder::BRG: FastLED.addLeds<SK6812, LED2_DATA_PIN, BRG>(leds2, gNumLeds2); break;
        }
        break;
      case LedType::WS2811_WHITE:
        FastLED.addLeds<WS2811, LED2_DATA_PIN, RGB>(leds2, gNumLeds2);
        break;
      default:
        FastLED.addLeds<WS2812B, LED2_DATA_PIN, GRB>(leds2, gNumLeds2);
        break;
    }
    if (DEBUG) Serial.printf("LED2 linija mode: %d LEDs\n", gNumLeds2);
  } else if (gInstallationType2 == "stepenica") {
    gBrojStepenica2 = configDoc["led2"]["numSteps"] | 3;
    gDefaultLedsPerStep2 = configDoc["led2"]["defaultLedsPerStep"] | 30;
    gVariableSteps2 = configDoc["led2"]["variable"] | false;
    if (gVariableSteps2 && !configDoc["led2"]["stepLengths"].isNull()) {
      JsonArray stepLengths = configDoc["led2"]["stepLengths"];
      if (gStepLengths2) {
        delete[] gStepLengths2;
        gStepLengths2 = nullptr;
      }
      gStepLengths2 = new int[gBrojStepenica2];
      for (int i = 0; i < gBrojStepenica2 && i < stepLengths.size(); i++) gStepLengths2[i] = stepLengths[i].as<int>();
    }
    initStepSegments(2);
  }


  gRotacijaStepenica2 = configDoc["led2"]["rotation"] | false;
  gEfektKreniOd2 = configDoc["led2"]["efektKreniOd"] | "sredina";
  gStepeniceMode2 = configDoc["led2"]["stepMode"] | "allAtOnce";
  gEffect2 = (configDoc["led2"]["effect"] == "solid") ? 0 : (configDoc["led2"]["effect"] == "confetti")  ? 1
                                                          : (configDoc["led2"]["effect"] == "rainbow")   ? 2
                                                          : (configDoc["led2"]["effect"] == "meteor")    ? 3
                                                          : (configDoc["led2"]["effect"] == "fade")      ? 4
                                                          : (configDoc["led2"]["effect"] == "fire")      ? 5
                                                          : (configDoc["led2"]["effect"] == "lightning") ? 6
                                                                                                         : 0;

  String colorStr2 = configDoc["led2"]["color"] | "#002aff";
  if (colorStr2.length() == 7) {
    gColorR2 = strtol(colorStr2.substring(1, 3).c_str(), NULL, 16);
    gColorG2 = strtol(colorStr2.substring(3, 5).c_str(), NULL, 16);
    gColorB2 = strtol(colorStr2.substring(5, 7).c_str(), NULL, 16);
  }
  gWipeInSpeedMs2 = configDoc["led2"]["wipeSpeed"] | 50;
  gWipeOutSpeedMs2 = configDoc["led2"]["wipeSpeed"] | 50;
  gOnTimeSec2 = configDoc["led2"]["onTime"] | 2;

  gDayStartStr = configDoc["dayStart"] | "08:00";
  gDayEndStr = configDoc["dayEnd"] | "20:00";
  gDayBrightnessPercent = configDoc["dayBr"] | 100;
  gNightBrightnessPercent = configDoc["nightBr"] | 30;
  maxCurrent = configDoc["maxCurrent"] | 2000;
  gMainSwitch = configDoc["mainSwitch"] | "both";

  FastLED.setMaxPowerInVoltsAndMilliamps(5, maxCurrent);
  FastLED.clear(true);
  if (DEBUG) Serial.printf("Config applied: mainSwitch = %s\n", gMainSwitch.c_str());
}

void handleGetConfig() {
  String output;
  serializeJson(configDoc, output);
  server.send(200, "application/json", output);
}

void handleSaveConfig() {
  if (server.hasArg("plain")) {
    String json = server.arg("plain");
    DeserializationError error = deserializeJson(configDoc, json);
    if (error) {
      server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
      return;
    }
    saveConfig();
    applyConfigToLEDStrips();
    server.send(200, "application/json", "{\"status\":\"ok\"}");
  } else {
    server.send(400, "application/json", "{\"error\":\"No data received\"}");
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);

  // Inicijalizacija watchdoga
  esp_task_wdt_init(&wdt_config);
  esp_task_wdt_add(NULL);

  // Inicijalizacija SPIFFS-a
  if (!SPIFFS.begin(true)) {
    if (DEBUG) Serial.println("SPIFFS failed to initialize");
    return;
  }

  // Učitavanje konfiguracije ili postavljanje zadanih vrijednosti
  if (!loadConfig()) {
    if (DEBUG) Serial.println("Config not found, setting defaults");
    setDefaultConfig();
    saveConfig();
  }
  applyConfigToLEDStrips();

  // Isključivanje watchdoga za WiFi postavljanje
  esp_task_wdt_delete(NULL);

  // Postavljanje WiFi-a
  wifiManager.setTimeout(30);
  if (!wifiManager.autoConnect("StairLight_AP", "12345678")) {
    if (DEBUG) Serial.println("WiFi connection failed...");
  } else {
    if (DEBUG) Serial.println("WiFi connected: " + WiFi.localIP().toString());
  }

  // Ponovno uključivanje watchdoga nakon WiFi postavljanja
  esp_task_wdt_add(NULL);

  // Postavljanje mDNS-a
  if (MDNS.begin("stairlight")) {
    MDNS.addService("http", "tcp", 80);
  }

  // Postavljanje web poslužitelja
  server.on("/api/getConfig", HTTP_GET, handleGetConfig);
  server.on("/api/saveConfig", HTTP_POST, handleSaveConfig);
  server.on("/", HTTP_GET, []() {
    handleFileRead("/index.html");
  });
  server.onNotFound(handleNotFound);
  server.begin();

  // Postavljanje pinova i prekida
  pinMode(MAIN_SWITCH_PIN, INPUT_PULLUP);
  pinMode(LED1_SENSOR_START_PIN, INPUT);
  pinMode(LED1_SENSOR_END_PIN, INPUT);
  pinMode(LED2_SENSOR_START_PIN, INPUT);
  pinMode(LED2_SENSOR_END_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(LED1_SENSOR_START_PIN), led1SensorStartISR, RISING);
  attachInterrupt(digitalPinToInterrupt(LED1_SENSOR_END_PIN), led1SensorEndISR, RISING);
  attachInterrupt(digitalPinToInterrupt(LED2_SENSOR_START_PIN), led2SensorStartISR, RISING);
  attachInterrupt(digitalPinToInterrupt(LED2_SENSOR_END_PIN), led2SensorEndISR, RISING);

  // Postavljanje vremena
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  if (DEBUG) Serial.println("Setup complete");
}

void loop() {
  server.handleClient();
  esp_task_wdt_reset();

  bool reading = digitalRead(MAIN_SWITCH_PIN);
  if (reading != gLastButtonState && (millis() - gLastButtonDebounce > BTN_DEBOUNCE_MS)) {
    gLastButtonDebounce = millis();
    gLastButtonState = reading;
    if (reading == LOW) {
      gManualOverride = !gManualOverride;
      if (DEBUG) Serial.printf("Manual override toggled to: %d\n", gManualOverride);
      if (!gManualOverride) {
        track1.state = TrackState::OFF;
        track2.state = TrackState::OFF;
        if (stepsArray1) {
          for (int i = 0; i < gBrojStepenica1; i++) stepsArray1[i].state = TrackState::OFF;
        }
        if (stepsArray2) {
          for (int i = 0; i < gBrojStepenica2; i++) stepsArray2[i].state = TrackState::OFF;
        }
        if (leds1) {
          for (int i = 0; i < (gInstallationType1 == "linija" ? gNumLeds1 : gTotalLedsStepenice1); i++) leds1[i] = CRGB::Black;
          if (DEBUG) Serial.println("LED1 cleared");
        }
        if (leds2) {
          for (int i = 0; i < (gInstallationType2 == "linija" ? gNumLeds2 : gTotalLedsStepenice2); i++) leds2[i] = CRGB::Black;
          if (DEBUG) Serial.println("LED2 cleared");
        }
        FastLED.show();
      }
    }
  }

  if (gManualOverride) {
    if (DEBUG) Serial.printf("Main switch mode: %s\n", gMainSwitch.c_str());
    if (gMainSwitch == "both" || gMainSwitch == "led1") {
      if (gInstallationType1 == "linija" && leds1) {
        applyGlobalEffect(leds1, gNumLeds1, true);
        if (DEBUG) Serial.println("LED1 effect applied");
      } else if (gTotalLedsStepenice1 > 0 && leds1) {
        applyGlobalEffect(leds1, gTotalLedsStepenice1, true);
        if (DEBUG) Serial.println("LED1 stepenica effect applied");
      } else {
        if (DEBUG) Serial.println("LED1 not initialized or invalid mode");
      }
    }
    if (gMainSwitch == "both" || gMainSwitch == "led2") {
      if (gInstallationType2 == "linija" && leds2) {
        applyGlobalEffect(leds2, gNumLeds2, false);
        if (DEBUG) Serial.println("LED2 effect applied");
      } else if (gTotalLedsStepenice2 > 0 && leds2) {
        applyGlobalEffect(leds2, gTotalLedsStepenice2, false);
        if (DEBUG) Serial.println("LED2 stepenica effect applied");
      } else {
        if (DEBUG) Serial.println("LED2 not initialized or invalid mode");
      }
    }
  } else {
    handleIrSensors();
    if (gMainSwitch == "both" || gMainSwitch == "led1") {
      if (gInstallationType1 == "stepenica" && gTotalLedsStepenice1 > 0) {
        if (gStepeniceMode1 == "sequence") updateSequence(1);
        else updateAllStepSegments(1);
      } else if (gInstallationType1 == "linija" && leds1) {
        updateTrack_line(track1, leds1, gNumLeds1, true);
      }
    }
    if (gMainSwitch == "both" || gMainSwitch == "led2") {
      if (gInstallationType2 == "stepenica" && gTotalLedsStepenice2 > 0) {
        if (gStepeniceMode2 == "sequence") updateSequence(2);
        else updateAllStepSegments(2);
      } else if (gInstallationType2 == "linija" && leds2) {
        updateTrack_line(track2, leds2, gNumLeds2, false);
      }
    }
  }

  FastLED.setBrightness(getDayNightBrightness());
  FastLED.show();
  delay(10);
}
