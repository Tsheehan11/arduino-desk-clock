/*
  Desk Clock — Arduino UNO + DS1307 RTC + 74HC595
  Drives a 4-digit 7-segment display (HH:MM) plus a 1-digit
  7-segment display used as an AM/PM indicator.

  Libraries needed (Library Manager):
    - RTClib (by Adafruit)

  === WIRING ===

  74HC595 (drives the shared segment bus, A-G,DP):
    Pin 16 (VCC)   -> 5V
    Pin 10 (MR)    -> 5V
    Pin 8  (GND)   -> GND
    Pin 13 (OE)    -> GND
    Pin 14 (DS)    -> Arduino D2
    Pin 12 (ST_CP / latch) -> Arduino D3
    Pin 11 (SH_CP / clock) -> Arduino D4
    Q0..Q7 (pins 15,1,2,3,4,5,6,7) -> segments A,B,C,D,E,F,G,DP
      Wire each Qx to BOTH displays' matching segment pin (in parallel):
        4-digit display: A=11 B=7 C=4 D=2 E=1 F=10 G=5 DP=3
        1-digit display:  A=7  B=6 C=4 D=2 E=1 F=9  G=10 DP=5

  Digit select (Arduino pin -> 220ohm resistor -> digit common pin).
  The Arduino pin goes LOW to light that digit, HIGH to turn it off.
    D5 -> 220ohm -> 4-digit display pin 12 (DIG1)
    D6 -> 220ohm -> 4-digit display pin 9  (DIG2)
    D7 -> 220ohm -> 4-digit display pin 8  (DIG3)
    D8 -> 220ohm -> 4-digit display pin 6  (DIG4)
    D9 -> 220ohm -> 1-digit display pin 3 or 8 (common)

  DS1307 RTC:
    VCC -> 5V, GND -> GND, SDA -> A4, SCL -> A5

  First upload only: uncomment the rtc.adjust(...) line below to set the
  time, upload once, then re-comment it and upload again.
*/

#include <Wire.h>
#include <RTClib.h>

// ---- 74HC595 pins ----
const int DATA_PIN  = 2; // DS
const int LATCH_PIN = 3; // ST_CP
const int CLOCK_PIN = 4; // SH_CP

// ---- Digit-select pins (active LOW) ----
// index 0-3 = the 4 time digits (left to right), index 4 = AM/PM digit
const int DIGIT_PINS[5] = {5, 6, 7, 8, 9};

RTC_DS1307 rtc;

// Standard 7-segment codes, bit order (MSB..LSB) = DP,G,F,E,D,C,B,A
const byte digitCode[10] = {
  0x3F, 0x06, 0x5B, 0x4F, 0x66,
  0x6D, 0x7D, 0x07, 0x7F, 0x6F
};
const byte SEG_BLANK = 0x00;
const byte SEG_A = 0x77; // "A" for AM
const byte SEG_P = 0x73; // "P" for PM

byte displayBuffer[5];

unsigned long lastClockUpdate = 0;
unsigned long lastMuxStep = 0;
int muxIndex = 0;

void writeSegments(byte pattern) {
  digitalWrite(LATCH_PIN, LOW);
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, pattern);
  digitalWrite(LATCH_PIN, HIGH);
}

void updateDisplayBuffer() {
  DateTime now = rtc.now();

  int hour24 = now.hour();
  bool isPM = (hour24 >= 12);
  int hour = hour24 % 12;
  if (hour == 0) hour = 12;
  int minute = now.minute();
  bool colon = (now.second() % 2 == 0);

  int h_tens = hour / 10;
  int h_ones = hour % 10;
  int m_tens = minute / 10;
  int m_ones = minute % 10;

  displayBuffer[0] = digitCode[h_tens];
  displayBuffer[1] = digitCode[h_ones] | (colon ? 0x80 : 0x00); // blinking dot between hours and minutes
  displayBuffer[2] = digitCode[m_tens];
  displayBuffer[3] = digitCode[m_ones];
  displayBuffer[4] = isPM ? SEG_P : SEG_A;
}

void setup() {
  pinMode(DATA_PIN, OUTPUT);
  pinMode(LATCH_PIN, OUTPUT);
  pinMode(CLOCK_PIN, OUTPUT);

  for (int i = 0; i < 5; i++) {
    pinMode(DIGIT_PINS[i], OUTPUT);
    digitalWrite(DIGIT_PINS[i], HIGH); // all digits off to start
  }

  Serial.begin(9600);

  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC - check wiring (SDA/SCL, VCC, GND).");
    while (1) delay(10);
  }

  if (!rtc.isrunning()) {
    Serial.println("RTC lost power, setting time to compile time.");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // --- To reset the time to your computer's clock: uncomment the ---
  // --- next line, upload once, then comment it out and upload again. ---
  // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  updateDisplayBuffer();
}

void loop() {
  unsigned long now = millis();

  // Refresh the clock values a few times a second (cheap, keeps colon blinking smoothly)
  if (now - lastClockUpdate >= 250) {
    lastClockUpdate = now;
    updateDisplayBuffer();
  }

  // Multiplex: light one digit at a time, fast enough that it looks solid
  if (now - lastMuxStep >= 2) {
    lastMuxStep = now;
    digitalWrite(DIGIT_PINS[muxIndex], HIGH); // turn off the digit we just showed
    muxIndex = (muxIndex + 1) % 5;
    writeSegments(displayBuffer[muxIndex]);
    digitalWrite(DIGIT_PINS[muxIndex], LOW); // turn on the next digit
  }
}
