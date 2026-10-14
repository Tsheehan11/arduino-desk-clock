/*
  Desk Clock + Alarm + Timer + Stopwatch (rotary dial version)
  Arduino UNO + DS1307 RTC + 74HC595 + rotary encoder + active buzzer
  + RC522 RFID reader (tap a card to turn the alarm off)

  Libraries needed (Library Manager):
    - RTClib (by Adafruit)
    - MFRC522 (by GithubCommunity)

  === WIRING ===
  Everything from desk_clock.ino stays exactly the same:
    74HC595: 16,10 -> 5V | 8,13 -> GND | 14 -> D2 | 12 -> D3 | 11 -> D4
    Segments: Q0..Q7 -> A..DP on both displays
    Digits:   D5..D8 -> 220ohm -> 4-digit pins 12, 9, 8, 6
              D9     -> 220ohm -> 1-digit pin 3 or 8
    RTC:      VCC -> 5V, GND -> GND, SDA -> A4, SCL -> A5
  Rotary encoder (female-to-male wires):
    CLK -> A0, DT -> A1, SW -> A2, + -> 5V (+ rail), GND -> GND (- rail)
  Active buzzer: + leg -> A3, - leg -> GND   (it moved here from pin 13)
  RC522 RFID reader (female-to-male wires). NOT the kit lesson's wiring:
  pin 9 is already a display digit, so RST goes to 3.3V instead.
    SDA -> D10, SCK -> D13, MOSI -> D11, MISO -> D12,
    RST -> 3.3V, 3.3V -> 3.3V, GND -> GND, IRQ -> not connected
    Power it from 3.3V, not 5V.

  === HOW TO USE ===
  TURN the knob to pick a screen:   Clock -> Timer -> Stopwatch -> Alarm -> Clock ...
    (turning the other way goes backwards)
    The small digit shows where you are: A/P = clock, t = timer, S = stopwatch,
    L = alarm.
  PRESS on the Timer, Stopwatch or Alarm screen to go INTO it.

  Inside the ALARM:
    Turn            set the alarm time, 5 min a click (spin fast: 1 hour)
    Press           done: alarm ON, step back out (shows " On" for a second)
    Hold 1 second   alarm OFF, step back out (shows " OFF" for a second)
    The alarm screen always shows the alarm time. A dot on the last digit
    means the alarm is on (same dot as on the clock); no dot means off.
    The small digit shows A/P for the alarm time. The alarm time is saved in
    the RTC's battery-backed memory, so unplugging the clock doesn't lose it.

  Pairing your card: go into the Alarm screen and tap the card on the reader.
    The display says "CArd" and it beeps. That card is saved in the RTC's
    memory; tap a different card the same way to replace it.

  When the ALARM goes off, it beeps and flashes the clock. No snooze.
    Tap your card   turn it off until tomorrow
    Press / turn    nothing (pressing just shows "CArd" to remind you)
    Unplugging the clock also stops it.
    No card paired yet? Then a press turns it off, so you're never stuck.

  Inside the TIMER:
    Turn            set the time (only while it isn't running)
                      under 1 min   5 s     (spin fast: 15 s)
                      1 - 3 min    10 s     (spin fast: 30 s)
                      3 - 5 min    30 s     (spin fast: 1 min)
                      5 - 60 min    1 min   (spin fast: 5 min)
                      1 hour +      5 min   (spin fast: 30 min)
    Press           start / pause
    Hold 1 second   once started (running or paused): reset to 00.00
                    while you're still setting it: step back out, keeping the time
    Turn            while it's running: step back out (it keeps running)

  Inside the STOPWATCH:
    Press           start / stop
    Hold 1 second   reset to 00.00  (hold again at 00.00 to step back out)
    Turn            step back out (it keeps running)

  When the timer finishes, it beeps and flashes for up to 30 seconds; any press
  or turn silences it.
  Just peeking at a screen (not pressed in)? It goes back to the clock after
  5 seconds of not touching the knob, even if something is running.
  Pressed in, it stays put, unless it's sitting empty at 00.00 or you're on
  the alarm screen (then 10 s).

  === WHAT THE DISPLAY SHOWS ===
  Clock:       HH.MM, small digit A/P. A dot on the A/P means the timer or
               stopwatch is going in the background (blinking = running,
               steady = paused).
               A dot on the last big digit means the alarm is on
               (and will go off at the alarm time).
  Alarm, from the outside: the alarm time, small digit "L", dot on the last digit if on.
  Timer / stopwatch, from the outside: live time, small digit "t" or "S".
  Inside, before starting: the "t" / "S" blinks.
  Inside, counting:
    under 1 hour:   MM.SS, small digit = tenths of a second
    1 hour or more: HH.MM, small digit = ring of 6 segments, 10 s each
                    (timer: the ring empties; stopwatch: it fills up)
  Paused / stopped: the big digits flash.
  The middle dot tells you the units, even while setting:
    steady dot   = MM.SS (minutes . seconds)
    blinking dot = HH.MM (hours . minutes), same as the clock
*/

