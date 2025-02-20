#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <FastLED.h>
#include <time.h>
#include <WiFiManager.h>
#include <ESPmDNS.h>
#include <esp_task_wdt.h>

// Watchdog konfiguracija
const esp_task_wdt_config_t wdt_config = {
    .timeout_ms = 10000,  // 10 sekundi u milisekundama
    .idle_core_mask = 0,  // Sve jezgre
    .trigger_panic = true // Resetiraj pri timeoutu
};

// Definicije pinova
#define MAIN_SWITCH_PIN 26
#define LED1_DATA_PIN 33
#define LED2_DATA_PIN 5
#define LED1_SENSOR_START_PIN 17
#define LED1_SENSOR_END_PIN   16
#define LED2_SENSOR_START_PIN 32
#define LED2_SENSOR_END_PIN   27

// ENUMI I STRUKTURE
enum class TrackState {
  OFF = 0,
  WIPE_IN,
  EFFECT,
  WIPE_OUT
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

// GLOBALNE PROMJENJIVE
WebServer server(80);
WiFiManager wifiManager;
JsonDocument configDoc;
int maxCurrent = 2000;

CRGB* leds1 = nullptr;
CRGB* leds2 = nullptr;
int gNumLeds1 = 0;
int gNumLeds2 = 0;

int gBrojStepenica = 3;
int gDefaultLedsPerStep = 30;
bool gVariableSteps = false;
int* gStepLengths = nullptr;
int gTotalLedsStepenice = 0;
StepSegment* stepsArray = nullptr;

String gInstallationType = "linija";
String gEfektKreniOd = "sredina";
bool gRotacijaStepenica = false;
String gStepeniceMode = "allAtOnce";

uint8_t gEffect = 0;
bool gWipeAll = true;
uint16_t gOnTimeSec = 2;
uint16_t gWipeInSpeedMs = 50;
uint16_t gWipeOutSpeedMs = 50;
uint8_t gColorR = 0, gColorG = 42, gColorB = 255;

String gDayStartStr = "08:00";
String gDayEndStr = "20:00";
int gDayBrightnessPercent = 100;
int gNightBrightnessPercent = 30;

bool gManualOverride = false;
unsigned long gLastButtonDebounce = 0;
bool gLastButtonState = HIGH;
const unsigned long BTN_DEBOUNCE_MS = 250;

Track track1, track2;
volatile bool sensorTriggered[4] = {false, false, false, false};
unsigned long gIrLast[4] = {0, 0, 0, 0};
const unsigned long IR_DEBOUNCE = 300;

static uint8_t sHue1=0, sHue2=0;
static uint16_t sMet1=0, sMet2=0;
static int sFadeVal1=0, sFadeDir1=1;
static int sFadeVal2=0, sFadeDir2=1;

static bool seqActive = false;
static bool seqForward = true;
static int seqState = 0;
static int seqCurrentStep = 0;
static unsigned long seqWaitStart = 0;

// PROTOTIPI FUNKCIJA
void nonBlockingDelay(unsigned long ms);
void applyGlobalEffect(CRGB* arr, int count, bool isLed1);
void updateTrack_line(Track& trk, CRGB* arr, int count, bool isLed1);
void initStepSegments();
void updateSegment(StepSegment& seg, CRGB* arr);
void updateAllStepSegments();
void startSequence(bool forward);
void updateSequence();
void handleIrSensors();
void applyConfigToLEDStrips(const JsonDocument &doc);
uint8_t getDayNightBrightness();
bool handleFileRead(String path);

// NON-BLOCKING DELAY
void nonBlockingDelay(unsigned long ms) {
  unsigned long start = millis();
  while (millis() - start < ms) {
    server.handleClient();
    esp_task_wdt_reset();
    delay(1);
  }
}

// EFEKTI
void doSolid(CRGB* arr, int count, uint8_t r, uint8_t g, uint8_t b) {
  for(int i=0; i<count; i++) {
    arr[i] = CRGB(r,g,b);
  }
}

void applyGlobalEffect(CRGB* arr, int count, bool isLed1) {
  if(gEffect == 0) {
    doSolid(arr, count, gColorR, gColorG, gColorB);
    return;
  }
  switch(gEffect) {
    case 1: {
      for(int i=0; i<count; i++) {
        arr[i].nscale8(200);
      }
      arr[random(count)] = CHSV(random8(), 200, 255);
    } break;
    case 2: {
      for(int i=0; i<count; i++) {
        if(isLed1) arr[i] = CHSV(sHue1 + i*2, 255, 255);
        else arr[i] = CHSV(sHue2 + i*2, 255, 255);
      }
      if(isLed1) sHue1++; else sHue2++;
    } break;
    case 3: {
      for(int i=0; i<count; i++) {
        arr[i].fadeToBlackBy(40);
      }
      if(isLed1) {
        arr[sMet1] = CHSV(30, 255, 255);
        sMet1++; if(sMet1 >= count) sMet1 = 0;
      } else {
        arr[sMet2] = CHSV(180, 255, 255);
        sMet2++; if(sMet2 >= count) sMet2 = 0;
      }
    } break;
    case 4: {
      if(isLed1) {
        sFadeVal1 += (5 * sFadeDir1);
        if(sFadeVal1 > 255) { sFadeVal1 = 255; sFadeDir1 = -1; }
        if(sFadeVal1 < 0) { sFadeVal1 = 0; sFadeDir1 = 1; }
        CRGB c = CHSV(160, 200, 255);
        c.nscale8_video((uint8_t)sFadeVal1);
        for(int i=0; i<count; i++) arr[i] = c;
      } else {
        sFadeVal2 += (5 * sFadeDir2);
        if(sFadeVal2 > 255) { sFadeVal2 = 255; sFadeDir2 = -1; }
        if(sFadeVal2 < 0) { sFadeVal2 = 0; sFadeDir2 = 1; }
        CRGB c = CHSV(60, 200, 255);
        c.nscale8_video((uint8_t)sFadeVal2);
        for(int i=0; i<count; i++) arr[i] = c;
      }
    } break;
    case 5: {
      for(int i=0; i<count; i++) {
        uint8_t rr = random(150, 256);
        uint8_t gg = random(0, 100);
        arr[i] = CRGB(rr, gg, 0);
      }
    } break;
    case 6: {
      for(int i=0; i<count; i++) {
        arr[i] = CRGB::Black;
      }
      int flashes = random(2, 4);
      for(int f=0; f<flashes; f++) {
        int start = random(count);
        int length = random(10, 30);
        for(int i=0; i<length && (start+i)<count; i++) {
          arr[start+i] = CRGB::White;
        }
        FastLED.show();
        delay(random(30, 80));
        for(int i=0; i<length && (start+i)<count; i++) {
          arr[start+i] = CRGB::Black;
        }
        FastLED.show();
        delay(random(60, 150));
      }
    } break;
    default: {
      doSolid(arr, count, gColorR, gColorG, gColorB);
    } break;
  }
}

// LINIJA NACIN
void updateWipeIn_line(Track& trk, CRGB* arr, int count, bool isLed1) {
  unsigned long now = millis();
  if(now - trk.lastUpdate < gWipeInSpeedMs) return;
  trk.lastUpdate += gWipeInSpeedMs;

  CRGB wipeColor;
  if(gEffect == 0) wipeColor = CRGB(gColorR, gColorG, gColorB);
  else {
    wipeColor = CHSV(isLed1 ? sHue1 : sHue2, 255, 255);
    if(isLed1) sHue1++; else sHue2++;
  }
  int idx = trk.reverse ? (count-1 - trk.step) : trk.step;
  if(idx >= 0 && idx < count) {
    arr[idx] = wipeColor;
    trk.step++;
  } else {
    trk.state = TrackState::EFFECT;
    trk.effectStartTime = now;
  }
}

void updateEffect_line(Track& trk) {
  unsigned long now = millis();
  if((now - trk.effectStartTime) >= (gOnTimeSec * 1000UL)) {
    if(gWipeAll) {
      trk.state = TrackState::WIPE_OUT;
      trk.lastUpdate = now;
      trk.step = 0;
    } else {
      trk.state = TrackState::OFF;
    }
  }
}

void updateWipeOut_line(Track& trk, CRGB* arr, int count) {
  unsigned long now = millis();
  if(now - trk.lastUpdate < gWipeOutSpeedMs) return;
  trk.lastUpdate += gWipeOutSpeedMs;

  int idx = trk.reverse ? trk.step : (count-1 - trk.step);
  if(idx >= 0 && idx < count) {
    arr[idx] = CRGB::Black;
    trk.step++;
  } else {
    trk.state = TrackState::OFF;
  }
}

void updateTrack_line(Track& trk, CRGB* arr, int count, bool isLed1) {
  switch(trk.state) {
    case TrackState::OFF: 
      break;
    case TrackState::WIPE_IN:
      updateWipeIn_line(trk, arr, count, isLed1);
      break;
    case TrackState::EFFECT:
      applyGlobalEffect(arr, count, isLed1);
      updateEffect_line(trk);
      break;
    case TrackState::WIPE_OUT:
      updateWipeOut_line(trk, arr, count);
      break;
  }
}

// STEPENICA
void initStepSegments() {
  Serial.println("Pokrećem initStepSegments...");
  if (gBrojStepenica <= 0 || gDefaultLedsPerStep <= 0) {
    Serial.println("Greška: Broj stepenica ili LED-ova po stepenici nije ispravan.");
    gTotalLedsStepenice = 0;
    return;
  }

  gTotalLedsStepenice = 0;
  if (gVariableSteps && gStepLengths) {
    Serial.println("Varijabilne stepenice - računam ukupno LED-ova...");
    for (int i = 0; i < gBrojStepenica; i++) {
      if (gStepLengths[i] <= 0) {
        Serial.println("Greška: Duljina stepenice nije ispravna na indeksu " + String(i));
        gTotalLedsStepenice = 0;
        return;
      }
      gTotalLedsStepenice += gStepLengths[i];
    }
  } else {
    gTotalLedsStepenice = gBrojStepenica * gDefaultLedsPerStep;
  }
  Serial.println("Ukupno LED-ova za stepenice: " + String(gTotalLedsStepenice));

  if (stepsArray) { 
    Serial.println("Brisanje starog stepsArray...");
    delete[] stepsArray; 
    stepsArray = nullptr; 
  }
  Serial.println("Alociram novi stepsArray za " + String(gBrojStepenica) + " stepenica...");
  stepsArray = new StepSegment[gBrojStepenica];
  if (!stepsArray) {
    Serial.println("Greška: Neuspjela alokacija stepsArray!");
    gTotalLedsStepenice = 0;
    return;
  }

  int currentIndex = 0;
  for(int i = 0; i < gBrojStepenica; i++) {
    stepsArray[i].state = TrackState::OFF;
    stepsArray[i].reverse = (gRotacijaStepenica && (i % 2 == 1)); // Rotacija svake druge stepenice
    stepsArray[i].lastUpdate = 0;
    stepsArray[i].step = 0;
    stepsArray[i].effectStartTime = 0;
    stepsArray[i].indexStart = currentIndex;
    stepsArray[i].indexCount = gVariableSteps ? gStepLengths[i] : gDefaultLedsPerStep;
    currentIndex += stepsArray[i].indexCount;
    Serial.println("Segment " + String(i) + ": Start=" + String(stepsArray[i].indexStart) + ", Count=" + String(stepsArray[i].indexCount) + ", Reverse=" + String(stepsArray[i].reverse));
  }
  Serial.println("StepsArray inicijaliziran, ukupni indeks: " + String(currentIndex));

  if (leds1) { 
    Serial.println("Brisanje starog leds1...");
    delete[] leds1; 
    leds1 = nullptr; 
  }
  Serial.println("Alociram novi leds1 za " + String(gTotalLedsStepenice) + " LED-ova...");
  leds1 = new CRGB[gTotalLedsStepenice];
  if (!leds1) {
    Serial.println("Greška: Neuspjela alokacija leds1!");
    delete[] stepsArray;
    stepsArray = nullptr;
    gTotalLedsStepenice = 0;
    return;
  }
  FastLED.addLeds<WS2811, LED1_DATA_PIN, GRB>(leds1, gTotalLedsStepenice);
  FastLED.clear(true);
  Serial.println("initStepSegments završen uspješno.");
}

void updateWipeIn_step(StepSegment& seg, CRGB* arr) {
  unsigned long now = millis();
  if(now - seg.lastUpdate < gWipeInSpeedMs) return;
  seg.lastUpdate += gWipeInSpeedMs;

  CRGB wipeColor = CRGB(gColorR, gColorG, gColorB);
  if(gEffect != 0) {
    wipeColor = CHSV(sHue1, 255, 255);
    sHue1++;
  }
  if(gEfektKreniOd == "sredina") {
    int mid = seg.indexCount/2;
    int leftIndex = mid - seg.step - 1;
    int rightIndex = mid + seg.step;
    if(leftIndex >= 0) arr[seg.indexStart + leftIndex] = wipeColor;
    if(rightIndex < seg.indexCount) arr[seg.indexStart + rightIndex] = wipeColor;
    seg.step++;
    if(leftIndex < 0 && rightIndex >= seg.indexCount) {
      seg.state = TrackState::EFFECT;
      seg.effectStartTime = now;
    }
  } else {
    int idx = seg.reverse ? (seg.indexCount-1 - seg.step) : seg.step;
    if(idx >= 0 && idx < seg.indexCount) {
      arr[seg.indexStart + idx] = wipeColor;
      seg.step++;
    } else {
      seg.state = TrackState::EFFECT;
      seg.effectStartTime = now;
    }
  }
}

void updateEffect_step(StepSegment& seg) {
  if(gStepeniceMode == "sequence") {
    return;
  }
  unsigned long now = millis();
  if((now - seg.effectStartTime) >= (gOnTimeSec * 1000UL)) {
    if(gWipeAll) {
      seg.state = TrackState::WIPE_OUT;
      seg.lastUpdate = now;
      seg.step = 0;
    } else {
      seg.state = TrackState::OFF;
    }
  }
}

void updateWipeOut_step(StepSegment& seg, CRGB* arr) {
  unsigned long now = millis();
  if(now - seg.lastUpdate < gWipeOutSpeedMs) return;
  seg.lastUpdate += gWipeOutSpeedMs;

  if(gEfektKreniOd == "sredina") {
    int mid = seg.indexCount/2;
    int leftIndex = mid - seg.step - 1;
    int rightIndex = mid + seg.step;
    if(leftIndex >= 0) arr[seg.indexStart + leftIndex] = CRGB::Black;
    if(rightIndex < seg.indexCount) arr[seg.indexStart + rightIndex] = CRGB::Black;
    seg.step++;
    if(leftIndex < 0 && rightIndex >= seg.indexCount) {
      seg.state = TrackState::OFF;
    }
  } else {
    int idx = seg.reverse ? seg.step : (seg.indexCount-1 - seg.step);
    if(idx >= 0 && idx < seg.indexCount) {
      arr[seg.indexStart + idx] = CRGB::Black;
      seg.step++;
    } else {
      seg.state = TrackState::OFF;
    }
  }
}

void applyEffectSegment(StepSegment& seg, CRGB* arr) {
  int start = seg.indexStart;
  int count = seg.indexCount;
  if(gEffect == 0) {
    for(int i=0; i<count; i++) {
      arr[start+i] = CRGB(gColorR, gColorG, gColorB);
    }
  } else {
    static CRGB temp[2048];
    if(count > 2048) return;
    for(int i=0; i<count; i++) {
      temp[i] = CRGB::Black;
    }
    applyGlobalEffect(temp, count, true);
    for(int i=0; i<count; i++) {
      arr[start+i] = temp[i];
    }
  }
}

void updateSegment(StepSegment& seg, CRGB* arr) {
  if (!arr) {
    Serial.println("Greška: arr je null u updateSegment!");
    return;
  }
  switch(seg.state) {
    case TrackState::OFF:
      break;
    case TrackState::WIPE_IN:
      updateWipeIn_step(seg, arr);
      break;
    case TrackState::EFFECT:
      applyEffectSegment(seg, arr);
      updateEffect_step(seg);
      break;
    case TrackState::WIPE_OUT:
      updateWipeOut_step(seg, arr);
      break;
  }
}

void updateAllStepSegments() {
  if (!stepsArray || !leds1 || gTotalLedsStepenice <= 0) {
    return;
  }
  for(int i=0; i<gBrojStepenica; i++) {
    updateSegment(stepsArray[i], leds1);
  }
}

void startSequence(bool forward) {
  if (!stepsArray || !leds1 || gTotalLedsStepenice <= 0) {
    return;
  }
  seqActive = true;
  seqForward = forward;
  seqState = 0;
  seqWaitStart = 0;

  if(forward) {
    seqCurrentStep = 0;
  } else {
    seqCurrentStep = gBrojStepenica - 1;
  }
  for(int i=0; i<gBrojStepenica; i++) {
    stepsArray[i].state = TrackState::OFF;
    stepsArray[i].step = 0;
    stepsArray[i].lastUpdate = millis();
  }
}

void updateSequence() {
  if (!stepsArray || !leds1 || gTotalLedsStepenice <= 0 || !seqActive) {
    return;
  }

  if(seqState == 0) {
    if(seqForward) {
      if(seqCurrentStep < gBrojStepenica) {
        if(stepsArray[seqCurrentStep].state == TrackState::OFF) {
          stepsArray[seqCurrentStep].state = TrackState::WIPE_IN;
          stepsArray[seqCurrentStep].step = 0;
          stepsArray[seqCurrentStep].lastUpdate = millis();
        }
        updateSegment(stepsArray[seqCurrentStep], leds1);
        if(stepsArray[seqCurrentStep].state == TrackState::EFFECT) {
          seqCurrentStep++;
        }
      } else {
        seqState = 1;
        seqWaitStart = millis();
      }
    } else {
      if(seqCurrentStep >= 0) {
        if(stepsArray[seqCurrentStep].state == TrackState::OFF) {
          stepsArray[seqCurrentStep].state = TrackState::WIPE_IN;
          stepsArray[seqCurrentStep].step = 0;
          stepsArray[seqCurrentStep].lastUpdate = millis();
        }
        updateSegment(stepsArray[seqCurrentStep], leds1);
        if(stepsArray[seqCurrentStep].state == TrackState::EFFECT) {
          seqCurrentStep--;
        }
      } else {
        seqState = 1;
        seqWaitStart = millis();
      }
    }
  } else if(seqState == 1) {
    unsigned long now = millis();
    if((now - seqWaitStart) >= (gOnTimeSec * 1000UL)) {
      seqState = 2;
      if(seqForward) seqCurrentStep = gBrojStepenica - 1;
      else seqCurrentStep = 0;
    } else {
      for(int i=0; i<gBrojStepenica; i++) {
        if(stepsArray[i].state == TrackState::EFFECT) {
          updateSegment(stepsArray[i], leds1);
        }
      }
    }
  } else if(seqState == 2) {
    if(seqForward) {
      if(seqCurrentStep >= 0) {
        if(stepsArray[seqCurrentStep].state == TrackState::EFFECT) {
          stepsArray[seqCurrentStep].state = TrackState::WIPE_OUT;
          stepsArray[seqCurrentStep].step = 0;
          stepsArray[seqCurrentStep].lastUpdate = millis();
        }
        updateSegment(stepsArray[seqCurrentStep], leds1);
        if(stepsArray[seqCurrentStep].state == TrackState::OFF) {
          seqCurrentStep--;
        } else {
          return;
        }
      } else {
        seqActive = false;
      }
    } else {
      if(seqCurrentStep < gBrojStepenica) {
        if(stepsArray[seqCurrentStep].state == TrackState::EFFECT) {
          stepsArray[seqCurrentStep].state = TrackState::WIPE_OUT;
          stepsArray[seqCurrentStep].step = 0;
          stepsArray[seqCurrentStep].lastUpdate = millis();
        }
        updateSegment(stepsArray[seqCurrentStep], leds1);
        if(stepsArray[seqCurrentStep].state == TrackState::OFF) {
          seqCurrentStep++;
        } else {
          return;
        }
      } else {
        seqActive = false;
      }
    }
  }
}

// IR SENZORI
void handleIrSensors() {
  unsigned long now = millis();

  if (sensorTriggered[0] && (now - gIrLast[0] > IR_DEBOUNCE)) {
    gIrLast[0] = now;
    sensorTriggered[0] = false;
    if (gInstallationType == "linija" && track1.state == TrackState::OFF) {
      track1.state = TrackState::WIPE_IN;
      track1.reverse = false;
      track1.lastUpdate = now;
      track1.step = 0;
      track1.effectStartTime = 0;
      Serial.println("Senzor 1 (Start LED1) aktiviran - WIPE_IN");
    } else if (gInstallationType == "stepenica" && gTotalLedsStepenice > 0) {
      if (gStepeniceMode == "sequence") {
        if (!seqActive) startSequence(true);
      } else {
        for (int i = 0; i < gBrojStepenica; i++) {
          if (stepsArray[i].state == TrackState::OFF) {
            stepsArray[i].state = gWipeAll ? TrackState::WIPE_IN : TrackState::EFFECT;
            stepsArray[i].step = 0;
            stepsArray[i].lastUpdate = millis();
          }
        }
      }
    }
  }

  if (sensorTriggered[1] && (now - gIrLast[1] > IR_DEBOUNCE)) {
    gIrLast[1] = now;
    sensorTriggered[1] = false;
    if (gInstallationType == "linija" && track1.state == TrackState::OFF) {
      track1.state = TrackState::WIPE_IN;
      track1.reverse = true;
      track1.lastUpdate = now;
      track1.step = 0;
      track1.effectStartTime = 0;
      Serial.println("Senzor 2 (End LED1) aktiviran - WIPE_IN");
    } else if (gInstallationType == "stepenica" && gTotalLedsStepenice > 0) {
      if (gStepeniceMode == "sequence") {
        if (!seqActive) startSequence(false);
      } else {
        for (int i = 0; i < gBrojStepenica; i++) {
          if (stepsArray[i].state == TrackState::OFF) {
            stepsArray[i].state = gWipeAll ? TrackState::WIPE_IN : TrackState::EFFECT;
            stepsArray[i].step = 0;
            stepsArray[i].lastUpdate = millis();
          }
        }
      }
    }
  }

  if (sensorTriggered[2] && (now - gIrLast[2] > IR_DEBOUNCE)) {
    gIrLast[2] = now;
    sensorTriggered[2] = false;
    if (gInstallationType == "linija" && track2.state == TrackState::OFF) {
      track2.state = TrackState::WIPE_IN;
      track2.reverse = false;
      track2.lastUpdate = now;
      track2.step = 0;
      track2.effectStartTime = 0;
      Serial.println("Senzor 3 (Start LED2) aktiviran - WIPE_IN");
    }
  }

  if (sensorTriggered[3] && (now - gIrLast[3] > IR_DEBOUNCE)) {
    gIrLast[3] = now;
    sensorTriggered[3] = false;
    if (gInstallationType == "linija" && track2.state == TrackState::OFF) {
      track2.state = TrackState::WIPE_IN;
      track2.reverse = true;
      track2.lastUpdate = now;
      track2.step = 0;
      track2.effectStartTime = 0;
      Serial.println("Senzor 4 (End LED2) aktiviran - WIPE_IN");
    }
  }
}

// ISR FUNKCIJE
void IRAM_ATTR led1SensorStartISR() { sensorTriggered[0] = true; }
void IRAM_ATTR led1SensorEndISR() { sensorTriggered[1] = true; }
void IRAM_ATTR led2SensorStartISR() { sensorTriggered[2] = true; }
void IRAM_ATTR led2SensorEndISR() { sensorTriggered[3] = true; }

// DAY/NIGHT SVJETLINA
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
  if (!getLocalTime(&tinfo)) {
    return map(gDayBrightnessPercent, 0, 100, 0, 255);
  }
  int cur = tinfo.tm_hour * 60 + tinfo.tm_min;
  int ds = parseHour(gDayStartStr) * 60 + parseMin(gDayStartStr);
  int de = parseHour(gDayEndStr) * 60 + parseMin(gDayEndStr);
  if (ds < de) {
    if (cur >= ds && cur < de) {
      return map(gDayBrightnessPercent, 0, 100, 0, 255);
    } else {
      return map(gNightBrightnessPercent, 0, 100, 0, 255);
    }
  } else {
    if (cur >= ds || cur < de) {
      return map(gDayBrightnessPercent, 0, 100, 0, 255);
    } else {
      return map(gNightBrightnessPercent, 0, 100, 0, 255);
    }
  }
}

