//#include "functions.h"
//#include <TimeLib.h>
//#include <Wire.h>

#define DS3231_ADDRESS 0x68 // decimal 104
#define TEMPERATURE_SENSOR_DS3231  0	// RTC Temperature sensor
#define MAX_TEMPERATURE_EXCEED 111.11   // Celsius, bad sensor if exceed

int rtc_second;			//00-59;
int rtc_minute;			//00-59;
int rtc_hour;			//1-12 - 00-23;
int rtc_weekday;		//1-7
int rtc_day;			//01-31
int rtc_month;			//01-12
int rtc_year;			//0-99 + 2000;
bool RTCworking = false;
tmElements_t tm;



//int BCD2DEC(int x) { return ((x)>>4)*10+((x)&0xf); }
//int DEC2BCD(int x) { return (((x)/10)<<4)+((x)%10); }
byte decToBcd(byte val) {return ((val / 10 * 16) + (val % 10));};
byte bcdToDec(byte val) {return ((val / 16 * 10) + (val % 16));};



bool set_rtc_date() {/////////////////////////////////////////////////////////////////////////////////////
	Wire.beginTransmission(DS3231_ADDRESS);
	Wire.write(3);//set register to day
	Wire.write(decToBcd(rtc_weekday));
	Wire.write(decToBcd(rtc_day));
	Wire.write(decToBcd(rtc_month));
	Wire.write(decToBcd(rtc_year - 2000));
	if (Wire.endTransmission() == 0) {
		return true;
	}
	else {
		return false;
	};
};

bool set_rtc_time() {
	Wire.beginTransmission(DS3231_ADDRESS);
	Wire.write(0);//set register to time
	Wire.write(decToBcd(rtc_second));
	Wire.write(decToBcd(rtc_minute));
	Wire.write(decToBcd(rtc_hour));
	if (Wire.endTransmission() == 0) {
		return true;
	}
	else {
		return false;
	};
};

bool get_rtc_date() {
	Wire.beginTransmission(DS3231_ADDRESS);
	Wire.write(3);//set register to day
	Wire.endTransmission();
	if (Wire.requestFrom(DS3231_ADDRESS, 4) == 4) { //get 4 bytes(day,date,month,year);
		rtc_weekday = bcdToDec(Wire.read());
		rtc_day = bcdToDec(Wire.read());
		rtc_month = bcdToDec(Wire.read());
		rtc_year = bcdToDec(Wire.read()) + 2000;
		RTCworking = true;
		return true;
	}
	else {
		RTCworking = false;
		return false;
	};
};

bool get_rtc_time() {
	Wire.beginTransmission(DS3231_ADDRESS);
	Wire.write(0);//set register to time
	Wire.endTransmission();
	if (Wire.requestFrom(DS3231_ADDRESS, 3) == 3) { //get 3 bytes (seconds,minutes,hours);
		rtc_second = bcdToDec(Wire.read() & 0x7f);
		rtc_minute = bcdToDec(Wire.read());
		rtc_hour = bcdToDec(Wire.read() & 0x3f);
		return true;
	}
	else {
		return false;
	};
};

bool read_rtc(time_t& cas) {
	if (get_rtc_date()) {
		get_rtc_time();
		tm.Year = rtc_year - 1970;
		tm.Month = rtc_month;
		tm.Day = rtc_day;
		tm.Hour = rtc_hour;
		tm.Minute = rtc_minute;
		tm.Second = rtc_second;
		cas = makeTime(tm);
		return true;
	}
	else {
		return false;
	};
};

float get_rtc_temperature() {
	Wire.beginTransmission(DS3231_ADDRESS);
	Wire.write(17);//set register to DS3132 internal temperature sensor
	Wire.endTransmission();
	if (Wire.requestFrom(DS3231_ADDRESS, 2) == 2) {
		float ttc = (float)(int)Wire.read();
		byte portion = Wire.read();
		if (portion == 0b01000000) ttc += 0.25;
		if (portion == 0b10000000) ttc += 0.5;
		if (portion == 0b11000000) ttc += 0.75;
		return ttc;
	}
	else {
		return MAX_TEMPERATURE_EXCEED;
	};
};