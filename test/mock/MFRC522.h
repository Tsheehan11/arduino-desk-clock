// Fake RC522 reader: tests "tap" a card by calling tapCard().
#pragma once
#include <cstdint>
#include <cstring>
typedef uint8_t byte;
byte fake_card[10]; byte fake_card_size = 0;   // the card being held to the reader
bool fake_tap = false;                          // one new card waiting to be read
bool fake_reader_present = true;
int fake_polls = 0;                             // how often the sketch asked
void tapCard(const byte *uid, byte n){ memcpy(fake_card, uid, n); fake_card_size = n; fake_tap = true; }
class MFRC522 { public:
  static constexpr uint8_t UNUSED_PIN = 255;
  enum PCD_Register { TReloadRegH, TReloadRegL, VersionReg };
  struct Uid { byte size; byte uidByte[10]; byte sak; } uid;
  MFRC522(byte, byte){}
  void PCD_Init(){}
  byte PCD_ReadRegister(PCD_Register){ return fake_reader_present ? 0x92 : 0x00; }
  void PCD_WriteRegister(PCD_Register, byte){}
  bool PICC_IsNewCardPresent(){ fake_polls++; return fake_tap; }
  bool PICC_ReadCardSerial(){ if(!fake_tap) return false; fake_tap=false; uid.size=fake_card_size; memcpy(uid.uidByte,fake_card,fake_card_size); return true; }
  int PICC_HaltA(){ return 0; }
};