// POSLUŽIVANJE DATOTEKA
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
  Serial.println("Datoteka nije pronađena: " + path);
  return false;
}

void handleNotFound() {
  if (!handleFileRead(server.uri())) {
    server.send(404, "text/plain", "Not found");
  }
}

// KONFIGURACIJA
bool loadConfig(JsonDocument &doc) {
  if (!SPIFFS.exists("/config.json")) return false;
  File file = SPIFFS.open("/config.json", "r");
  if (!file) return false;
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  if (error) {
    Serial.println("Greška pri učitavanju konfiguracije: " + String(error.c_str()));
    return false;
  }
  return true;
}

bool saveConfig(const JsonDocument &doc) {
  File file = SPIFFS.open("/config.json", "w");
  if (!file) return false;
  if (serializeJson(doc, file) == 0) {
    file.close();
    return false;
  }
  file.close();
  return true;
}

void setDefaultConfig(JsonDocument &doc) {
  JsonObject led1 = doc["led1"].to<JsonObject>();
  led1["type"] = "ws2811";
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

  JsonObject led2 = doc["led2"].to<JsonObject>();
  led2["type"] = "";
  led2["mode"] = "";
  led2["linijaLEDCount"] = 0;
  led2["numSteps"] = 0;
  led2["defaultLedsPerStep"] = 0;
  led2["variable"] = false;
  led2["efektKreniOd"] = "sredina";
  led2["stepMode"] = "allAtOnce";
  led2["effect"] = "";
  led2["color"] = "#ffffff";
  led2["wipeSpeed"] = 15;
  led2["onTime"] = 5;

  doc["dayStart"] = "08:00";
  doc["dayEnd"] = "20:00";
  doc["dayBr"] = 100;
  doc["nightBr"] = 30;
  doc["maxCurrent"] = 2000;
  doc["mainSwitch"] = "both";
}

