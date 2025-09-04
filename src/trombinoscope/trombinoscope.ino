/*
  Trombinoscope – Random selection of 1..N people with a single button
  Hardware:
    - NeoPixel strip/panel (WS2812/WS2813) driven by Adafruit_NeoPixel
    - 1 push button wired to BUTTON_PIN (with INPUT_PULLUP)
    - 11 “persons” represented by 11 LEDs on the strip

  Behavior:
    - IDLE: rainbow animation on the "person" LEDs.
    - COUNTING: every short press increases the number of people to select.
      A sliding time window (COUNT_WINDOW_MS) is used: if you keep pressing,
      the counter increases; when the window expires, the total is frozen.
    - ANIMATING: short flashy animation.
    - SHOW: randomly select K unique people and light them in white.
      Hold for HOLD_MS, then return to IDLE.
    - LONG PRESS (> LONG_PRESS_MS): immediate reset (useful if you pressed by mistake).

  Notes & assumptions:
    - The LEDs representing people are defined in UsedLEDs[].
      You can change the list/order/positions. The code adapts automatically.
    - If K > number of people, we clamp to PEOPLE_NB.
    - Randomness is seeded using analogRead(A0). If your board has no floating A0,
      change the seeding method.

  Dependencies:
    - Adafruit NeoPixel (Library Manager: “Adafruit NeoPixel”)

  Author: Original code + cleanup and structuring by ChatGPT
*/

#include <Adafruit_NeoPixel.h>

// ========================== GENERAL CONFIG ==========================
#define LED_PIN          15              // Data pin for the LED strip
#define NUM_LEDS         30             // Total number of LEDs on the strip/panel
#define BUTTON_PIN       16              // Button with INPUT_PULLUP (active LOW)

// List of LEDs representing the people (indices on the strip)
const uint16_t UsedLEDs[] = {0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20};
const uint8_t  PEOPLE_NB    = sizeof(UsedLEDs) / sizeof(UsedLEDs[0]);

// Timing constants (tune to taste)
const uint16_t DEBOUNCE_MS       = 35;     // Button debounce
const uint16_t LONG_PRESS_MS     = 800;    // Long press = reset
const uint16_t COUNT_WINDOW_MS   = 900;    // Time window for multiple presses
const uint16_t IDLE_STEP_MS      = 20;     // Idle rainbow update rate
const uint16_t ANIM_STEP_MS      = 55;     // Animation update rate
const uint16_t ANIM_MS           = 1800;   // Animation duration
const uint32_t HOLD_MS           = 10000;  // How long to display the selection
const uint8_t  RAINBOW_SPACING   = 20;     // Hue spacing between people in idle

// Safety: maximum request count
const uint8_t  MAX_REQUEST       = PEOPLE_NB;

// ========================== OBJECTS & STATE ==========================
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

enum class State : uint8_t {
  IDLE,         // Idle rainbow animation
  COUNTING,     // Counting button presses
  ANIMATING,    // Short flashy animation
  SHOW          // Showing the selected people
};

State state = State::IDLE;

// Timing variables (non-blocking)
uint32_t nowMs           = 0;
uint32_t lastIdleStep    = 0;
uint32_t lastAnimStep    = 0;
uint32_t stateEnterMs    = 0;
uint32_t lastButtonEdge  = 0;  // debounce

// Button state
bool     lastButtonLevel = HIGH; // with INPUT_PULLUP, HIGH = released
bool     longPressArmed  = false;
uint32_t buttonPressMs   = 0;

// Animation
uint8_t  rainbowOffset   = 0;

// Counting presses
uint8_t  requestedCount  = 0;     // number of people requested
uint32_t countWindowEnds = 0;     // end of sliding time window

// Selection
static const uint8_t MAX_SEL = 32;
uint8_t selectedIdx[MAX_SEL];
uint8_t selectedCount = 0;

// ========================== COLOR HELPERS ==========================
uint32_t colorWheel(uint8_t pos) {
  pos = 255 - pos;
  if (pos < 85) {
    return strip.Color(255 - pos * 3, 0, pos * 3);
  } else if (pos < 170) {
    pos -= 85;
    return strip.Color(0, pos * 3, 255 - pos * 3);
  } else {
    pos -= 170;
    return strip.Color(pos * 3, 255 - pos * 3, 0);
  }
}

// ========================== SELECTION HELPERS ==========================
// Returns true if val is already in arr[0..len-1]
bool contains(const uint8_t* arr, uint8_t len, uint8_t val) {
  for (uint8_t i = 0; i < len; i++) if (arr[i] == val) return true;
  return false;
}

// Pick k unique random indices in [0..PEOPLE_NB-1] into out[]
void pickUniqueRandom(uint8_t k, uint8_t* out) {
  uint8_t count = 0;
  while (count < k) {
    uint8_t r = (uint8_t)random(0, PEOPLE_NB);
    if (!contains(out, count, r)) {
      out[count++] = r;
    }
  }
}

