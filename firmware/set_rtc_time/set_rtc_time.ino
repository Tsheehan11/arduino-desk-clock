/*
  Set RTC Time — upload this ONCE to set the DS1307 to your computer's clock.
  Then upload desk_clock.ino again.

  The display stays blank while this sketch is running; that's expected.
*/

#include <Wire.h>
#include <RTClib.h>

RTC_DS1307 rtc;

// The time is captured when the sketch compiles, so the clock starts behind by
// however long compiling + uploading takes. This adds that time back.
// If the clock ends up AHEAD of real time, lower this number.
const int SECONDS_TO_ADD = 60;

void setup() {
  Serial.begin(9600);

  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC - check SDA/SCL and power.");
    while (1) delay(10);
  }

  // Sets the RTC to the date/time this sketch was compiled on your computer.
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)) + TimeSpan(SECONDS_TO_ADD));

  DateTime now = rtc.now();
  Serial.print("RTC set to: ");
  Serial.print(now.hour());
  Serial.print(':');
  if (now.minute() < 10) Serial.print('0');
  Serial.println(now.minute());
  Serial.println("Done. Now upload desk_clock.ino again.");
}

void loop() {}