#include <Wire.h>
#include <RTClib.h>
#include <SPI.h>
#include <MFRC522.h>

// ---- 74HC595 pins ----
const int DATA_PIN  = 2; // DS
const int LATCH_PIN = 3; // ST_CP
const int CLOCK_PIN = 4; // SH_CP

// ---- Digit-select pins (active LOW) ----
// index 0-3 = the 4 big digits (left to right), index 4 = small digit
const int DIGIT_PINS[5] = {5, 6, 7, 8, 9};

// ---- Rotary encoder + buzzer ----
const int ENC_CLK_PIN = A0;
const int ENC_DT_PIN  = A1;
const int ENC_SW_PIN  = A2;
const int BUZZER_PIN  = A3;

// ---- RFID reader (SPI: D11 MOSI, D12 MISO, D13 SCK are fixed by the Uno) ----
const int RFID_SS_PIN = 10;
MFRC522 rfid(RFID_SS_PIN, MFRC522::UNUSED_PIN); // RST is wired to 3.3V
const bool ENC_REVERSE = true;  // flip this if turning right goes the wrong way

RTC_DS1307 rtc;

// Standard 7-segment codes, bit order (MSB..LSB) = DP,G,F,E,D,C,B,A
const byte digitCode[10] = {
  0x3F, 0x06, 0x5B, 0x4F, 0x66,
  0x6D, 0x7D, 0x07, 0x7F, 0x6F
};
const byte SEG_BLANK = 0x00;
const byte SEG_A     = 0x77; // "A" for AM
const byte SEG_P     = 0x73; // "P" for PM
const byte SEG_T     = 0x78; // "t" for timer
const byte SEG_S     = 0x6D; // "S" for stopwatch
const byte SEG_L     = 0x38; // "L" for alarm
const byte SEG_O     = 0x3F; // "O" (for OFF)
const byte SEG_F     = 0x71; // "F" (for OFF)
const byte SEG_N     = 0x54; // "n" (for On)
const byte SEG_C     = 0x39; // "C" (for CArd)
const byte SEG_R     = 0x50; // "r" (for CArd)
const byte SEG_D     = 0x5E; // "d" (for CArd)
const byte DOT       = 0x80;

byte displayBuffer[5];
unsigned long lastDisplayUpdate = 0;
unsigned long lastMuxStep = 0;
int muxIndex = 0;

// ---- Screens ----
enum Screen { CLOCK_SCREEN, TIMER_SCREEN, STOPWATCH_SCREEN, ALARM_SCREEN };
const int NUM_SCREENS = 4;
Screen screen = CLOCK_SCREEN;
bool inside = false;        // true once you've pressed into a screen

const unsigned long MINUTE_MS     = 60000UL;
const unsigned long HOUR_MS       = 3600000UL;
const unsigned long TIMER_BEEP_MS      = 30000UL; // beep for 30 s max
const unsigned long HOLD_MS       = 1000UL;  // press-and-hold time
const unsigned long PEEK_MS       = 5000UL;  // looking at timer/stopwatch without pressing in -> back to clock
const unsigned long IDLE_MS       = 10000UL; // pressed in but left at 00.00 -> back to clock
const unsigned long FAST_SPIN_MS  = 60UL;    // clicks closer than this = fast spin
const unsigned long EXIT_LOCK_MS  = 500UL;   // after stepping out, ignore the rest of that spin
unsigned long lastExitMs = 0;
unsigned long lastInputMs = 0;

// ---- Timer state ----
const unsigned long MAX_TIMER_MS = 99UL * HOUR_MS + 55UL * MINUTE_MS; // 99h 55m
unsigned long timerRemainingMs = 0;
unsigned long timerLastTick = 0;
bool timerRunning = false;
bool timerPaused = false;   // paused partway through (makes it flash)
bool timerBeeping = false;
unsigned long timerBeepStart = 0;