void applyConfigToLEDStrips(const JsonDocument &doc) {
  Serial.println("Pokrećem applyConfigToLEDStrips...");
  if (!doc["led1"].isNull()) {
    JsonObjectConst led1Config = doc["led1"];
    gInstallationType = led1Config["mode"] | "linija";
    Serial.println("Mod: " + gInstallationType);
    if (gInstallationType == "linija") {
      gNumLeds1 = led1Config["linijaLEDCount"] | 35;
      Serial.println("LED1 traka ima " + String(gNumLeds1) + " LED-ova.");
    } else if (gInstallationType == "stepenica") {
      gBrojStepenica = led1Config["numSteps"] | 3;
      gDefaultLedsPerStep = led1Config["defaultLedsPerStep"] | 30;
      gVariableSteps = led1Config["variable"] | false;
      Serial.println("Broj stepenica: " + String(gBrojStepenica));
      Serial.println("Default LED-ova po stepenici: " + String(gDefaultLedsPerStep));
      Serial.println("Varijabilne stepenice: " + String(gVariableSteps ? "da" : "ne"));
      if (gVariableSteps && !led1Config["stepLengths"].isNull()) {
        JsonArrayConst stepLengths = led1Config["stepLengths"];
        if (gStepLengths) { delete[] gStepLengths; gStepLengths = nullptr; }
        gStepLengths = new int[gBrojStepenica];
        for (int i = 0; i < gBrojStepenica && i < stepLengths.size(); i++) {
          gStepLengths[i] = stepLengths[i].as<int>();
          Serial.println("Stepenica " + String(i) + ": " + String(gStepLengths[i]) + " LED-ova");
        }
      }
      initStepSegments();
      if (gTotalLedsStepenice == 0) {
        Serial.println("Greška u inicijalizaciji stepenica - vraćam na linija mod.");
        gInstallationType = "linija";
        gNumLeds1 = 35;
      }
    }
    gRotacijaStepenica = led1Config["rotation"] | false;
    Serial.println("Rotacija stepenica: " + String(gRotacijaStepenica ? "uključena" : "isključena"));
    gEfektKreniOd = led1Config["efektKreniOd"] | "sredina";
    gStepeniceMode = led1Config["stepMode"] | "allAtOnce";
    gEffect = led1Config["effect"] == "solid" ? 0 : led1Config["effect"] == "confetti" ? 1 : 
              led1Config["effect"] == "rainbow" ? 2 : led1Config["effect"] == "meteor" ? 3 : 
              led1Config["effect"] == "fade" ? 4 : led1Config["effect"] == "fire" ? 5 : 
              led1Config["effect"] == "lightning" ? 6 : 0;
    String colorStr = led1Config["color"] | "#002aff";
    if (colorStr.length() == 7) {
      gColorR = strtol(colorStr.substring(1,3).c_str(), NULL, 16);
      gColorG = strtol(colorStr.substring(3,5).c_str(), NULL, 16);
      gColorB = strtol(colorStr.substring(5,7).c_str(), NULL, 16);
    }
    gWipeInSpeedMs = led1Config["wipeSpeed"] | 50;
    gWipeOutSpeedMs = led1Config["wipeSpeed"] | 50;
    gOnTimeSec = led1Config["onTime"] | 2;
  }

  if (!doc["led2"].isNull()) {
    gNumLeds2 = doc["led2"]["linijaLEDCount"] | 0;
    Serial.println("LED2 traka ima " + String(gNumLeds2) + " LED-ova.");
  }

  gDayStartStr = doc["dayStart"] | "08:00";
  gDayEndStr = doc["dayEnd"] | "20:00";
  gDayBrightnessPercent = doc["dayBr"] | 100;
  gNightBrightnessPercent = doc["nightBr"] | 30;
  maxCurrent = doc["maxCurrent"] | 2000;

  if (leds1 && gInstallationType != "stepenica") { 
    Serial.println("Brisanje starog leds1...");
    delete[] leds1; 
    leds1 = nullptr; 
  }
  if (leds2) { 
    Serial.println("Brisanje starog leds2...");
    delete[] leds2; 
    leds2 = nullptr; 
  }
  if (gNumLeds1 > 0 && gInstallationType == "linija") {
    Serial.println("Alociram novi leds1 za linija mod...");
    leds1 = new CRGB[gNumLeds1];
    FastLED.addLeds<WS2811, LED1_DATA_PIN, GRB>(leds1, gNumLeds1);
  }
  if (gNumLeds2 > 0) {
    Serial.println("Alociram novi leds2 za linija mod...");
    leds2 = new CRGB[gNumLeds2];
    FastLED.addLeds<WS2811, LED2_DATA_PIN, GRB>(leds2, gNumLeds2);
  }
  FastLED.setMaxPowerInVoltsAndMilliamps(5, maxCurrent);
  FastLED.clear(true);
  Serial.println("applyConfigToLEDStrips završen.");
}