// ========================== BUTTON HANDLING ==========================
// Returns: 1 = short press, 2 = long press, 0 = nothing
uint8_t pollButton() {
  uint8_t evt = 0;

  bool level = digitalRead(BUTTON_PIN); // HIGH = released, LOW = pressed

  // Edge detection with debounce
  if (level != lastButtonLevel) {
    uint32_t t = nowMs;
    if (t - lastButtonEdge > DEBOUNCE_MS) {
      lastButtonEdge = t;

      if (level == LOW) {
        // Button pressed
        buttonPressMs = t;
        longPressArmed = true;
      } else {
        // Button released
        if (longPressArmed) {
          uint32_t pressDur = t - buttonPressMs;
          if (pressDur >= LONG_PRESS_MS) {
            evt = 2; // long
          } else {
            evt = 1; // short
          }
          longPressArmed = false;
        }
      }
      lastButtonLevel = level;
    }
  }

  // Detect long press if held down
  if (longPressArmed && (nowMs - buttonPressMs >= LONG_PRESS_MS) && (digitalRead(BUTTON_PIN) == LOW)) {
    evt = 2; // long
    longPressArmed = false;
  }

  return evt;
}

// ========================== RENDERING ==========================
void renderIdle() {
  if (nowMs - lastIdleStep >= IDLE_STEP_MS) {
    rainbowOffset++;
    for (uint8_t i = 0; i < PEOPLE_NB; i++) {
      uint16_t led = UsedLEDs[i];
      strip.setPixelColor(led, colorWheel(rainbowOffset + i * RAINBOW_SPACING));
    }
    strip.show();
    lastIdleStep = nowMs;
  }
}

void renderAnimating() {
  if (nowMs - lastAnimStep >= ANIM_STEP_MS) {
    for (uint8_t i = 0; i < PEOPLE_NB; i++) {
      uint16_t led = UsedLEDs[i];
      strip.setPixelColor(led, strip.Color(random(0, 255), random(0, 255), random(0, 255)));
    }
    strip.show();
    lastAnimStep = nowMs;
  }
}

void renderShowSelection() {
  strip.clear();
  for (uint8_t i = 0; i < selectedCount; i++) {
    uint16_t led = UsedLEDs[selectedIdx[i]];
    strip.setPixelColor(led, strip.Color(255, 255, 255));
  }
  strip.show();
}

// ========================== STATE TRANSITIONS ==========================
void enterState(State s) {
  state = s;
  stateEnterMs = nowMs;

  switch (state) {
    case State::IDLE: break;
    case State::COUNTING: countWindowEnds = nowMs + COUNT_WINDOW_MS; break;
    case State::ANIMATING: lastAnimStep = 0; break;
    case State::SHOW: renderShowSelection(); break;
  }
}

// ========================== SETUP / LOOP ==========================
void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  strip.begin();
  strip.clear();
  strip.show();

  // Safety: ensure indices fit in strip
  for (uint8_t i = 0; i < PEOPLE_NB; i++) {
    if (UsedLEDs[i] >= NUM_LEDS) {
      while (true) { /* Fix UsedLEDs[] or NUM_LEDS */ }
    }
  }

  randomSeed(analogRead(A0));
  enterState(State::IDLE);
}

void loop() {
  nowMs = millis();
  uint8_t btn = pollButton();

  switch (state) {
    case State::IDLE:
      renderIdle();
      if (btn == 2) { strip.clear(); strip.show(); }
      else if (btn == 1) { requestedCount = 1; enterState(State::COUNTING); }
      break;

    case State::COUNTING:
      renderIdle();
      if (btn == 2) { requestedCount = 0; enterState(State::IDLE); break; }
      if (btn == 1) {
        if (requestedCount < MAX_REQUEST) requestedCount++;
        countWindowEnds = nowMs + COUNT_WINDOW_MS;
      }
      if ((int32_t)(nowMs - countWindowEnds) >= 0) {
        if (requestedCount == 0) requestedCount = 1;
        if (requestedCount > PEOPLE_NB) requestedCount = PEOPLE_NB;
        enterState(State::ANIMATING);
      }
      break;

    case State::ANIMATING:
      renderAnimating();
      if (btn == 2) { enterState(State::IDLE); break; }
      if ((nowMs - stateEnterMs) >= ANIM_MS) {
        selectedCount = (requestedCount > PEOPLE_NB) ? PEOPLE_NB : requestedCount;
        if (selectedCount > MAX_SEL) selectedCount = MAX_SEL;
        pickUniqueRandom(selectedCount, selectedIdx);
        enterState(State::SHOW);
      }
      break;

    case State::SHOW:
      if (btn == 2) { enterState(State::IDLE); break; }
      if (nowMs - stateEnterMs >= HOLD_MS) { enterState(State::IDLE); }
      break;
  }
}