// ---- Stopwatch state ----
const unsigned long MAX_STOPWATCH_MS = 99UL * HOUR_MS + 59UL * MINUTE_MS + 59999UL; // 99:59:59.9
unsigned long swElapsedMs = 0;
unsigned long swLastTick = 0;
bool swRunning = false;
bool swPaused = false;      // stopped with time on it (makes it flash)

// ---- Alarm state ----
const int MINUTES_PER_DAY = 24 * 60;
const int ALARM_STEP_MIN      = 5;    // one click while setting
const int ALARM_FAST_STEP_MIN = 60;   // one click while spinning fast
int alarmMinuteOfDay = 7 * 60;        // 7:00 AM until you set it
bool alarmEnabled = false;
bool alarmRinging = false;            // rings until the card (no time limit, no snooze)
unsigned long ringStart = 0;
int lastClockMinute = -1;             // last minute-of-day read from the RTC
unsigned long lastAlarmCheck = 0;

// Short messages that cover the display for a second (" On", " OFF", "CArd")
enum StatusMsg { MSG_ON, MSG_OFF, MSG_CARD };
StatusMsg statusMsg = MSG_ON;
unsigned long statusUntil = 0;
const unsigned long STATUS_MS = 1000UL;

// ---- RFID card ----
byte cardUid[10];                     // the paired card's ID
byte cardUidSize = 0;                 // 0 = no card paired yet
bool rfidFound = false;               // reader answered at startup
unsigned long lastCardPoll = 0;
const unsigned long CARD_POLL_MS = 150UL;
unsigned long confirmBeepUntil = 0;   // short beep when a card is paired

// Where the alarm is saved in the DS1307's battery-backed memory
//   0 magic, 1 hour, 2 minute, 3 on/off, 4 card ID length, 5-14 card ID
const uint8_t NVRAM_MAGIC = 0xA1;     // "this memory holds a saved alarm"

// ---- Debounced knob button ----
// (Defined up here, before any functions, so the Arduino IDE can see it
//  when it auto-generates function declarations.)
struct Button {
  int pin;
  bool stable;              // HIGH = not pressed (pull-up)
  bool lastRead;
  unsigned long lastChange;
  unsigned long downSince;
  bool wentDown;            // true for one loop when pressed
  bool wentUp;              // true for one loop when released
};

Button knobBtn = {ENC_SW_PIN, HIGH, HIGH, 0, 0, false, false};
bool holdHandled = false;

// ---- Rotary encoder decoding ----
// Looks at the last two CLK/DT readings to tell which way the knob moved.
// Adding these up and only counting a click when the knob settles in a
// detent (both pins HIGH) ignores the contact bounce these knobs have.
const int8_t ENC_TABLE[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};
uint8_t encHistory = 0b11;
int8_t encAccum = 0;
unsigned long lastClickMs = 0;

int readEncoderClicks() {
  uint8_t pins = (digitalRead(ENC_CLK_PIN) << 1) | digitalRead(ENC_DT_PIN);
  encHistory = ((encHistory << 2) | pins) & 0x0F;
  encAccum += ENC_TABLE[encHistory];
  if (pins == 0b11) {              // knob resting in a detent
    int clicks = 0;
    if (encAccum >= 2) clicks = 1;
    else if (encAccum <= -2) clicks = -1;
    encAccum = 0;
    return ENC_REVERSE ? -clicks : clicks;
  }
  return 0;
}

void updateButton(Button &b) {
  b.wentDown = false;
  b.wentUp = false;
  bool reading = digitalRead(b.pin);
  unsigned long now = millis();
  if (reading != b.lastRead) {
    b.lastRead = reading;
    b.lastChange = now;
  }
  if (reading != b.stable && now - b.lastChange >= 30) { // 30 ms debounce
    b.stable = reading;
    if (reading == LOW) {
      b.wentDown = true;
      b.downSince = now;
    } else {
      b.wentUp = true;
    }
  }
}

// ---- Display helpers ----
void writeSegments(byte pattern) {
  digitalWrite(LATCH_PIN, LOW);
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, pattern);
  digitalWrite(LATCH_PIN, HIGH);
}

bool blinkPhase(unsigned long periodMs) {   // true for the "off" half of a blink
  return (millis() / periodMs) % 2 == 1;
}

