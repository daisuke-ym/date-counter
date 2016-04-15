#include <Wire.h>
#include <ST7032.h>

ST7032 lcd;
#define RTC_ADRS  0x32
// SCL=A5 SDA=A4

char strDate[9], strTime[9];

void setup()
{
  lcd.begin(8, 2);
  lcd.setContrast(30);
  lcd.print("rtc");

  delay(2000);
  
//  pinMode(13, OUTPUT);
  Wire.begin();
  
  Wire.beginTransmission(RTC_ADRS);
  Wire.write(0xE0);
  Wire.write(0x20);  //24hourMode
  Wire.write(0x00);  // PON Clear
  Wire.endTransmission();
  delay(1);
  
  Wire.beginTransmission(RTC_ADRS);
  Wire.write(0x00);
  Wire.write(0x55);  //sec
  Wire.write(0x59);  //min
  Wire.write(0x23);  //hour
  Wire.write(0x00);  //week
  Wire.write(0x31);  //day
  Wire.write(0x12);  //month
  Wire.write(0x15);  //year
  Wire.endTransmission();
  delay(1);
}
 
void loop()
{
//  digitalWrite(13, HIGH);
 
  byte seconds,minutes,hours,days,months,years;
  Wire.requestFrom(RTC_ADRS, 8);
 
  Wire.read();
  seconds = Wire.read();
  minutes = Wire.read();
  hours = Wire.read();
  Wire.read();
  days = Wire.read();
  months = Wire.read();
  years = Wire.read();

  sprintf(strDate, "%02d/%02d/%02d", 
    (years >> 4) + (years & 0xf),
    (months >> 4) + (months & 0xf),
    (days >> 4) + (days & 0xf));
  sprintf(strTime, "%02d:%02d:%02d",
    (hours >> 4) + (hours & 0xf),
    (minutes >> 4) + (minutes & 0xf),
    (seconds >> 4) + (seconds & 0xf));  
  lcd.setCursor(0, 0);
  lcd.print(strDate);
  lcd.setCursor(0, 1);
  lcd.print(strTime);
   
//  digitalWrite(13, LOW);
  delay(100);
}

