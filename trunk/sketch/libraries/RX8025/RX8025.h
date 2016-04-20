/*
  $Id$
*/

#ifndef __RX8025_H__
#define __RX8025_H__

#include <inttypes.h>

#define RX8025_ADDR                0x32

// regsters
#define RX8025_REG_SECONDS         0x00
#define RX8025_REG_MINUTES         0x10
#define RX8025_REG_HOURS           0x20
#define RX8025_REG_WEEKDAYS        0x30
#define RX8025_REG_DAYS            0x40
#define RX8025_REG_MONTHS          0x50
#define RX8025_REG_YEARS           0x60
#define RX8025_REG_DIGITAL_OFFSET  0x70
#define RX8025_REG_ALARMW_MINUTE   0x80
#define RX8025_REG_ALARMW_HOUR     0x90
#define RX8025_REG_ALARMW_WEEKDAY  0xA0
#define RX8025_REG_ALARMD_MINUTE   0xB0
#define RX8025_REG_ALARMD_HOUR     0xC0
#define RX8025_REG_RESERVED        0xD0
#define RX8025_REG_CONTROL1        0xE0
#define RX8025_REG_CONTROL2        0xF0

// ----------------------------------------------------------------------
class RX8025 {
public:
	void begin(void);

	// data[7]: 0: sec,  1: min,  2: hour,  3: weekday,  4: day,  5: month,  6: year
	void getClock(uint8_t data[7]);
	
	// yyyy/mm/dd hh:mm:ss
	char *getClockStr();
	
	// data[6]: 0: sec,  1: min,  2: hour,  3: day,  4: month,  5: year
	void setClock(uint8_t data[6]);
	
	// data[3]: 0: min,  1: hour,  2: weekday
	void setAlarmW(uint8_t data[3]);
	
	// data[2]: 0: min,  1: hour
	void setAlarmD(uint8_t data[2]);
	
	void enableAlarmW(void);
	void enableAlarmD(void);
	void disableAlarmW(void);
	void disableAlarmD(void);
	void clearAlarmW(void);
	void clearAlarmD(void);
	
	void dumpReg(void);
	
private:
	void init(void);

};

#endif  // __RX8025_H__
