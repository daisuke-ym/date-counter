/*
 * $Id$
 * 
 * 名称:
 * あと何日カウンター
 * 
 * 機能概要:
 * RTCを用いて日時管理をし，目標日までの日数をカウントダウンするタイマー
 * 
 * 機能詳細:
 * 1. RTCによる日時管理とLCDへの表示
 * 2. 日時の設定
 * 3. 目標日の設定
 * 4. 日付が変わる時，日数をカウントダウン
 */

#include <RX8025.h>
#include <ST7032.h>
#include <EEPROM.h>
#include <SPI.h>

ST7032 lcd; // AQM0802 LCD
RX8025 rtc; // RX8025  RTC

uint8_t data[7]; // 時刻データやりとり用
char strDate[9], strTime[9]; // 液晶表示用バッファ

volatile boolean AlarmD = false; // アラーム検出か

// スイッチの接続先
const uint8_t SW1 = 4; // SW UP
const uint8_t SW2 = 5; // SW DOWN
const uint8_t SW3 = 6; // SW OK-NEXT
const uint8_t SW4 = 7; // SW CANCEL-BACK

// 7セグの接続先（SPI）
const uint8_t LATCH = 10;
const uint8_t SDI = 11;
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
  0b00000010, // -
};

// 目標日の保存先アドレス（内蔵EEPROM）
const uint8_t ROM_DAY   = 0; // Target Day
const uint8_t ROM_MONTH = 1; // Target Month
const uint8_t ROM_YEAR  = 2; // Target Year

// 目標日までの日数
int count = 0;

// ----------------------------------------------------------------------
void setup() {
  Serial.begin(9600);

  pinMode(SW1, INPUT_PULLUP);
  pinMode(SW2, INPUT_PULLUP);
  pinMode(SW3, INPUT_PULLUP);
  pinMode(SW4, INPUT_PULLUP);

  // 液晶の初期化
  lcd.begin(8, 2);
  lcd.setContrast(30);
  lcd.clear();

  // 7セグの初期化
  pinMode(LATCH, OUTPUT);
  pinMode(SCK, OUTPUT);
  pinMode(SDI, OUTPUT);
  SPI.begin();
  SPI.setBitOrder(LSBFIRST);
  SPI.setDataMode(0);
  
  // RTCの初期化
  rtc.begin();
  // AlarmDの設定（日付が変わる時に割り込み）
  uint8_t al[] = {0, 0};
  rtc.setAlarmD(al);
  // AlarmD は割り込みで処理
  attachInterrupt(0, onAlarmD, FALLING);
  
  // 目標日の設定
  if (EEPROM.read(ROM_YEAR) >= 100) {
    count = 0;
  } else {
    count = CalcTarget(EEPROM.read(ROM_YEAR), EEPROM.read(ROM_MONTH), EEPROM.read(ROM_DAY));
  }
  // 7セグへ表示
  disp7seg(count);
}

// ----------------------------------------------------------------------
void loop() {
  unsigned long now; // 長押し検出用

  // セッティングモードへの移行
  if (digitalRead(SW1) == LOW && digitalRead(SW2) == LOW) {
    delay(20);
    if (digitalRead(SW1) == LOW && digitalRead(SW2) == LOW) {
      now = millis();
      while (digitalRead(SW1) == LOW && digitalRead(SW2) == LOW) {
        if (millis() - now > 2000) {
          detachInterrupt(0);
          SettingMode();
          attachInterrupt(0, onAlarmD, FALLING);
        }
      }
    }    
  }
  // ターゲット表示モードへの移行
  if (digitalRead(SW3) == LOW) {
    delay(20);
    if (digitalRead(SW3) == LOW) {
      detachInterrupt(0);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Target  ");
      lcd.setCursor(0, 1);
      if (EEPROM.read(ROM_YEAR) > 100) {
        lcd.print("Not set ");
      } else {
        sprintf(strDate, "%02d/%02d/%02d", 
          EEPROM.read(ROM_YEAR), 
          EEPROM.read(ROM_MONTH), 
          EEPROM.read(ROM_DAY));
        lcd.print(strDate);
      }
      while (digitalRead(SW3) == LOW) {
        // NOP
      }
      delay(1500); // ターゲット表示時間（秒）
      attachInterrupt(0, onAlarmD, FALLING);
    }
  }
  
  // 時計表示（通常モード）
  rtc.getClock(data);
  sprintf(strDate, "%02d/%02d/%02d", data[6], data[5], data[4]);
  sprintf(strTime, "%02d:%02d:%02d", data[2], data[1], data[0]);
  lcd.setCursor(0, 0);
  lcd.print(strDate);
  lcd.setCursor(0, 1);
  lcd.print(strTime);

  // AlarmD を止める
  if (AlarmD) {
    dispTarget(); // 7セグへ表示
    AlarmD = false;
    rtc.clearAlarmD();
  }

  delay(100);
}