void showClock() {
  DateTime now = rtc.now();

  int hour24 = now.hour();
  bool isPM = (hour24 >= 12);
  int hour = hour24 % 12;
  if (hour == 0) hour = 12;
  int minute = now.minute();
  bool dotOn = (now.second() % 2 == 0);

  displayBuffer[0] = digitCode[hour / 10];
  displayBuffer[1] = digitCode[hour % 10] | (dotOn ? DOT : 0);
  displayBuffer[2] = digitCode[minute / 10];
  displayBuffer[3] = digitCode[minute % 10];

  // Timer or stopwatch in the background? Dot on the A/P digit:
  // blinking while something is running, steady while paused/stopped.
  bool bgDot;
  if (timerRunning || swRunning) bgDot = !blinkPhase(500);
  else bgDot = (timerRemainingMs > 0) || (swElapsedMs > 0);
  displayBuffer[4] = (isPM ? SEG_P : SEG_A) | (bgDot ? DOT : 0);

  // Alarm on? Dot on the last big digit.
  if (alarmEnabled) displayBuffer[3] |= DOT;

  // Alarm going off: flash the big digits
  if (alarmRinging && blinkPhase(250)) {
    for (int i = 0; i < 4; i++) displayBuffer[i] = SEG_BLANK;
  }
}

void showStatus(StatusMsg msg, unsigned long ms) {
  statusMsg = msg;
  statusUntil = millis() + ms;
}

bool statusShowing() {
  return (long)(statusUntil - millis()) > 0;
}

void showStatusMessage() {
  const byte on[4]   = {SEG_BLANK, SEG_O, SEG_N, SEG_BLANK};  // " On"
  const byte off[4]  = {SEG_BLANK, SEG_O, SEG_F, SEG_F};      // " OFF"
  const byte card[4] = {SEG_C, SEG_A, SEG_R, SEG_D};          // "CArd"
  const byte *text = (statusMsg == MSG_ON) ? on : (statusMsg == MSG_OFF) ? off : card;
  for (int i = 0; i < 4; i++) displayBuffer[i] = text[i];
  displayBuffer[4] = SEG_L;
}

void showAlarm() {
  int hour24 = alarmMinuteOfDay / 60;
  int minute = alarmMinuteOfDay % 60;
  int hour = hour24 % 12;
  if (hour == 0) hour = 12;
  displayBuffer[0] = digitCode[hour / 10];
  displayBuffer[1] = digitCode[hour % 10] | (blinkPhase(500) ? 0 : DOT); // blinking dot = hours.minutes
  displayBuffer[2] = digitCode[minute / 10];
  displayBuffer[3] = digitCode[minute % 10] | (alarmEnabled ? DOT : 0); // on: same dot as on the clock
  if (!inside) displayBuffer[4] = SEG_L;             // outside: just say which screen this is
  else displayBuffer[4] = (hour24 >= 12) ? SEG_P : SEG_A; // inside: AM/PM of the alarm time
}

// Ring on the small digit: the 6 outer segments are 10 seconds each.
// Segment bits: A=top, B=top-right, C=bottom-right, D=bottom, E=bottom-left, F=top-left
byte ringEmptying(unsigned long msIntoMinute) {      // timer: top segment goes out first
  int lit = (msIntoMinute + 9999UL) / 10000UL;       // 0..6 left
  return 0x3F & ~((1 << (6 - lit)) - 1);
}
byte ringFilling(unsigned long msIntoMinute) {       // stopwatch: fills from the top
  int lit = msIntoMinute / 10000UL + 1;              // 1..6 lit
  return (1 << lit) - 1;
}