// API HANDLERI
void handleGetConfig() {
  JsonDocument doc;
  if (!loadConfig(doc)) {
    setDefaultConfig(doc);
    saveConfig(doc);
  }
  String output;
  serializeJson(doc, output);
  server.send(200, "application/json", output);
}

void handleSaveConfig() {
  if (server.hasArg("plain")) {
    String json = server.arg("plain");
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, json);
    if (error) {
      server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
      return;
    }
    saveConfig(doc);
    applyConfigToLEDStrips(doc);
    server.send(200, "application/json", "{\"status\":\"ok\"}");
  } else {
    server.send(400, "application/json", "{\"error\":\"No data received\"}");
  }
}

// SETUP
void setup() {
  Serial.begin(115200);
  delay(100);

  esp_task_wdt_init(&wdt_config);
  esp_task_wdt_add(NULL);

  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Mount Failed");
    return;
  }

  File root = SPIFFS.open("/");
  File file = root.openNextFile();
  while (file) {
    Serial.println("Datoteka: " + String(file.name()));
    file = root.openNextFile();
  }
  root.close();

  if (!loadConfig(configDoc)) {
    setDefaultConfig(configDoc);
    saveConfig(configDoc);
  }
  applyConfigToLEDStrips(configDoc);

 wifiManager.setTimeout(30); // 30 sekundi za povezivanje