// ----------------------------------------------------------------------
void SettingMode() {
  static uint8_t mode = 1;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Setting  ");

  // SW1 および SW2 が OFF になるまで待つ処理
  if (digitalRead(SW1) == LOW || digitalRead(SW2) == LOW) {
    delay(20);
    if (digitalRead(SW1) == LOW || digitalRead(SW2) == LOW) {
      delay(100);
      while (digitalRead(SW1) == LOW && digitalRead(SW2) == LOW) {
        // NOP
      }
    }
  }
  //
  while (1) {
    switch (mode) {
      case 1:
        lcd.setCursor(0, 1);
        lcd.print("1.Clock  ");
        break;
      case 2:
        lcd.setCursor(0, 1);
        lcd.print("2.Target ");
    }
    // 設定モード切替
    if (digitalRead(SW1) == LOW) {
      delay(20);
      if (digitalRead(SW1) == LOW) {
        if (mode == 1) {
          mode = 2;
        } else {
          mode = 1;
        }
        while (digitalRead(SW1) == LOW) {
          // NOP
        }
      }
    }
    if (digitalRead(SW2) == LOW) {
      delay(20);
      if (digitalRead(SW2) == LOW) {
        if (mode == 1) {
          mode = 2;
        } else {
          mode = 1;
        }
        while (digitalRead(SW2) == LOW) {
          // NOP
        }
      }
    }
    // 各設定モードへ移行
    if (digitalRead(SW3) == LOW) {
      delay(20);
      if (digitalRead(SW3) == LOW) {
        switch (mode) {
          case 1:
            SetClock();
            break;
          case 2:
            SetTarget();
            break;
        }
      }
    }
    // 設定モードを抜ける
    if (digitalRead(SW4) == LOW) {
      delay(20);
      if (digitalRead(SW4) == LOW) {
        break;
      }
    }
  }
}

// ----------------------------------------------------------------------
void SetClock() {
  Serial.println("*** SetClock() ***");
  uint8_t cal[6];

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Clock   ");
  sprintf(strDate, "%02d/%02d/%02d", 1, 1, 1);
  lcd.setCursor(0, 1);
  lcd.print(strDate);
  cal[5] = ChangeValue(1, 1, 99, 0, 1);
  cal[4] = ChangeValue(1, 1, 12, 3, 1);
  switch (cal[4]) {
    case 1:
    case 3:
    case 5:
    case 7:
    case 8:
    case 10:
    case 12:
      cal[3] = ChangeValue(1, 1, 31, 6, 1);
      break;
    case 2:
      if (cal[5] % 4 == 0) {
        cal[3] = ChangeValue(1, 1, 29, 6, 1);
      } else {
        cal[3] = ChangeValue(1, 1, 28, 6, 1);
      }
      break;
    case 4:
    case 6:
    case 9:
    case 11:
      cal[3] = ChangeValue(1, 1, 30, 6, 1);
      break;
  }
  // time
  sprintf(strTime, "%02d:%02d:%02d", 0, 0, 0);
  lcd.setCursor(0, 1);
  lcd.print(strTime);
  cal[2] = ChangeValue(0, 0, 23, 0, 1); // hour
  cal[1] = ChangeValue(0, 0, 59, 3, 1); // minute
  cal[0] = ChangeValue(0, 0, 59, 6, 1); // minute
  //
  rtc.setClock(cal);
  dispTarget();
}

// ----------------------------------------------------------------------
void SetTarget() {
  Serial.println("*** SetTarget() ***");
  uint8_t year, month, day;
  uint8_t today[7];

  rtc.getClock(today);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Target  ");
  sprintf(strDate, "%02d/%02d/%02d", today[6], today[5], today[4]);
  lcd.setCursor(0, 1);
  lcd.print(strDate);
  //
  year = ChangeValue(today[6], 1, 99, 0, 1);
  Serial.print("   year: ");
  Serial.println(year);
  month = ChangeValue(today[5], 1, 12, 3, 1);
  Serial.print("   month: ");
  Serial.println(month);
  switch (month) {
    case 1:
    case 3:
    case 5:
    case 7:
    case 8:
    case 10:
    case 12:
      day = ChangeValue(today[4], 1, 31, 6, 1);
      break;
    case 2:
      if (year % 4 == 0) {
        day = ChangeValue(today[4], 1, 29, 6, 1);
      } else {
        day = ChangeValue(today[4], 1, 28, 6, 1);
      }
      break;
    case 4:
    case 6:
    case 9:
    case 11:
      day = ChangeValue(1, 1, 30, 6, 1);
      break;
  }
  // write target
  Serial.print("   day: ");
  Serial.println(day);
  EEPROM.write(ROM_YEAR, year);
  EEPROM.write(ROM_MONTH, month);
  EEPROM.write(ROM_DAY, day);
  dispTarget();
}

