// Minimal stand-ins for the Arduino functions the sketch uses.
#pragma once
#include <cstdint>
#include <cstdio>
typedef uint8_t byte;
#define HIGH 1
#define LOW 0
#define INPUT_PULLUP 2
#define OUTPUT 1
#define MSBFIRST 1
#define A0 14
#define A1 15
#define A2 16
#define F(x) x
unsigned long fake_ms = 0;
int pinIn[20]; int pinOut[20];
unsigned long millis(){ return fake_ms; }
int digitalRead(int p){ return pinIn[p]; }
void digitalWrite(int p, int v){ pinOut[p]=v; }
void pinMode(int p, int m){ if (m==INPUT_PULLUP) pinIn[p]=HIGH; }
void shiftOut(int,int,int,byte){}
void delay(unsigned long d){ fake_ms += d; }
struct { void begin(int){} void println(const char*){} void print(const char*){} } Serial;