// Shared layout for the timer and stopwatch screens.
//   ms        time to show
//   letter    SEG_T or SEG_S
//   counting  started (running or paused), so show tenths / ring
//   paused    flash the big digits
//   filling   ring fills (stopwatch) instead of emptying (timer)
void showTime(unsigned long ms, byte letter, bool counting, bool paused, bool filling) {
  unsigned long totalSec = ms / 1000UL;
  byte detail;

  if (totalSec >= 3600UL) {
    // 1 hour or more: HH.MM
    int hours   = totalSec / 3600UL;
    int minutes = (totalSec % 3600UL) / 60;
    displayBuffer[0] = digitCode[hours / 10];
    displayBuffer[1] = digitCode[hours % 10] | (blinkPhase(500) ? 0 : DOT); // blinking dot = hours.minutes
    displayBuffer[2] = digitCode[minutes / 10];
    displayBuffer[3] = digitCode[minutes % 10];
    detail = filling ? ringFilling(ms % 60000UL) : ringEmptying(ms % 60000UL);
  } else {
    // Under 1 hour: MM.SS
    int minutes = totalSec / 60;
    int seconds = totalSec % 60;
    displayBuffer[0] = digitCode[minutes / 10];
    displayBuffer[1] = digitCode[minutes % 10] | DOT;
    displayBuffer[2] = digitCode[seconds / 10];
    displayBuffer[3] = digitCode[seconds % 10];
    detail = digitCode[(ms % 1000UL) / 100];        // tenths of a second
  }

  // Small digit
  if (!inside) {
    displayBuffer[4] = letter;                       // outside: just say which screen this is
  } else if (counting) {
    displayBuffer[4] = detail;                       // inside and going: tenths / ring
  } else {
    displayBuffer[4] = blinkPhase(300) ? SEG_BLANK : letter; // inside, not started: blink the letter
  }

  // Paused / stopped: flash the big digits
  if (paused && blinkPhase(400)) {
    for (int i = 0; i < 4; i++) displayBuffer[i] = SEG_BLANK;
  }
}

void showTimer() {
  if (timerBeeping && blinkPhase(250)) {                  // alarm: flash everything
    for (int i = 0; i < 4; i++) displayBuffer[i] = SEG_BLANK;
    displayBuffer[4] = SEG_T;
    return;
  }
  showTime(timerRemainingMs, SEG_T, timerRunning || timerPaused, timerPaused, false);
}

void showStopwatch() {
  showTime(swElapsedMs, SEG_S, swRunning || swPaused, swPaused, true);
}

// ---- Timer ----
// How big one click of the knob is, depending on how much time is set.
// Change these numbers (in seconds) if you want different steps.
unsigned long stepSecondsFor(unsigned long ms, bool fast) {
  if (ms < 1UL * MINUTE_MS) return fast ?   15 :   5;  // under 1 min
  if (ms < 3UL * MINUTE_MS) return fast ?   30 :  10;  // 1-3 min
  if (ms < 5UL * MINUTE_MS) return fast ?   60 :  30;  // 3-5 min
  if (ms < HOUR_MS)         return fast ?  300 :  60;  // 5-60 min
  return                           fast ? 1800 : 300;  // 1 hour +
}

// Every turn lands on a multiple of the step, so you always get round numbers
// (e.g. 0:55 -> 1:00 -> 1:10 ... 58:00 -> 59:00 -> 1:00:00 -> 1:05:00).
void stepTimer(int direction, bool fast) {
  timerPaused = false;  // adjusting a paused timer stops the flashing
  if (direction > 0) {
    unsigned long step = stepSecondsFor(timerRemainingMs, fast) * 1000UL;
    unsigned long next = (timerRemainingMs / step + 1) * step;   // up to the next whole step
    timerRemainingMs = (next > MAX_TIMER_MS) ? MAX_TIMER_MS : next;
  } else if (direction < 0 && timerRemainingMs > 0) {
    // Going down uses the step size of the range just below, so 1:00 -> 0:55
    unsigned long step = stepSecondsFor(timerRemainingMs - 1, fast) * 1000UL;
    timerRemainingMs = ((timerRemainingMs + step - 1) / step - 1) * step; // down to the previous whole step
  }
}

void updateTimer() {
  if (!timerRunning) return;
  unsigned long now = millis();
  unsigned long elapsed = now - timerLastTick;
  timerLastTick = now;

  if (elapsed >= timerRemainingMs) {
    timerRemainingMs = 0;
    timerRunning = false;
    timerBeeping = true;
    timerBeepStart = now;
    if (!alarmRinging) {     // (the alarm going off keeps the screen)
      screen = TIMER_SCREEN; // pop up the timer, wherever you were
      inside = false;
      lastInputMs = now;
    }
  } else {
    timerRemainingMs -= elapsed;
  }
}

void updateBuzzer() {
  if (timerBeeping && millis() - timerBeepStart >= TIMER_BEEP_MS) {
    timerBeeping = false; // the timer gives up after 30 s (the alarm never does)
  }
  bool beep = timerBeeping && ((millis() - timerBeepStart) / 200) % 2 == 0; // timer: 200 ms on/off
  if (alarmRinging) {
    // Alarm: beep-beep-beep, pause (sounds different from the timer)
    unsigned long t = (millis() - ringStart) % 1000UL;
    if (t < 500 && (t / 100) % 2 == 0) beep = true;
  }
  if ((long)(confirmBeepUntil - millis()) > 0) beep = true; // card paired
  digitalWrite(BUZZER_PIN, beep ? HIGH : LOW);
}

