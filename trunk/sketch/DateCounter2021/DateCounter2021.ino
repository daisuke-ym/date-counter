/*
 * $Id$
 * 
 * 名称:
 * あと何日カウンター 2021
 * 
 * 対応ボード:
 * Da Vinci 32U
 * 
 * 機能概要:
 * RTCを用いて日時管理をし，目標日までの日数をカウントダウンするタイマー
 * 
 * 機能詳細:
 * 1. RTCによる日時管理とLCDへの表示
 * 2. 日時の設定
 * 3. 目標日の設定
 * 4. 日付が変わる時，日数をカウントダウン
 * 5. 7セグLEDを点灯・消灯させる時間の設定
 * 6. 7セグLEDの明るさ制御
 */

#include <RX8025.h>
#include <LiquidCrystal.h>
#include <EEPROM.h>
#include <SPI.h>

// RTCに関する定義
RX8025 rtc; // RX8025  RTC
const uint8_t intAPin = 7; // intA 割り込みピン接続先
volatile boolean AlarmD = false; // アラーム検出か

uint8_t data[7]; // 時刻データやりとり用
char strDate[9], strTime[9]; // 液晶表示用バッファ

// LCDの接続先
const int rs = 22, en = 23, d4 = 13, d5 = 5, d6 = 6, d7 = 12;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// スイッチの接続先
const uint8_t SW_UP     = 21; // SW UP
const uint8_t SW_DOWN   = 20; // SW DOWN
const uint8_t SW_OK     = 19; // SW OK-NEXT
const uint8_t SW_CANCEL = 18; // SW CANCEL-BACK

// 7セグの接続先（SPI）
const uint8_t SDI   = 16; // MOSI
const uint8_t LATCH = 8;  // ラッチ
const uint8_t PWM   = A6;  // 明るさ調整VR用
const uint8_t OE    = 9;  // 明るさ調整PWM用
const uint8_t CLR   = 10; // クリア

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

// 7セグLED点灯・消灯時間の保存先アドレス（内蔵EEPROM）
const uint8_t ROM_UPHOUR   = 3; // 点灯時
const uint8_t ROM_UPMIN    = 4; // 点灯分
const uint8_t ROM_DOWNHOUR = 5; // 消灯時
const uint8_t ROM_DOWNMIN  = 6; // 消灯分

// 目標日までの日数
int count = 0;

// ----------------------------------------------------------------------
void setup() {
  Serial.begin(9600);

  pinMode(SW_UP, INPUT_PULLUP);
  pinMode(SW_DOWN, INPUT_PULLUP);
  pinMode(SW_OK, INPUT_PULLUP);
  pinMode(SW_CANCEL, INPUT_PULLUP);

  // 液晶の初期化
  lcd.begin(20, 4);
  lcd.clear();

  // 7セグの初期化
  pinMode(SCK, OUTPUT);
  pinMode(SDI, OUTPUT);
  pinMode(LATCH, OUTPUT);
  pinMode(OE, OUTPUT);
  //pinMode(CLR, OUTPUT);
  SPI.begin();
  SPI.setBitOrder(LSBFIRST);
  SPI.setClockDivider(SPI_CLOCK_DIV128);
  SPI.setDataMode(SPI_MODE0);
  
  // RTCの初期化
  rtc.begin();
  // AlarmDの設定（日付が変わる時に割り込み）
  uint8_t al[] = {0, 0};
  rtc.setAlarmD(al);
  // AlarmD は割り込みで処理
  attachInterrupt(digitalPinToInterrupt(intAPin), onAlarmD, FALLING);
  
  // 目標日の設定
  if (EEPROM.read(ROM_YEAR) >= 100) {
    count = 0;
  } else {
    count = CalcTarget(EEPROM.read(ROM_YEAR), EEPROM.read(ROM_MONTH), EEPROM.read(ROM_DAY));
  }
  // 7セグへ表示
  disp7seg(count);

  // 点灯・消灯時間の表示
  dispStaticInfo();
}

