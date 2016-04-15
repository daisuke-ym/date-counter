/*
 * $Id$
 * 
 * あと何日タイマー
 */

#include<SPI.h>
//int SCK = 13; // 標準の SCK ピンを使うので定義しなくて良い
int LATCH = 10;
int SDI = 11;

// 7セグのフォントデータ
const byte digits[] = {
  0b11111100, // 0
  0b01100000, // 1
  0b11011010, // 2
  0b11110010, // 3
  0b01100110, // 4
  0b10110110, // 5
  0b10111110, // 6
  0b11100000, // 7
  0b11111110, // 8
  0b11110110, // 9
};

// 操作スイッチ
const int SW1 = 4; // UP
const int SW2 = 5; // DOWN
const int SW3 = 6; // OK
const int SW4 = 7; // CANCEL

// カウンタの値（7セグの表示値）
int count = 25;

// ----------------------------------------------------------------------
void setup() {
  Serial.begin(9600);
  
  pinMode(LATCH, OUTPUT);
  pinMode(SCK, OUTPUT);
  pinMode(SDI, OUTPUT);
  SPI.begin();
  SPI.setBitOrder(LSBFIRST);
  SPI.setDataMode(0);

  pinMode(SW1, INPUT_PULLUP);
  pinMode(SW2, INPUT_PULLUP);
  pinMode(SW3, INPUT_PULLUP);
  pinMode(SW4, INPUT_PULLUP);

  disp7seg(count);
}

// ----------------------------------------------------------------------
void loop() {
  unsigned long now;
  
  if (digitalRead(SW1) == LOW) {
    delay(20);
    if (digitalRead(SW1) == LOW) {
      count++;
      if (count >= 999) {
        count = 999;
      }
      disp7seg(count);
      now = millis();
      while (digitalRead(SW1) == LOW) {
        // SW が離されるまで待つループ & 長押し処理
        if (millis() - now >= 1000) {
          count++;
          if (count >= 999) {
            count = 999;
          }
          disp7seg(count);
          delay(100);
        }
      }
    }
  }
  if (digitalRead(SW2) == LOW) {
    delay(20);
    if (digitalRead(SW2) == LOW) {
      count--;
      if (count <= 0) {
        count = 0;
      }
      disp7seg(count);
      now = millis();
      while (digitalRead(SW2) == LOW) {
        // SW が離されるまで待つループ & 長押し処理
        if (millis() - now >= 1000) {
          count--;
          if (count <= 0) {
            count = 0;
          }
          disp7seg(count);
          delay(100);
        }
      }
    }
  }
}

// ----------------------------------------------------------------------
void disp7seg(int value) {
  uint8_t dig3, dig2, dig1;

  dig3 = int(value / 100);
  dig2 = int((value - dig3 * 100) / 10);
  dig1 = value - dig3 * 100 - dig2 * 10;
  Serial.print("dig3: ");
  Serial.println(dig3);
  Serial.print("dig2: ");
  Serial.println(dig2);
  Serial.print("dig1: ");
  Serial.println(dig1);
  
  digitalWrite(LATCH, LOW);
  if (dig3 == 0) {
    SPI.transfer(0x0);
  } else {
    SPI.transfer(digits[dig3]);
  }
  if (dig3 == 0 && dig2 == 0) {
    SPI.transfer(0x0);
  } else {
    SPI.transfer(digits[dig2]);
  }
  SPI.transfer(digits[dig1]);
  digitalWrite(LATCH, HIGH);
}