// ---- Alarm ----
void saveAlarm() {        // DS1307 memory has no write limit, so saving every change is fine
  rtc.writenvram(0, NVRAM_MAGIC);
  rtc.writenvram(1, alarmMinuteOfDay / 60);
  rtc.writenvram(2, alarmMinuteOfDay % 60);
  rtc.writenvram(3, alarmEnabled ? 1 : 0);
}

void loadAlarm() {
  if (rtc.readnvram(0) != NVRAM_MAGIC) return;      // nothing saved yet: keep 7:00 AM, off
  uint8_t h = rtc.readnvram(1);
  uint8_t m = rtc.readnvram(2);
  if (h >= 24 || m >= 60) return;
  alarmMinuteOfDay = h * 60 + m;
  alarmEnabled = rtc.readnvram(3) == 1;
  uint8_t n = rtc.readnvram(4);
  if (n == 4 || n == 7 || n == 10) {                // valid card ID lengths
    for (int i = 0; i < n; i++) cardUid[i] = rtc.readnvram(5 + i);
    cardUidSize = n;
  }
}

void saveCard() {
  saveAlarm();                                      // makes sure the magic byte is there
  rtc.writenvram(4, cardUidSize);
  for (int i = 0; i < cardUidSize; i++) rtc.writenvram(5 + i, cardUid[i]);
}

// ---- RFID ----
// Only talks to the reader when it matters (alarm ringing, or
// pairing on the alarm screen), a few times a second. Each check briefly
// pauses the display, so the digits are switched off first to avoid a flicker.
void updateCard() {
  if (!rfidFound) return;
  bool waitingForCard = alarmRinging;
  bool pairing = inside && screen == ALARM_SCREEN;
  if (!waitingForCard && !pairing) return;
  unsigned long now = millis();
  if (now - lastCardPoll < CARD_POLL_MS) return;
  lastCardPoll = now;

  digitalWrite(DIGIT_PINS[muxIndex], HIGH);         // display off for the few ms this takes
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) return;
  byte n = rfid.uid.size;
  rfid.PICC_HaltA();                                // card stays quiet until it's taken away
  if (n > 10) return;

  if (waitingForCard) {
    bool match = (cardUidSize > 0 && n == cardUidSize);
    for (int i = 0; match && i < n; i++) if (rfid.uid.uidByte[i] != cardUid[i]) match = false;
    if (match) {                                    // the right card: off until tomorrow
      alarmRinging = false;
      showStatus(MSG_OFF, STATUS_MS);
    }
  } else {                                          // pairing: remember this card
    for (int i = 0; i < n; i++) cardUid[i] = rfid.uid.uidByte[i];
    cardUidSize = n;
    saveCard();
    showStatus(MSG_CARD, STATUS_MS);
    confirmBeepUntil = now + 150;
    lastInputMs = now;
  }
}

// Same idea as the timer: every click lands on a round number, wrapping past midnight.
void stepAlarm(int direction, bool fast) {
  int step = fast ? ALARM_FAST_STEP_MIN : ALARM_STEP_MIN;
  int t = alarmMinuteOfDay;
  if (direction > 0) t = (t / step + 1) * step;           // up to the next whole step
  else               t = ((t + step - 1) / step - 1) * step; // down to the previous one
  alarmMinuteOfDay = (t + MINUTES_PER_DAY) % MINUTES_PER_DAY;
  alarmEnabled = true;     // setting a time means you want it on
  saveAlarm();
}

void startRinging(unsigned long now) {
  alarmRinging = true;
  ringStart = now;
  screen = CLOCK_SCREEN;   // show the clock, wherever you were
  inside = false;
  lastInputMs = now;
}

void updateAlarm() {
  unsigned long now = millis();

  // Check the RTC twice a second. Ring when the clock ROLLS OVER into the alarm
  // minute, so it doesn't ring right away if you set it for the current minute
  // (or if the clock powers up during that minute).
  if (now - lastAlarmCheck < 500) return;
  lastAlarmCheck = now;
  DateTime t = rtc.now();
  int minuteOfDay = t.hour() * 60 + t.minute();
  if (lastClockMinute >= 0 && minuteOfDay != lastClockMinute &&
      minuteOfDay == alarmMinuteOfDay && alarmEnabled && !alarmRinging) {
    startRinging(now);
  }
  lastClockMinute = minuteOfDay;
}