// ----------------------------------------------------------------------
void loop() {
  unsigned long now; // 長押し検出用
  long uptime, downtime, nowtime;

  // セッティングモードへの移行
  if (digitalRead(SW_UP) == LOW && digitalRead(SW_DOWN) == LOW) {
    delay(20);
    if (digitalRead(SW_UP) == LOW && digitalRead(SW_DOWN) == LOW) {
      now = millis();
      while (digitalRead(SW_UP) == LOW && digitalRead(SW_DOWN) == LOW) {
        if (millis() - now > 2000) {
          detachInterrupt(0);
          SettingMode();
          attachInterrupt(0, onAlarmD, FALLING);
        }
      }
    }    
  }
  
  // 時計表示（通常モード）
  rtc.getClock(data);
  sprintf(strDate, "%02d/%02d/%02d", data[6], data[5], data[4]);
  sprintf(strTime, "%02d:%02d:%02d", data[2], data[1], data[0]);
  lcd.setCursor(0, 0);
  lcd.print("NOW ");
  lcd.setCursor(4, 0);
  lcd.print(strDate);
  lcd.setCursor(4, 1);
  lcd.print(strTime);

  // AlarmD を止める
  if (AlarmD) {
    dispTarget(); // 7セグへ表示
    AlarmD = false;
    rtc.clearAlarmD();
    dispStaticInfo();
  }

  // 7セグLEDを指定時刻に点灯・消灯させる処理
  uptime = (long)EEPROM.read(ROM_UPHOUR) * 3600 + (long)EEPROM.read(ROM_UPMIN) * 60;
  downtime = (long)EEPROM.read(ROM_DOWNHOUR) * 3600 + (long)EEPROM.read(ROM_DOWNMIN) * 60;
  nowtime = (long)data[2] * 3600 + (long)data[1] * 60 + (long)data[0];
  Serial.print(uptime);
  Serial.print("\t");
  Serial.print(nowtime);
  Serial.print("\t");
  Serial.print(downtime);
  Serial.println();
  if (uptime < downtime) {
    if (nowtime >= uptime && nowtime <= downtime) {
      analogWrite(OE, map(analogRead(PWM), 0, 1023, 0, 255));
    } else {
      digitalWrite(OE, HIGH);
    }
  } else if (uptime > downtime) {
    //if (!(nowtime >= downtime && nowtime <= uptime)) {
    if (nowtime <= downtime || nowtime >= uptime) {
      analogWrite(OE, map(analogRead(PWM), 0, 1023, 0, 255));
    } else {
      digitalWrite(OE, HIGH);
    }
  } else {
    analogWrite(OE, map(analogRead(PWM), 0, 1023, 0, 255));
  }
  
  delay(100);
}