if (!wifiManager.autoConnect("StairLight_AP", "12345678")) {
  Serial.println("WiFi povezivanje nije uspjelo, nastavljam bez WiFi-a...");
} else {
  Serial.println("WiFi spojen: " + WiFi.localIP().toString());
}
  Serial.println("WiFi spojen: " + WiFi.localIP().toString());

  if (MDNS.begin("stairlight")) {
    Serial.println("mDNS responder pokrenut kao stairlight.local");
    MDNS.addService("http", "tcp", 80);
    Serial.println("mDNS servis dodan: http://stairlight.local/");
  } else {
    Serial.println("Greška pri postavljanju mDNS respondera!");
  }

  server.on("/api/getConfig", HTTP_GET, handleGetConfig);
  server.on("/api/saveConfig", HTTP_POST, handleSaveConfig);
  server.on("/", HTTP_GET, [](){ handleFileRead("/index.html"); });
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server pokrenut");

  pinMode(MAIN_SWITCH_PIN, INPUT_PULLUP);
  pinMode(LED1_SENSOR_START_PIN, INPUT);
  pinMode(LED1_SENSOR_END_PIN, INPUT);
  pinMode(LED2_SENSOR_START_PIN, INPUT);
  pinMode(LED2_SENSOR_END_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(LED1_SENSOR_START_PIN), led1SensorStartISR, RISING);
  attachInterrupt(digitalPinToInterrupt(LED1_SENSOR_END_PIN), led1SensorEndISR, RISING);
  attachInterrupt(digitalPinToInterrupt(LED2_SENSOR_START_PIN), led2SensorStartISR, RISING);
  attachInterrupt(digitalPinToInterrupt(LED2_SENSOR_END_PIN), led2SensorEndISR, RISING);

  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
}

