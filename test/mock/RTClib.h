#pragma once
#include <cstdint>
extern unsigned long fake_ms;
class DateTime { public: uint32_t s; DateTime(uint32_t x=0):s(x){} DateTime(const char*, const char*):s(0){}
  int hour() const { return (s/3600)%24; } int minute() const { return (s/60)%60; } int second() const { return s%60; } };
class RTC_DS1307 { public: bool begin(){return true;} bool isrunning(){return true;} void adjust(const DateTime&){}
  DateTime now(){ return DateTime(15*60 + fake_ms/1000); } };  // starts at 00:15:00
