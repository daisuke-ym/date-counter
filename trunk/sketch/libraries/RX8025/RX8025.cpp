/*
	RX8025.cpp - Arduino RX-8025 RTC library
	
	$Id$
*/

#include "RX8025.h"
#include "Arduino.h"
#include <Wire.h>
#include <inttypes.h>

#define BCD2BIN(x)   (x - 6 * (x >> 4))
#define BIN2BCD(x)   (x + 6 * (x / 10))

// ----------------------------------------------------------------------
void RX8025::begin(void) {
	Wire.begin();
	
	// 検出ビットによる日時データの信頼性推定
	// 読み出しアドレスを指定しないと reg-F から読まれる
	Wire.requestFrom(RX8025_ADDR, 1);
	byte FLAGS = Wire.read();
	delay(1);
	if (FLAGS & 0x10) {
		// PON = 1: 全データの初期化が必要
		init();
	} else if ((FLAGS & 0x10) == 0 && (FLAGS & 0x20) == 0) {
		// PON = 0, XST = 0: 約10ms以上の水晶発振停止が発生（全データの初期化が必要）
		init();
	} else {
		Wire.beginTransmission(RX8025_ADDR);
		Wire.write(RX8025_REG_CONTROL2);
		Wire.write(0x20);
		Wire.endTransmission();
		delay(1);
	}	
}

// ----------------------------------------------------------------------
void RX8025::getClock(uint8_t data[7]) {
	int buf[7];
	
	Wire.requestFrom(RX8025_ADDR, 8);
	Wire.read(); // 空読み（読み出しアドレスを指定しないと reg-F から読まれる）
	for (int i = 0; i < 7; i++) {
		buf[i] = Wire.read();
	}
	for (int i = 0; i < 7; i++) {
		data[i] = BCD2BIN(buf[i]);
	}	
}

// ----------------------------------------------------------------------
void RX8025::setClock(uint8_t clock[6]) {
	uint8_t weekday;
	uint8_t a, b, c, x;
	
	// 曜日の計算（ツェラーの公式）
	a = clock[5]; // year
	b = clock[4]; // month
	c = clock[3]; // day
	if (b < 3) {
		a--;
		b += 13;
	} else {
		b++;
	}
	x = int(365.25 * a) + int(30.6 * b) + int(a / 400) + c - int(a / 100) - 429;
	weekday = x - int(x / 7) * 7 + 1;
	
	Wire.beginTransmission(RX8025_ADDR);
	Wire.write(RX8025_REG_SECONDS);
	Wire.write(BIN2BCD(clock[0])); // seconds
	Wire.write(BIN2BCD(clock[1])); // minutes
	Wire.write(BIN2BCD(clock[2])); // hours
	Wire.write(BIN2BCD(weekday));
	Wire.write(BIN2BCD(clock[3])); // days
	Wire.write(BIN2BCD(clock[4])); // months
	Wire.write(BIN2BCD(clock[5])); // years
	Wire.endTransmission();
	delay(1);
}

// ----------------------------------------------------------------------
void RX8025::setAlarmD(uint8_t data[2]) {
	Wire.beginTransmission(RX8025_ADDR);
	Wire.write(RX8025_REG_ALARMD_MINUTE); // reg-B
	Wire.write(BIN2BCD(data[0])); // minute
	Wire.write(BIN2BCD(data[1])); // hour
	Wire.endTransmission();
	delay(1);
	enableAlarmD();
	clearAlarmD();
}

// ----------------------------------------------------------------------
void RX8025::enableAlarmD(void) {
	int FLAG;
	
	Wire.beginTransmission(RX8025_ADDR);
	Wire.write(RX8025_REG_CONTROL1); // reg-E
	Wire.endTransmission(false);
	Wire.requestFrom(RX8025_ADDR, 1);
	FLAG = Wire.read();
	delay(1);
	Wire.beginTransmission(RX8025_ADDR);
	Wire.write(RX8025_REG_CONTROL1);
	Wire.write(FLAG | 0x40);
	Wire.endTransmission();
	delay(1);
}

// ----------------------------------------------------------------------
void RX8025::disableAlarmD(void) {
	int FLAG;
	
	Wire.beginTransmission(RX8025_ADDR);
	Wire.write(RX8025_REG_CONTROL1); // reg-E
	Wire.endTransmission(false);
	Wire.requestFrom(RX8025_ADDR, 1);
	FLAG = Wire.read();
	Wire.beginTransmission(RX8025_ADDR);
	Wire.write(RX8025_REG_CONTROL1);
	Wire.write(FLAG & 0xBF);
	Wire.endTransmission();
	delay(1);
}

// ----------------------------------------------------------------------
void RX8025::clearAlarmD(void) {
	int FLAG;
	
	Wire.beginTransmission(RX8025_ADDR);
	Wire.write(RX8025_REG_CONTROL2); // reg-F
	Wire.endTransmission(false);
	Wire.requestFrom(RX8025_ADDR, 1);
	FLAG = Wire.read();
	Wire.beginTransmission(RX8025_ADDR);
	Wire.write(RX8025_REG_CONTROL2);
	Wire.write(FLAG & 0xFE);
	Wire.endTransmission();
	delay(1);
}

// ----------------------------------------------------------------------
void RX8025::dumpReg(void) {
	Wire.beginTransmission(RX8025_ADDR);
	Wire.write(RX8025_REG_SECONDS);
	Wire.endTransmission(false);
	Wire.requestFrom(RX8025_ADDR, 16);
	Serial.println("*** dump register ***");
	for (int i = 0; i < 16; i++) {
		Serial.print(i);
		Serial.print(": ");
		Serial.println(Wire.read(), BIN);
	}
	Serial.println("");
	delay(1);
}

// ----------------------------------------------------------------------
// ----------------------------------------------------------------------
// ----------------------------------------------------------------------
// private methods
void RX8025::init() {
	// 全データの初期化
	Wire.beginTransmission(RX8025_ADDR);
	Wire.write(0xE0); // reg-E
	Wire.write(0x20); // 12,12 = 1: 24H モード
	Wire.write(0x20); // XST = 1:   発振停止検出有効
	Wire.endTransmission();
	// 2014/12/31 Mon 13:30:55 にセット
	uint8_t def[] = {55, 30, 13, 31, 12, 14};
	setClock(&def[0]);
	delay(1);
}