// ----------------------------------------------------------------------
void SettingMode() {
  static uint8_t mode = 1;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Setting  ");

  // SW_UP および SW_DOWN が OFF になるまで待つ処理
  if (digitalRead(SW_UP) == LOW || digitalRead(SW_DOWN) == LOW) {
    delay(20);
    if (digitalRead(SW_UP) == LOW || digitalRead(SW_DOWN) == LOW) {
      delay(100);
      while (digitalRead(SW_UP) == LOW && digitalRead(SW_DOWN) == LOW) {
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
        break;
      case 3:
        lcd.setCursor(0, 1);
        lcd.print("3.Up/Down ");
        break;
    }
    // 設定モード切替
    if (digitalRead(SW_UP) == LOW) {
      delay(20);
      if (digitalRead(SW_UP) == LOW) {
        mode++;
        if (mode > 3) {
          mode = 1;
        }
        while (digitalRead(SW_UP) == LOW) {
          // NOP
        }
      }
    }
    if (digitalRead(SW_DOWN) == LOW) {
      delay(20);
      if (digitalRead(SW_DOWN) == LOW) {
        mode--;
        if (mode < 1) {
          mode = 3;
        }
        while (digitalRead(SW_DOWN) == LOW) {
          // NOP
        }
      }
    }
    // 各設定モードへ移行
    if (digitalRead(SW_OK) == LOW) {
      delay(20);
      if (digitalRead(SW_OK) == LOW) {
        switch (mode) {
          case 1:
            SetClock();
            break;
          case 2:
            SetTarget();
            break;
          case 3:
            SetUpDownTime();
            break;
        }
      }
    }
    // 設定モードを抜ける
    if (digitalRead(SW_CANCEL) == LOW) {
      delay(20);
      if (digitalRead(SW_CANCEL) == LOW) {
        break;
      }
    }
  }
  lcd.clear();
  dispStaticInfo();
}

// ----------------------------------------------------------------------
void SetClock() {
  Serial.println(F("*** SetClock() ***"));
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
}

// ----------------------------------------------------------------------
void SetTarget() {
  Serial.println(F("*** SetTarget() ***"));
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
  Serial.print(F("   year: "));
  Serial.println(year);
  month = ChangeValue(today[5], 1, 12, 3, 1);
  Serial.print(F("   month: "));
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
  Serial.print(F("   day: "));
  Serial.println(day);
  EEPROM.write(ROM_YEAR, year);
  EEPROM.write(ROM_MONTH, month);
  EEPROM.write(ROM_DAY, day);
}

// ----------------------------------------------------------------------
void SetUpDownTime() {
  Serial.println(F("*** SetUpDownTime() ***"));
  uint8_t uphour, upmin, downhour, downmin;
  uint8_t today[7];

  rtc.getClock(today);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Up time  ");
  lcd.setCursor(0, 1);
  lcd.print("00:00");
  uphour = ChangeValue(EEPROM.read(ROM_UPHOUR), 0, 23, 0, 1);
  Serial.print(F("   uphour: "));
  Serial.println(uphour);
  upmin = ChangeValue(EEPROM.read(ROM_UPMIN), 0, 59, 3, 1);
  Serial.print(F("   upmin: "));
  Serial.println(upmin);
  //
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Down time  ");
  lcd.setCursor(0, 1);
  lcd.print("00:00");
  downhour = ChangeValue(EEPROM.read(ROM_DOWNHOUR), 0, 23, 0, 1);
  Serial.print(F("   downhour: "));
  Serial.println(downhour);
  downmin = ChangeValue(EEPROM.read(ROM_DOWNMIN), 0, 59, 3, 1);
  Serial.print(F("   downmin: "));
  Serial.println(downmin);

  // write target
  EEPROM.write(ROM_UPHOUR, uphour);
  EEPROM.write(ROM_UPMIN, upmin);
  EEPROM.write(ROM_DOWNHOUR, downhour);
  EEPROM.write(ROM_DOWNMIN, downmin);
}
// ----------------------------------------------------------------------
uint8_t ChangeValue(uint8_t arg, uint8_t min, uint8_t max, uint8_t col, uint8_t row) {
  unsigned long now;
  int val = arg;
  char ss[2];

  Serial.println(F("*** ChangeValue() ***"));
  Serial.print(F("   arg: "));
  Serial.println(val);
  if (val > max) val = 1;
  // SW_OK が OFF になるまで待つ処理
  if (digitalRead(SW_OK) == LOW) {
    delay(20);
    if (digitalRead(SW_OK) == LOW) {
      delay(100);
      while (digitalRead(SW_OK) == LOW) {
        // NOP
      }
    }
  }
  // 初期値を表示
  sprintf(ss, "%02d", val);
  lcd.setCursor(col, row);
  lcd.print(ss);
  
  lcd.setCursor(col + 1, row);
  lcd.blink();
  while (1) {
    // カウントアップ
    if (digitalRead(SW_UP) == LOW) {
      delay(20);
      if (digitalRead(SW_UP) == LOW) {
        Serial.print(F("   count up: "));
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
        while (digitalRead(SW_UP) == LOW) {
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
    if (digitalRead(SW_DOWN) == LOW) {
      delay(20);
      if (digitalRead(SW_DOWN) == LOW) {
        Serial.print(F("   count down: "));
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
        while (digitalRead(SW_DOWN) == LOW) {
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
    if (digitalRead(SW_OK) == LOW) {
      delay(20);
      if (digitalRead(SW_OK) == LOW) {
        Serial.println(F("   next"));
        delay(100);
        while (digitalRead(SW_OK) == LOW) {
          // NOP
        }
        break;
      }
    }
    /*
    // キャンセル
    if (digitalRead(SW_CANCEL) == LOW) {
      delay(20);
      if (digitalRead(SW_CANCEL) == LOW) {
        val = arg;
        Serial.println("   cancel");
        delay(100);
        while (digitalRead(SW_CANCEL) == LOW) {
          // NOP
        }
        break;
      }
    }
    */
  }
  lcd.noBlink();
  return val;
}

// ----------------------------------------------------------------------
int CalcTarget(uint8_t year, uint8_t month, uint8_t day) {
  Serial.println(F("*** CalcTarget() ***"));

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
  Serial.println();
  return (x2 - x1);
}

// ----------------------------------------------------------------------
void onAlarmD() {
  // 割り込み処理の中で Wire を使うとなぜか止まってしまうので，
  // ここでの 処理はフラグを変更するだけ
  Serial.println(F("*** onAlarmD() ***"));
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
  Serial.print(F("dig3: "));
  Serial.println(dig3);
  Serial.print(F("dig2: "));
  Serial.println(dig2);
  Serial.print(F("dig1: "));
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

// ----------------------------------------------------------------------
void dispStaticInfo() {
  char strTime[6], strDate[9];
  uint8_t year, month, day;

  // 目標日表示
  year = EEPROM.read(ROM_YEAR);
  month = EEPROM.read(ROM_MONTH);
  day = EEPROM.read(ROM_DAY);
  lcd.setCursor(0, 2);
  lcd.print("TGT ");
  lcd.setCursor(4, 2);
  sprintf(strDate, "%02d/%02d/%02d", year, month, day);
  lcd.print(strDate);
  lcd.setCursor(4, 3);
  lcd.print(CalcTarget(year, month, day));
  
  // 点灯・消灯時間の表示
  lcd.setCursor(15, 0);
  lcd.print("UP");
  lcd.setCursor(15, 1);
  sprintf(strTime, "%02d:%02d", EEPROM.read(ROM_UPHOUR), EEPROM.read(ROM_UPMIN));
  lcd.print(strTime);
  lcd.setCursor(15, 2);
  lcd.print("DOWN");
  lcd.setCursor(15, 3);
  sprintf(strTime, "%02d:%02d", EEPROM.read(ROM_DOWNHOUR), EEPROM.read(ROM_DOWNMIN));
  lcd.print(strTime);
}