// ----------------------------------------------------------------------
uint8_t ChangeValue(uint8_t arg, uint8_t min, uint8_t max, uint8_t col, uint8_t row) {
  unsigned long now;
  int val = arg;
  char ss[2];

  Serial.println("*** ChangeValue() ***");
  Serial.print("   arg: ");
  Serial.println(val);
  if (val > max) val = 1;
  // SW3 が OFF になるまで待つ処理
  if (digitalRead(SW3) == LOW) {
    delay(20);
    if (digitalRead(SW3) == LOW) {
      delay(100);
      while (digitalRead(SW3) == LOW) {
        // NOP
      }
    }
  }
  
  lcd.setCursor(col + 1, row);
  lcd.blink();
  while (1) {
    // カウントアップ
    if (digitalRead(SW1) == LOW) {
      delay(20);
      if (digitalRead(SW1) == LOW) {
        Serial.print("   count up: ");
        val++;
        if (val > max) {
          val = min;
        }
        sprintf(ss, "%02d", val);
        lcd.setCursor(col, row);
        lcd.print(ss);
        lcd.setCursor(col + 1, row);
        // 長押し処理
        now = millis();
        while (digitalRead(SW1) == LOW) {
          if ((millis() - now) > 500) {
            val++;
            if (val > max) {
              val = min;
            }
            sprintf(ss, "%02d", val);
            lcd.setCursor(col, row);
            lcd.print(ss);
            lcd.setCursor(col + 1, row);
            delay(100);
          }
        }
        Serial.println(val);
      }
    }
    // カウントダウン
    if (digitalRead(SW2) == LOW) {
      delay(20);
      if (digitalRead(SW2) == LOW) {
        Serial.print("   count down: ");
        val--;
        if (val < min) {
          val = max;
        }
        sprintf(ss, "%02d", val);
        lcd.setCursor(col, row);
        lcd.print(ss);
        lcd.setCursor(col + 1, row);
        // 長押し処理
        now = millis();
        while (digitalRead(SW2) == LOW) {
          if ((millis() - now) > 500) {
            val--;
            if (val < min) {
              val = max;
            }
            sprintf(ss, "%02d", val);
            lcd.setCursor(col, row);
            lcd.print(ss);
            lcd.setCursor(col + 1, row);
            delay(100);
          }
        }
        Serial.println(val);
      }
    }
    // 確定（次へ）
    if (digitalRead(SW3) == LOW) {
      delay(20);
      if (digitalRead(SW3) == LOW) {
        Serial.println("   next");
        delay(100);
        while (digitalRead(SW3) == LOW) {
          // NOP
        }
        break;
      }
    }
  }
  lcd.noBlink();
  return val;
}

// ----------------------------------------------------------------------
int CalcTarget(uint8_t year, uint8_t month, uint8_t day) {
  Serial.println("*** CalcTarget() ***");

  int a1, b1, c1, x1, a2, b2, c2, x2;

  rtc.getClock(data);
  a1 = data[6]; // year
  b1 = data[5]; // month
  c1 = data[4]; // day
  if (b1 < 3) {
    a1--;
    b1 += 13;
  } else {
    b1++;
  }
  x1 = int(365.25 * a1) + int(30.6 * b1) + int(a1 / 400) + c1 - int(a1 / 100) - 429;

  a2 = year;
  b2 = month;
  c2 = day;
  if (b2 < 3) {
    a2--;
    b2 += 13;
  } else {
    b2++;
  }
  x2 = int(365.25 * a2) + int(30.6 * b2) + int(a2 / 400) + c2 - int(a2 / 100) - 429;

  Serial.println(x2 - x1);
  Serial.println("");
  return (x2 - x1);
}

// ----------------------------------------------------------------------
void onAlarmD() {
  // 割り込み処理の中で Wire を使うとなぜか止まってしまうので，
  // ここでの 処理はフラグを変更するだけ
  Serial.println("*** onAlarmD() ***");
  AlarmD = true;
}

// ----------------------------------------------------------------------
void dispTarget() {
  uint8_t year, month, day;

  year = EEPROM.read(ROM_YEAR);
  month = EEPROM.read(ROM_MONTH);
  day = EEPROM.read(ROM_DAY);

  disp7seg(CalcTarget(year, month, day));
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