// ---- Stopwatch ----
void updateStopwatch() {
  if (!swRunning) return;
  unsigned long now = millis();
  swElapsedMs += now - swLastTick;
  swLastTick = now;
  if (swElapsedMs >= MAX_STOPWATCH_MS) {   // ran out of digits: stop at 99:59:59
    swElapsedMs = MAX_STOPWATCH_MS;
    swRunning = false;
    swPaused = true;
  }
}

// ---- Knob ----
// byTurn: stepping out by turning the knob ignores the rest of that spin,
// so one turn doesn't step out AND jump to another screen.
void stepOut(unsigned long now, bool byTurn) {
  inside = false;
  if (byTurn) lastExitMs = now;
}

void handleInput() {
  int clicks = readEncoderClicks();
  updateButton(knobBtn);
  unsigned long now = millis();

  if (clicks != 0 || knobBtn.wentDown) lastInputMs = now;

  // ---- Alarm ringing: only the card stops it ----
  if (alarmRinging) {
    if (knobBtn.wentDown) {
      holdHandled = true;          // this press does nothing else, even after it stops
      if (cardUidSize == 0) {
        alarmRinging = false;      // no card paired yet: a press turns it off
        showStatus(MSG_OFF, STATUS_MS);
      } else {
        showStatus(MSG_CARD, STATUS_MS); // card paired: "go get the card"
      }
    }
    return;                        // turning while it rings does nothing
  }

  // Any press or turn while the timer is beeping just silences it.
  if (timerBeeping && (clicks != 0 || knobBtn.wentDown)) {
    timerBeeping = false;
    holdHandled = true; // ignore the rest of this press
    return;
  }

  // ---- Turning ----
  if (clicks != 0) {
    bool fast = (now - lastClickMs) < FAST_SPIN_MS;
    lastClickMs = now;

    if (inside) {
      if (screen == TIMER_SCREEN && !timerRunning) {
        stepTimer(clicks, fast);               // setting the timer
      } else if (screen == ALARM_SCREEN) {
        statusUntil = now;                     // turning: back to the time right away
        stepAlarm(clicks, fast);               // setting the alarm
      } else {
        stepOut(now, true);                    // running timer or stopwatch: step back out
      }
    } else if (now - lastExitMs >= EXIT_LOCK_MS) {
      // Outside: cycle Clock -> Timer -> Stopwatch -> Alarm (backwards if turning the other way)
      screen = (Screen)((screen + NUM_SCREENS + clicks) % NUM_SCREENS);
    }
  }

  if (knobBtn.wentDown) holdHandled = false;

  // ---- Hold 1 second: reset, or step out if already at 00.00 ----
  if (knobBtn.stable == LOW && !holdHandled && now - knobBtn.downSince >= HOLD_MS) {
    holdHandled = true;
    if (inside && screen == TIMER_SCREEN) {
      if (timerRunning || timerPaused) {         // started: reset it
        // (if you're still setting it, holding just steps out and keeps the time)
        timerRunning = false;
        timerPaused = false;
        timerRemainingMs = 0;
      } else {
        stepOut(now, false);
      }
    } else if (inside && screen == STOPWATCH_SCREEN) {
      if (swRunning || swElapsedMs > 0) {
        swRunning = false;
        swPaused = false;
        swElapsedMs = 0;
      } else {
        stepOut(now, false);
      }
    } else if (inside && screen == ALARM_SCREEN) {
      alarmEnabled = false;                      // hold: alarm off, step out
      showStatus(MSG_OFF, STATUS_MS);
      saveAlarm();
      stepOut(now, false);
    }
  }

  // ---- Short press (let go before the hold kicked in) ----
  if (knobBtn.wentUp && !holdHandled) {
    if (!inside) {
      if (screen != CLOCK_SCREEN) inside = true;   // go into the timer / stopwatch / alarm
    } else if (screen == TIMER_SCREEN) {
      if (timerRunning) {
        timerRunning = false;                    // pause
        timerPaused = true;
      } else if (timerRemainingMs > 0) {
        timerRunning = true;                     // start / resume
        timerPaused = false;
        timerLastTick = now;
      }
    } else if (screen == STOPWATCH_SCREEN) {
      if (swRunning) {
        swRunning = false;                       // stop
        swPaused = true;
      } else {
        swRunning = true;                        // start / continue
        swPaused = false;
        swLastTick = now;
      }
    } else if (screen == ALARM_SCREEN) {
      alarmEnabled = true;                       // press: done, alarm on, step out
      showStatus(MSG_ON, STATUS_MS);
      saveAlarm();
      stepOut(now, false);
    }
  }

  // ---- Going back to the clock on its own ----
  if (!timerBeeping && screen != CLOCK_SCREEN) {
    unsigned long idle = now - lastInputMs;
    if (!inside) {
      // Just peeking (never pressed in): back to the clock after a few seconds
      if (idle >= PEEK_MS) screen = CLOCK_SCREEN;
    } else if (idle >= IDLE_MS) {
      // Pressed in but left empty at 00.00: back to the clock too
      bool emptyTimer = screen == TIMER_SCREEN && !timerRunning && timerRemainingMs == 0;
      bool emptySw    = screen == STOPWATCH_SCREEN && !swRunning && swElapsedMs == 0;
      if (emptyTimer || emptySw || screen == ALARM_SCREEN) {
        screen = CLOCK_SCREEN;
        inside = false;
      }
    }
  }
}