// LOOP
void loop() {
  server.handleClient();
  esp_task_wdt_reset();

  bool reading = digitalRead(MAIN_SWITCH_PIN);
  if (reading != gLastButtonState && (millis() - gLastButtonDebounce > BTN_DEBOUNCE_MS)) {
    gLastButtonDebounce = millis();
    gLastButtonState = reading;
    if (reading == LOW) {
      gManualOverride = !gManualOverride;
      if (!gManualOverride) {
        track1.state = TrackState::OFF;
        track2.state = TrackState::OFF;
        if (stepsArray) {
          for (int i = 0; i < gBrojStepenica; i++) {
            stepsArray[i].state = TrackState::OFF;
          }
        }
        if (leds1) {
          for (int i = 0; i < (gInstallationType == "linija" ? gNumLeds1 : gTotalLedsStepenice); i++) {
            leds1[i] = CRGB::Black;
          }
        }
        if (leds2) {
          for (int i = 0; i < gNumLeds2; i++) leds2[i] = CRGB::Black;
        }
        FastLED.show();
      }
    }
  }

  if (gManualOverride) {
    if (gInstallationType == "linija") {
      if (leds1) applyGlobalEffect(leds1, gNumLeds1, true);
      if (leds2) applyGlobalEffect(leds2, gNumLeds2, false);
    } else if (gTotalLedsStepenice > 0) {
      if (leds1) applyGlobalEffect(leds1, gTotalLedsStepenice, true);
    }
  } else {
    handleIrSensors();
    if (gInstallationType == "stepenica" && gTotalLedsStepenice > 0) {
      if (gStepeniceMode == "sequence") {
        updateSequence();
      } else {
        updateAllStepSegments();
      }
    } else if (gInstallationType == "linija") {
      if (leds1) updateTrack_line(track1, leds1, gNumLeds1, true);
      if (leds2) updateTrack_line(track2, leds2, gNumLeds2, false);
    }
  }

  FastLED.setBrightness(getDayNightBrightness());
  FastLED.show();
  delay(10);
}