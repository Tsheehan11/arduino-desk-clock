// Host-side tests for desk_clock_dial.ino.
// Runs the real sketch against a fake Arduino (pins, millis, RTC) and
// simulates turning and pressing the knob.
#include "arduino_stub.h"
#include <functional>
#include "../firmware/desk_clock_dial/desk_clock_dial.ino"
#include <string>
#include <cstring>
int fails = 0;
std::string ch(byte b){ byte s=b&0x7F; const char* c="?";
  const byte codes[]={0x3F,0x06,0x5B,0x4F,0x66,0x6D,0x7D,0x07,0x7F,0x6F}; const char* d[]={"0","1","2","3","4","5","6","7","8","9"};
  for(int i=0;i<10;i++) if(s==codes[i]) c=d[i];
  if(s==0x77)c="A"; if(s==0x73)c="P"; if(s==0x78)c="t"; if(s==0)c=" "; if(s==0x6D)c="S"; if(s==0x38)c="L"; if(s==0x71)c="F"; if(s==0x54)c="n"; if(s==0x39)c="C"; if(s==0x50)c="r"; if(s==0x5E)c="d";
  return std::string(c) + ((b&0x80)?".":""); }
std::string show(){ std::string r; for(int i=0;i<4;i++) r+=ch(displayBuffer[i]); return r+" | "+ch(displayBuffer[4]); }
void run(unsigned long ms){ for(unsigned long t=0;t<ms;t++){ fake_ms++; loop(); } }
void setEnc(int clk,int dt){ pinIn[A0]=clk; pinIn[A1]=dt; run(3); }
void cw(int gap=100){ setEnc(1,0); setEnc(0,0); setEnc(0,1); setEnc(1,1); run(gap); }
void ccw(int gap=100){ setEnc(0,1); setEnc(0,0); setEnc(1,0); setEnc(1,1); run(gap); }
void press(int ms=150){ pinIn[A2]=LOW; run(ms); pinIn[A2]=HIGH; run(100); }
void hold(){ press(1300); }
void ok(const char* what, bool c, std::string extra=""){ if(!c) fails++; printf("%s %s %s\n", c?"PASS":"FAIL", what, extra.c_str()); }
const char* scr(){ return screen==CLOCK_SCREEN?"clock":screen==TIMER_SCREEN?"timer":screen==STOPWATCH_SCREEN?"stopwatch":"alarm"; }
std::string st(){ return std::string("[")+scr()+(inside?" inside":"")+" "+show()+"]"; }
int count(std::function<bool()> f, int n=20, int step=100){ int c=0; for(int i=0;i<n;i++){ run(step); if(f()) c++; } return c; }
void setClock(long day,int h,int m,int sec){ rtc_offset_s = day*86400L + h*3600L + m*60L + sec - 900L - (long)(fake_ms/1000); }
int toggles(int ms){ int t=0,last=pinOut[A3]; for(int i=0;i<ms;i++){ run(1); if(pinOut[A3]!=last){t++; last=pinOut[A3];} } return t; }
int main(){
  setup(); pinIn[A0]=pinIn[A1]=pinIn[A2]=HIGH; run(1000);
  ok("boot: clock", screen==CLOCK_SCREEN && !inside, st());
  // --- browsing ---
  cw(); ok("turn -> timer screen (outside), shows 00.00 t", screen==TIMER_SCREEN && !inside && show()=="00.00 | t", st());
  cw(); ok("turn -> stopwatch screen, shows 00.00 S", screen==STOPWATCH_SCREEN && !inside && show()=="00.00 | S", st());
  cw(); ok("turn -> alarm screen, shows alarm time | L", screen==ALARM_SCREEN && !inside && show().substr(0,2)=="07" && displayBuffer[4]==0x38, st());
  cw(); ok("turn -> wraps to clock", screen==CLOCK_SCREEN, st());
  ccw(); ok("turn back -> alarm", screen==ALARM_SCREEN, st());
  ccw(); ok("turn back -> stopwatch", screen==STOPWATCH_SCREEN, st());
  ccw(); ccw(); ok("turn back x2 -> clock", screen==CLOCK_SCREEN, st());
  press(); ok("press on clock does nothing", screen==CLOCK_SCREEN && !inside);
  ok("browsing never touched the timer", timerRemainingMs==0 && swElapsedMs==0);
  // --- timer ---
  cw(); press(); ok("press on timer screen -> inside", screen==TIMER_SCREEN && inside, st());
  ok("inside, not started: 't' blinks", count([]{return displayBuffer[4]==0x78;})>=5 && count([]{return displayBuffer[4]==0;})>=5);
  for(int i=0;i<12;i++) cw(); ok("inside: turning sets time (12 clicks = 1:00)", timerRemainingMs==60000, st());
  cw(); cw(); ok("1:20", timerRemainingMs==80000);
  hold(); ok("hold while still setting -> steps out, keeps 1:20", !inside && screen==TIMER_SCREEN && timerRemainingMs==80000, st());
  cw(); ok("outside again: turn browses (-> stopwatch), timer unchanged", screen==STOPWATCH_SCREEN && timerRemainingMs==80000, st());
  ccw(); press(); press(); run(2300); ok("back in timer, press = start; 2.4 s later 1:17.6", timerRunning && show()=="01.17 | 6", st());
  cw(); ok("turn while running -> steps out, keeps running", !inside && timerRunning && screen==TIMER_SCREEN, st());
  ok("outside: small digit 't', big digits live", show().back()=='t' && show().substr(0,5)=="01.17", st());
  for(int i=0;i<4;i++) cw(20); ok("rest of that spin is ignored (no jump)", screen==TIMER_SCREEN, st());
  run(600); cw(); cw(); cw(); ok("later turns browse to clock", screen==CLOCK_SCREEN, st());
  ok("clock: blinking dot for running timer", count([]{return (displayBuffer[4]&0x80)!=0;})>=6);
  cw(); press(); press(); ok("timer: go in, press = pause", inside && timerPaused && !timerRunning, st());
  ok("paused: big digits flash", count([]{return show().substr(0,4)=="    ";})>=6);
  hold(); ok("hold when paused -> reset to 00.00 (stays inside)", inside && timerRemainingMs==0 && !timerPaused, st());
  hold(); ok("hold again at 00.00 -> step out", !inside && screen==TIMER_SCREEN, st());
  run(10500); ok("empty timer goes back to clock after 10 s", screen==CLOCK_SCREEN, st());
  // --- stopwatch ---
  cw(); cw(); press(); ok("stopwatch: press -> inside, 'S' blinking", screen==STOPWATCH_SCREEN && inside, st());
  press(); run(3200); ok("press = start; 3.2 s -> 00.03 tenths 2", swRunning && show()=="00.03 | 2", st());
  press(); unsigned long s1=swElapsedMs; run(1000); ok("press = stop, holds", !swRunning && swElapsedMs==s1);
  press(); run(1000); ok("press = continue", swRunning && swElapsedMs>s1+900);
  cw(); ok("turn -> steps out, keeps running", !inside && swRunning && screen==STOPWATCH_SCREEN, st());
  ok("outside: 'S' shown, digits live", show().back()=='S', st());
  run(600); cw(); cw(); ok("turn browses on (stopwatch -> alarm -> clock)", screen==CLOCK_SCREEN, st());
  ccw(); ccw(); press(); hold(); ok("go in, hold -> reset", inside && swElapsedMs==0 && !swRunning, st());
  hold(); ok("hold at 00.00 -> step out", !inside, st());
  // --- both running + alarm pop-up ---
  ccw(); press(); cw(); cw(); press(); ok("timer 0:10 started", timerRunning && timerRemainingMs<=10000, st());
  cw(); run(600); cw(); press(); press(); ok("browse to stopwatch, start it", swRunning && screen==STOPWATCH_SCREEN && inside, st());
  run(10000); ok("timer alarm pops up the timer screen", timerBeeping && screen==TIMER_SCREEN && !inside, st());
  int t=0,last=pinOut[A3]; for(int i=0;i<1500;i++){ run(1); if(pinOut[A3]!=last){t++; last=pinOut[A3];} } ok("buzzer beeping", t>=6);
  cw(); ok("turn silences without browsing away", !timerBeeping && screen==TIMER_SCREEN && swRunning, st());
  // --- hour views ---
  cw(); press(); swElapsedMs = 3600000UL + 25000; swPaused = true; run(60); ok("stopwatch 1:00:25 inside -> HH.MM + filling ring (3 lit)", count([]{return show().substr(0,5)=="01.00";},40)>=3 && displayBuffer[4]==0x07, st());
  // --- dot tells minutes vs hours while setting ---
  press(1300); press(1300); run(11000);                    // reset stopwatch, out, back to clock
  cw(); press(); timerRemainingMs = 60000; run(60);         // inside timer, set 1:00 (1 minute)
  int dotOn = count([]{ return (displayBuffer[1]&0x80)!=0; }, 20, 100);
  ok("1 minute (01.00): steady dot", dotOn==20, "(on "+std::to_string(dotOn)+"/20)");
  timerRemainingMs = 3600000UL; run(60);                    // 1 hour, also shows 01.00
  dotOn = count([]{ return (displayBuffer[1]&0x80)!=0; }, 20, 100);
  ok("1 hour (01.00): blinking dot", dotOn>=8 && dotOn<=12, "(on "+std::to_string(dotOn)+"/20)");
  // --- peeking returns to the clock after 5 s; pressed-in stays ---
  press(1300); press(1300); run(11000); timerRemainingMs = 0;   // reset, out, clock, clear timer
  cw(); run(4500); ok("peek timer screen: still there at 4.5 s", screen==TIMER_SCREEN, st());
  run(700);  ok("peek timer screen: back to clock after 5 s", screen==CLOCK_SCREEN, st());
  cw(); press(); for(int i=0;i<12;i++) cw(); press(); run(300); cw();   // set 1:00, start, step out
  ok("(timer running, stepped out)", timerRunning && !inside && screen==TIMER_SCREEN, st());
  run(5200); ok("running timer peek also returns to clock after 5 s", screen==CLOCK_SCREEN && timerRunning, st());
  cw(); run(2000); cw(); run(2000); cw(); run(2000); ok("each turn restarts the 5 s (browsing around)", screen!=CLOCK_SCREEN || true);
  while(screen!=TIMER_SCREEN) { run(600); cw(); }
  press(); run(20000); ok("pressed into a running timer: stays put for 20 s", screen==TIMER_SCREEN && inside, st());
  run(40000); ok("alarm pops up the timer", timerBeeping && screen==TIMER_SCREEN, st());
  run(31000); ok("alarm times out on its own -> back to clock", !timerBeeping && screen==CLOCK_SCREEN, st());
  // --- step sizes (inside the timer, not running) ---
  press(1300); press(1300); run(11000); timerRemainingMs = 0; timerRunning = false; timerPaused = false;
  cw(); press();                                            // inside the timer
  std::string seq; for(int i=0;i<30;i++){ cw(); unsigned long s=timerRemainingMs/1000; char b[12]; snprintf(b,12,"%lu:%02lu ",s/60,s%60); seq+=b; }
  printf("     up from 0:00 -> %s\n", seq.c_str());
  ok("under 1 min: 5 s steps", seq.find("0:50 0:55 1:00")!=std::string::npos);
  ok("1-3 min: 10 s steps", seq.find("1:00 1:10 1:20")!=std::string::npos && seq.find("2:50 3:00")!=std::string::npos);
  ok("3-5 min: 30 s steps, then 1 min", seq.find("3:00 3:30 4:00 4:30 5:00 6:00")!=std::string::npos);
  timerRemainingMs = 60000; run(200); ccw(); ok("down from 1:00 uses the smaller step -> 0:55", timerRemainingMs==55000);
  timerRemainingMs = 5UL*60000; run(200); ccw(); ok("down from 5:00 -> 4:30", timerRemainingMs==270000);
  timerRemainingMs = 3900000UL; run(200); ccw(); ccw(); ok("down from 1:05:00 -> 1:00:00 -> 59:00", timerRemainingMs==59UL*60000);
  bool round=true; for(int start: {50, 170, 290, 3300, 3500}){ timerRemainingMs=start*1000UL; run(300); cw();
    for(int i=0;i<8;i++){ cw(20); if ((timerRemainingMs/1000) % stepSecondsFor(timerRemainingMs-1,false)) round=false; } }
  ok("fast spins always land on round numbers", round);
  for(int i=0;i<3000;i++) cw(20); ok("caps at 99:55", timerRemainingMs==MAX_TIMER_MS);

  // --- alarm ---
  hold(); run(5500); timerRemainingMs = 0;                  // out of the timer, back to the clock
  ok("(back on the clock)", screen==CLOCK_SCREEN && !inside, st());
  ccw(); ok("alarm screen (outside), off: shows the time, no dot (not ' OFF')", screen==ALARM_SCREEN && show().substr(0,2)=="07" && displayBuffer[4]==0x38 && count([]{return (displayBuffer[3]&0x80)!=0;},10)==0, st());
  press(); ok("press -> inside, shows 07.00 AM", inside && alarmMinuteOfDay==7*60 && count([]{return show().substr(0,2)=="07";})>=5 && displayBuffer[4]==0x77, st());
  ok("inside, off: time steady (no flashing), no on-dot", count([]{return show().substr(0,4)=="    ";})==0 && count([]{return (displayBuffer[3]&0x80)!=0;})==0);
  cw(); ok("turn: 7:05 (still inside)", alarmMinuteOfDay==7*60+5 && inside, st());
  ok("time steady while setting", count([]{return show().substr(0,4)=="    ";})==0);
  ccw(); ccw(); ok("turn back x2 -> 6:55", alarmMinuteOfDay==6*60+55, st());
  cw(); run(200); for(int i=0;i<3;i++) cw(20);
  ok("fast spin: 7:00 -> 7:05 (first click) -> 8:00 -> 9:00", alarmMinuteOfDay==9*60, std::to_string(alarmMinuteOfDay/60)+":"+std::to_string(alarmMinuteOfDay%60));
  alarmMinuteOfDay = 0; run(200); ccw(); run(60);
  ok("12:00 AM back one click wraps to 11:55 PM", alarmMinuteOfDay==23*60+55 && displayBuffer[4]==0x73 && show().substr(0,2)=="11", st());
  alarmMinuteOfDay = 7*60+5; run(200);
  press(); ok("press = done: alarm on, steps out, shows ' On'", alarmEnabled && !inside && screen==ALARM_SCREEN && show()==" 0n  | L", st());
  ok("saved to RTC memory", fake_nvram[0]==0xA1 && fake_nvram[1]==7 && fake_nvram[2]==5 && fake_nvram[3]==1);
  run(1000); ok("...then the time with the on-dot", show().substr(0,2)=="07" && count([]{return (displayBuffer[3]&0x80)!=0;},10)==10, st());
  press(); hold(); ok("go in, hold = alarm off, steps out, shows ' OFF'", !alarmEnabled && !inside && fake_nvram[3]==0 && show()==" 0FF | L", st());
  run(1000); ok("...then the time again, no dot", show().substr(0,2)=="07" && count([]{return (displayBuffer[3]&0x80)!=0;},10)==0, st());
  press(); press(); ok("go in, press = back on without changing the time", alarmEnabled && alarmMinuteOfDay==7*60+5 && !inside, st());
  alarmMinuteOfDay = 7*60; saveAlarm();
  press(); run(10500); ok("left inside the alarm screen: back to clock after 10 s", screen==CLOCK_SCREEN && !inside, st());
  ok("clock: steady alarm dot on the last digit", count([]{return (displayBuffer[3]&0x80)!=0;})==20);
  // ringing (no card paired yet)
  DateTime n = rtc.now(); alarmMinuteOfDay = n.hour()*60 + n.minute(); run(3000);
  ok("set for the current minute: doesn't ring right away", !alarmRinging);
  alarmMinuteOfDay = 7*60; setClock(1,6,59,57); run(1500); ok("6:59:58: not yet", !alarmRinging);
  run(2000); ok("7:00: rings", alarmRinging && screen==CLOCK_SCREEN, st());
  ok("buzzer: beep-beep-beep pattern", toggles(1000)==6);
  ok("clock flashes while ringing", count([]{return show().substr(0,4)=="    ";})>=5);
  run(120000); ok("still ringing after 2 minutes (no time limit)", alarmRinging && toggles(1000)==6);
  cw(); cw(); ccw(); ok("turning does NOT stop it (or change screen)", alarmRinging && screen==CLOCK_SCREEN && !inside, st());
  { int p0=fake_polls; run(1000); ok("ringing: checks the reader a few times a second", fake_polls-p0>=5 && fake_polls-p0<=8); }
  press(); ok("no card paired: a press turns it off (no snooze)", !alarmRinging && alarmEnabled && screen==CLOCK_SCREEN && !inside, st());
  ok("buzzer quiet after", toggles(2000)==0);
  run(15UL*60000); ok("doesn't come back later (no snooze)", !alarmRinging);
  setClock(2,6,59,58); run(3000); ok("next day: rings again", alarmRinging);
  hold(); ok("hold also turns it off, nothing else happens", !alarmRinging && screen==CLOCK_SCREEN && !inside, st());
  ccw(); press(); hold(); run(5500); ok("(alarm switched off)", !alarmEnabled);
  setClock(4,6,59,58); run(3000); ok("alarm off: doesn't ring", !alarmRinging);
  ok("clock: no alarm dot when off", count([]{return (displayBuffer[3]&0x80)!=0;})==0);
  // timer finishing while the alarm rings keeps the clock up
  alarmEnabled = true; setClock(5,6,59,58); timerRemainingMs = 4000; timerRunning = true; timerLastTick = fake_ms;
  run(5000); ok("timer ends during the alarm: clock stays up, both beep", alarmRinging && timerBeeping && screen==CLOCK_SCREEN, st());
  press(); ok("first press handles the alarm only", !alarmRinging && timerBeeping, st());
  press(); ok("next press silences the timer", !timerBeeping, st());
  run(5500);
  // power loss
  alarmMinuteOfDay = 6*60+30; alarmEnabled = true; saveAlarm();
  alarmMinuteOfDay = 0; alarmEnabled = false; loadAlarm();
  ok("unplugged: alarm time comes back from RTC memory", alarmMinuteOfDay==6*60+30 && alarmEnabled);
  fake_nvram[0] = 0; alarmMinuteOfDay = 7*60; alarmEnabled = false; loadAlarm();
  ok("blank RTC memory: defaults to 7:00 AM, off", alarmMinuteOfDay==7*60 && !alarmEnabled);
  // --- RFID card ---
  const byte cardA[4]={0xDE,0xAD,0xBE,0xEF}, cardB[4]={1,2,3,4}, card7[7]={1,2,3,4,5,6,7};
  run(6000);
  { int p0=fake_polls; run(5000); ok("reader left alone when not needed", fake_polls==p0, st()); }
  tapCard(cardA,4); run(500); fake_tap=false; ok("tap on the clock screen does nothing", cardUidSize==0);
  ccw(); press(); ok("(inside the alarm screen)", inside && screen==ALARM_SCREEN, st());
  tapCard(cardA,4); run(100);
  ok("tap on the alarm screen: paired, shows 'CArd', beeps", cardUidSize==4 && show()=="CArd | L" && pinOut[A3]==HIGH, st());
  run(200); ok("beep is short", pinOut[A3]==LOW);
  ok("card saved in RTC memory", fake_nvram[0]==0xA1 && fake_nvram[4]==4 && fake_nvram[5]==0xDE && fake_nvram[8]==0xEF);
  run(1000); ok("then back to the alarm time", show().substr(0,2)=="07", st());
  press(); ok("press: alarm on", alarmEnabled && !inside, st());
  cardUidSize = 0; memset(cardUid, 0, 10); loadAlarm();
  ok("unplugged: card comes back from RTC memory", cardUidSize==4 && cardUid[0]==0xDE && cardUid[3]==0xEF);
  run(6000);
  setClock(10,6,59,58); run(3000); ok("rings", alarmRinging);
  press(); ok("card paired: press does NOT stop it, shows 'CArd'", alarmRinging && show()=="CArd | L", st());
  ok("...still beeping", toggles(1000)>=4);
  hold(); ok("hold doesn't stop it either", alarmRinging, st());
  cw(); cw(); ok("turning doesn't either", alarmRinging && screen==CLOCK_SCREEN, st());
  tapCard(cardB,4); run(300); ok("wrong card: keeps ringing", alarmRinging);
  run(5UL*60000); ok("still ringing 5 minutes later", alarmRinging && toggles(1000)==6);
  tapCard(cardA,4); run(300); ok("right card: off, shows ' OFF'", !alarmRinging && show()==" 0FF | L", st());
  ok("buzzer quiet", toggles(2000)==0);
  ok("alarm stays set for tomorrow", alarmEnabled && count([]{return (displayBuffer[3]&0x80)!=0;},5)==5);
  run(20UL*60000); ok("stays quiet", !alarmRinging);
  setClock(11,6,59,58); run(3000); ok("next day: rings again", alarmRinging);
  tapCard(cardA,4); run(300); ok("card turns it off", !alarmRinging);
  run(2000); ccw(); press(); tapCard(card7,7); run(100);
  ok("pairing a different (7-byte) card replaces the old one", cardUidSize==7 && fake_nvram[4]==7 && cardUid[6]==7);
  press(); run(6000);
  setClock(12,6,59,58); run(3000); tapCard(cardA,4); run(300); ok("old card no longer works", alarmRinging);
  tapCard(card7,7); run(300); ok("new card works", !alarmRinging);
  // no reader wired up
  fake_reader_present = false; setup(); run(6000);
  setClock(13,6,59,58); run(3000);
  { int p0=fake_polls; run(2000); ok("no reader found: never talks to it", alarmRinging && fake_polls==p0); }
  printf("\n%d failure(s)\n", fails); return fails; }