void setup() {
  pinMode(DATA_PIN, OUTPUT);
  pinMode(LATCH_PIN, OUTPUT);
  pinMode(CLOCK_PIN, OUTPUT);

  for (int i = 0; i < 5; i++) {
    pinMode(DIGIT_PINS[i], OUTPUT);
    digitalWrite(DIGIT_PINS[i], HIGH); // all digits off to start
  }

  pinMode(ENC_CLK_PIN, INPUT_PULLUP);
  pinMode(ENC_DT_PIN, INPUT_PULLUP);
  pinMode(ENC_SW_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  Serial.begin(9600);

  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC - check wiring (SDA/SCL, VCC, GND).");
    while (1) delay(10);
  }

  if (!rtc.isrunning()) {
    Serial.println("RTC lost power, setting time to compile time.");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  loadAlarm();

  SPI.begin();
  rfid.PCD_Init();
  byte v = rfid.PCD_ReadRegister(MFRC522::VersionReg);
  rfidFound = (v != 0x00 && v != 0xFF);             // no reader wired up: skip it
  if (rfidFound) {
    // Give up waiting for a card after 5 ms instead of 25 ms, so checking for
    // one barely pauses the display (the reader's timer ticks every 25 us).
    rfid.PCD_WriteRegister(MFRC522::TReloadRegH, 0);
    rfid.PCD_WriteRegister(MFRC522::TReloadRegL, 200);
  } else {
    Serial.println("No RFID reader found - check wiring (it needs 3.3V).");
  }
  showClock();
}

void loop() {
  unsigned long now = millis();

  handleInput();
  updateTimer();
  updateStopwatch();
  updateAlarm();
  updateCard();
  updateBuzzer();

  // Refresh what's on the display (timer/stopwatch and flashing need faster
  // updates than the clock). Also refresh right away whenever the screen changes.
  static Screen shownScreen = CLOCK_SCREEN;
  static bool shownInside = false;
  static bool shownStatus = false;
  bool status = statusShowing();
  unsigned long refreshMs = (screen == CLOCK_SCREEN && !alarmRinging) ? 250 : 50;
  if (screen != shownScreen || inside != shownInside || status != shownStatus ||
      now - lastDisplayUpdate >= refreshMs) {
    shownScreen = screen;
    shownInside = inside;
    shownStatus = status;
    lastDisplayUpdate = now;
    if (status) showStatusMessage();
    else if (screen == CLOCK_SCREEN) showClock();
    else if (screen == TIMER_SCREEN) showTimer();
    else if (screen == STOPWATCH_SCREEN) showStopwatch();
    else showAlarm();
  }

  // Multiplex: light one digit at a time, fast enough that it looks solid
  if (now - lastMuxStep >= 2) {
    lastMuxStep = now;
    digitalWrite(DIGIT_PINS[muxIndex], HIGH); // turn off the digit we just showed
    muxIndex = (muxIndex + 1) % 5;
    writeSegments(displayBuffer[muxIndex]);
    digitalWrite(DIGIT_PINS[muxIndex], LOW);  // turn on the next digit
  }
}
