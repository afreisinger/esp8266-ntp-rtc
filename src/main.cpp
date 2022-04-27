/**The MIT License (MIT)
 
 Copyright (c) 2018 by Adrian Freisinger Ltd., https://afreisinger.gitlab.io
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
 */

/*****************************
 * Important: see settings.h to configure your settings!!!
 * ***************************/

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUDP.h>
#include <TimeLib.h>
//#include <DS1307RTC.h>
//#include <avr/pgmspace.h>
//#include <Ethernet.h>
//#include <EthernetUDP.h>
#include <Wire.h>
//#include <RTClib.h>
//#include <SPI.h>

#include "settings.h"
#include "rtc.h"
#include "wifi.h"
//#include "settings.h"
//#include "print.h"
//#include "time.h"
//#include "output_subrutines.h"
//#include "flashrom_msg.h"
//#include "flashrom_subrutines.h"

/////////////////////////////////////////////////////////////////////////////////////
#define BAUDRATE 115200
/////////////////////////////////////////////////////////////////////////////////////
#define NTP_CHECK_PERIOD 		3600//3600
#define NTP_RETRY_CHECK_PERIOD 	60
#define REPORTDS_TIME_PERIOD 	1 //1
volatile bool ntp_interrupt_in_progress = false;
volatile bool NTPworking = false;	
volatile bool NTPsetstime = false;
volatile bool NTPasked = false;
volatile bool NTPavailable = true;
/////////////////////////////////////////////////////////////////////////////////////
#define DHCP_RETRY_PERIOD 300 
#define LCD_UPDATE_PERIOD 1
/////////////////////////////////////////////////////////////////////////////////////
#define ENABLE_RTC_UPDATE /* uncomment next line if updating time in the RTC chip is allowed */
/////////////////////////////////////////////////////////////////////////////////////
// Find the nearest server
// http://www.pool.ntp.org/zone/
// or
// http://support.ntp.org/bin/view/Servers/StratumTwoTimeServers

IPAddress timeServer(10, 0, 1, 250);				// local time server
//IPAddress timeServer(168, 96, 251, 195);				// AR global time server
////////////////////////////////////////////////////////////////////////////////////
WiFiUDP Udp;
#define LOCALUDPPORT 8888

#define DAYLIGHTSAVING_START_MONTH 1 // 
#define DAYLIGHTSAVING_START_HOUR  1 // 2:00:00 -> 3:00:00
#define DAYLIGHTSAVING_STOP_MONTH  1 // 
#define DAYLIGHTSAVING_STOP_HOUR   1 // 3:00:00 -> 2:00:00
#define DAYLIGHTSAVING_TIMESHIFT   0 // Desplazamiento
#define TIME_ZONE_INT 			      -3 // Time Zone
#define TIME_ZONE_NODST 		      "+0000"
#define TIME_ZONE_DST 			      "+0000"
#define TIME_SYSTEM_CLOCK              0b0000000000000001
#define TIME_RTC_CLOCK                 0b0000000000000010
#define TIME_FORMAT_SMTP_DATE          0b0000000000000100
#define TIME_FORMAT_SMTP_MSGID         0b0000000000001000
#define TIME_FORMAT_HUMAN              0b0000000000010000
#define TIME_FORMAT_HUMAN_SHORT        0b0000000000100000
#define TIME_FORMAT_TIME_ONLY          0b0000000001000000
#define TIME_FORMAT_TIME_ONLY_WO_COLON 0b0000000010000000
#define TIME_FORMAT_DATE_ONLY          0b0000000100000000
#define TIME_FORMAT_DATE_SHORT         0b0000001000000000
#define TIME_FORMAT_DATE_FILE          0b0000010000000000

String TIME_ZONE = TIME_ZONE_NODST;




time_t EvaluateDaylightsavingTime(time_t result) {
	if (
		(
		(                                                                       // plati od
			(month(result) == DAYLIGHTSAVING_START_MONTH)                       // breznove
			&& (month(nextSunday(result)) == (DAYLIGHTSAVING_START_MONTH + 1))  // last week
			&&
			(
			(weekday(result) != 1)
				||
				(hour(result) >= DAYLIGHTSAVING_START_HOUR)                      // od dvou hodin rano
				)
			)
			|| (month(result) > DAYLIGHTSAVING_START_MONTH)                      // 	
			)
		&&
		(
		(                                                                       // plati do
			(month(result) == DAYLIGHTSAVING_STOP_MONTH)                        // rijnove
			&& (month(nextSunday(result)) == DAYLIGHTSAVING_STOP_MONTH)         // posledni nedele
			)
			||
			(
			(month(result) == DAYLIGHTSAVING_STOP_MONTH)
				&& (month(nextSunday(result)) == (DAYLIGHTSAVING_STOP_MONTH + 1))
				&& (weekday(result) == 1)
				&& (hour(result) < DAYLIGHTSAVING_STOP_HOUR)
				)
			|| (month(result) < DAYLIGHTSAVING_STOP_MONTH)                  // a v predchozich mesicich
			)
		) {
		result = result + DAYLIGHTSAVING_TIMESHIFT * SECS_PER_HOUR;
		TIME_ZONE = TIME_ZONE_DST;
	}
	else {
		TIME_ZONE = TIME_ZONE_NODST;
	};
	return result;
};


time_t time_dhcp_retry = 0;
time_t time_last_ntp_check = 0;
time_t rtc_now = 0;
time_t rtc_epoch = 0;
time_t time_last_lcd = 0;
time_t time_last_reportDS = 0;
/*
//connectWifi////////////////////////////////////////////////////////////////////////////////////////////////
void connectWifi() {
  bool isConnected = false;
  if (WiFi.status() == WL_CONNECTED && !isConnected) return;
  //Manual Wifi
  Serial.printf("Connecting to WiFi %s/%s", WIFI_SSID.c_str(), WIFI_PASS.c_str());
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.hostname(WIFI_HOSTNAME);
  WiFi.begin(WIFI_SSID.c_str(), WIFI_PASS.c_str());
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    isConnected = false;
  }
  
  Serial.println("connected.");
  digitalWrite(LED_BUILTIN, LOW);
  isConnected = true;
}
//connectWifi////////////////////////////////////////////////////////////////////////////////////////////////
*/

//#define BUFFERSIZE 80
//static char buffer[BUFFERSIZE + 1];

#define WELCOME_MESSAGE              0
#define UNINITIALIZED_RTC_MESSAGE    1
#define DHCP_WAIT_MESSAGE            2
#define DHCP_FAIL_MESSAGE            3
#define DHCP_PASS_MESSAGE            4
#define IP_MESSAGE                   5
#define MASK_MESSAGE                 6
#define GATEWAY_MESSAGE              7
#define DNS_MESSAGE                  8
#define TS_MESSAGE                   9
#define MAC_MESSAGE                 10
#define NTP_WAIT_MESSAGE            11
#define NTP_SEND_MESSAGE            12
#define NTP_RESPONSES_MESSAGE       13
#define NTP_NO_RESPONSE_MESSAGE     14
#define NTP_ERROR_MESSAGE           15
#define RTC_TIME_MESSAGE            16
#define RTC_OK_MESSAGE              17
#define RTC_NEED_SYNC_MESSAGE       18
#define SECONDS_MESSAGE             19
#define RTC_SYNC_MESSAGE            20
#define RTC_TF_MESSAGE              21
#define RTC_DF_MESSAGE              22
#define INFO_MESSAGE                23
#define RTC_ERROR_MESSAGE           24
#define TEMPERATURE_ERRROR_MESSAGE  25
#define TIME_ERRROR_MESSAGE         26
#define SENSORS_MESSAGE             27
#define RTC_NOT_PRESENT             28
#define TIMESOURCE_ERRROR_MESSAGE   29
#define ERROR_MESSAGE				        30
#define SENSORS_MESSAGE_2           31
#define SD_INITIALIZED_MESSAGE      32
#define SD_READING_MESSAGE          33
#define SD_FAILED_MESSAGE			      34
#define SD_INSERTED_MESSAGE			    35
#define SD_WIRING_MESSAGE			      36
#define SD_CHIPSELECT_MESSAGE		    37
#define SD_OK_MESSAGE				        38
#define SD_OK_DISABLE				        39
#define SD_TYPE_MESSAGE				      40
#define SD_TYPE_SD1_MESSAGE		    	41
#define SD_TYPE_SD2_MESSAGE			    42
#define SD_TYPE_SDHC_MESSAGE		    43
#define SD_TYPE_UNKNOWN_MESSAGE		  44
#define SD_ERROR_MESSAGE			      45
#define SD_CLUSTERS_MESSAGE			    46
#define SD_BLOCKS_MESSAGE			      47
#define SD_TOTAL_BLOCKS_MESSAGE		  48
#define SD_VOLUME_TYPE				      49
#define SD_VOLUME_KB_MESSAGE		    50
#define SD_VOLUME_MB_MESSAGE		    51
#define SD_VOLUME_GB_MESSAGE		    52
#define SD_FORMATTED_MESSAGE		    53
#define SD_FREE_SPACE_MB       		  54
#define SD_FREE_SPACE_GB 		        55
#define SD_UNIT_KB          		    56
#define SD_UNIT_MB          		    57
#define SD_UNIT_GB           		    58
#define SD_USED_CAPACITY            59
#define SD_USED_PERCENTAGE	        60
#define SD_FILES_LS     	          61
#define SD_FILES_COUNT  	          62
#define SD_DIR_COUNT  	            63
#define SD_LOG_TO                   64
#define PROJECT_MESSAGE				      65
#define VERSION_MESSAGE				      66
#define DATE_MESSAGE				        67
#define AUTHOR_MESSAGE				      68
#define EMAIL_MESSAGE				        69
#define COPYRIGHT_MESSAGE			      70
#define CLEAR_MESSAGE				        71
#define EMPTY_MESSAGE				        72

//The ESP8266 doesn't have a separate address space for "PROGMEM" so those function are all stubbed for compatibility.

const char TextInfo0[] PROGMEM = "Get NTP and RTC chip time\r\n";
//const char TextInfo0[] PROGMEM = "\r\n\xA9 2018, Freisinger \x8Aec\r\n\r\nGet NTP and RTC chip time\r\n";
const char TextInfo1[] PROGMEM = "\r\nUninitialized RTC time and temperature:";
const char TextInfo2[] PROGMEM = "Waiting for DHCP lease ...";
const char TextInfo3[] PROGMEM = "Failed to configure IPv4 for using Dynamic Host Configuration Protocol!";
const char TextInfo4[] PROGMEM = "Dynamic Host Configuration Protocol passed.";
const char TextInfo5[] PROGMEM = "Board IP address";
const char TextInfo6[] PROGMEM = "Network subnet mask";
const char TextInfo7[] PROGMEM = "Network gateway IP address";
const char TextInfo8[] PROGMEM = "DNS IP address";
const char TextInfo9[] PROGMEM = "Time server IP address";
const char TextInfo10[] PROGMEM = "\r\nThe network interface MAC address ";
const char TextInfo11[] PROGMEM = "Waiting to obtain the accurate time from NTP server ";
const char TextInfo12[] PROGMEM = "Sending NTP request... ";
const char TextInfo13[] PROGMEM = "and NTP responses.\r\nNTP time: ";
const char TextInfo14[] PROGMEM = "No NTP Response.";
const char TextInfo15[] PROGMEM = "Failed to get the current time from NTP server.";
const char TextInfo16[] PROGMEM = "RTC time: ";
const char TextInfo17[] PROGMEM = "RTC chip do not need to be updated.";
const char TextInfo18[] PROGMEM = "RTC need to be updated. Current time difference is ";
const char TextInfo19[] PROGMEM = " second(s).";
const char TextInfo20[] PROGMEM = "RTC chip has been successfully updated.";
const char TextInfo21[] PROGMEM = "Error setting time to RTC.";
const char TextInfo22[] PROGMEM = "Error setting date to RTC.";
const char TextInfo23[] PROGMEM = "RTC Time and Temperature: ";
const char TextInfo24[] PROGMEM = "Failed to get the current time from RTC chip.";
const char TextInfo25[] PROGMEM = "No data (temperature sensor error).";
const char TextInfo26[] PROGMEM = "Current time failed to get either the NTP server or the RTC chip.";
const char TextInfo27[] PROGMEM = "TMP36: ";
const char TextInfo28[] PROGMEM = "Unable to compare NTP and RTC time (RTC is not present?)";
const char TextInfo29[] PROGMEM = "Error calling function: function ComposeTimeStamp requires time source (CPU or RTC).";
const char TextInfo30[] PROGMEM = "Error getting time";
const char TextInfo31[] PROGMEM = "DHT22 Temperature and Humidity: ";
const char TextInfo32[] PROGMEM = "\nInitializing SD card...";
const char TextInfo33[] PROGMEM = "Reading SD card...";
const char TextInfo34[] PROGMEM = "Initialization failed. Things to check:";
const char TextInfo35[] PROGMEM = "* is a card inserted?";
const char TextInfo36[] PROGMEM = "* is your wiring correct?";
const char TextInfo37[] PROGMEM = "* did you change the chipSelect pin to match your shield or module?";
const char TextInfo38[] PROGMEM = "Wiring is correct and a card is present.";
const char TextInfo39[] PROGMEM = "SD card is disable.";
const char TextInfo40[] PROGMEM = "Card type:        ";
const char TextInfo41[] PROGMEM = " SD1";
const char TextInfo42[] PROGMEM = " SD2";
const char TextInfo43[] PROGMEM = " SDHC";
const char TextInfo44[] PROGMEM = " Unknown";
const char TextInfo45[] PROGMEM = "Could not find FAT16/FAT32 partition.\nMake sure you've formatted the card";
const char TextInfo46[] PROGMEM = "Clusters:          ";
const char TextInfo47[] PROGMEM = "Blocks x Cluster:  ";
const char TextInfo48[] PROGMEM = "Total Blocks:      ";
const char TextInfo49[] PROGMEM = "Volume type is:    FAT";
const char TextInfo50[] PROGMEM = "Volume size (Kb):  ";
const char TextInfo51[] PROGMEM = "Volume size (Mb):  ";
const char TextInfo52[] PROGMEM = "Volume size (Gb):  ";
const char TextInfo53[] PROGMEM = "\nFiles found on the card (name, date and size in bytes): ";
const char TextInfo54[] PROGMEM = "Free space in (Mb): ";
const char TextInfo55[] PROGMEM = "Free space in (Gb): "; //The SD card is disable."
const char TextInfo56[] PROGMEM = "Kbytes";
const char TextInfo57[] PROGMEM = "Mbytes";
const char TextInfo58[] PROGMEM = "Gbytes";
const char TextInfo59[] PROGMEM = " (Used: ";
const char TextInfo60[] PROGMEM = "Percentage (Used): "; 
const char TextInfo61[] PROGMEM = "./"; 
const char TextInfo62[] PROGMEM = " files";
const char TextInfo63[] PROGMEM = " dirs ";
const char TextInfo64[] PROGMEM = "Logging to: ";
const char TextInfo65[] PROGMEM = "Weather Station";
const char TextInfo66[] PROGMEM = "Version 1.1.8.18";
const char TextInfo67[] PROGMEM = "August 2018";
const char TextInfo68[] PROGMEM = "Adrian Freisinger";
const char TextInfo69[] PROGMEM = "afreisinger@gmail.com";
const char TextInfo70[] PROGMEM = "Everyone is permitted to copy and distribute verbatim copies of this license document, but changing it is not allowed.\n";
const char TextInfo71[] PROGMEM = "                ";
const char TextInfo72[] PROGMEM = "";


const char* const TextItemPointers[] PROGMEM = {
	TextInfo0,TextInfo1,TextInfo2,TextInfo3,TextInfo4,TextInfo5,TextInfo6,TextInfo7,TextInfo8,TextInfo9,
	TextInfo10,TextInfo11,TextInfo12,TextInfo13,TextInfo14,TextInfo15,TextInfo16,TextInfo17,TextInfo18,TextInfo19,
	TextInfo20,TextInfo21,TextInfo22,TextInfo23,TextInfo24,TextInfo25,TextInfo26,TextInfo27,TextInfo28,TextInfo29,
	TextInfo30,TextInfo31,TextInfo32,TextInfo33,TextInfo34,TextInfo35,TextInfo36,TextInfo37,TextInfo38,TextInfo39,
	TextInfo40,TextInfo41,TextInfo42,TextInfo43,TextInfo44,TextInfo45,TextInfo46,TextInfo47,TextInfo48,TextInfo49,
	TextInfo50,TextInfo51,TextInfo52,TextInfo53,TextInfo54,TextInfo55,TextInfo56,TextInfo57,TextInfo58,TextInfo59,
	TextInfo60,TextInfo61,TextInfo62,TextInfo63,TextInfo64,TextInfo65,TextInfo66,TextInfo67,TextInfo68,TextInfo69,
	TextInfo70,TextInfo71,TextInfo72,
};


String GetTextFromFlashMemory(int ItemIndex){return String(TextItemPointers[ItemIndex]);} //ESP8266

/*
String GetTextFromFlashMemory(int ItemIndex) //AVR
{
	int i = 0;
	char c;
	while ((c != '\0') && (i < BUFFERSIZE))
	{
		c = pgm_read_byte(pgm_read_word(&TextItemPointers[ItemIndex]) + i);
		string_buffer[i] = c;
		i++;
	}
	string_buffer[i] = '\0';
	return String(string_buffer);
}
*/

/////////////////////////////////////////////////////////////////////////////////////////////////////////
String ComposeZerosLeadedNumber(unsigned long int MyNumber, byte NumberOfCharacters) {
	String TempString = "";
	for (byte index = 1; index <= NumberOfCharacters; index++) {
		TempString = "0" + TempString;
	};
	TempString = TempString + String(MyNumber);
	int ifrom = TempString.length() - NumberOfCharacters;
	int ito = TempString.length();
	return TempString.substring(ifrom, ito);
};//ComposeZerosLeadedNumber/////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////
String ComposeTimeStamp(unsigned int details, time_t datum) {
	String strday;
	String strmonth;
	String stryear;
	String strshortyear;
  String strhour;
	String strminute;
	String strsecond;
	String strweekday;
	int myvalue;
	if (
		((details & TIME_RTC_CLOCK) != TIME_RTC_CLOCK)
		&&
		((details & TIME_SYSTEM_CLOCK) != TIME_SYSTEM_CLOCK)
		) {
		strday = GetTextFromFlashMemory(TIMESOURCE_ERRROR_MESSAGE);
		
    return strday;
	};
	if ((details & TIME_RTC_CLOCK) == TIME_RTC_CLOCK) {
		if (datum <= 0) {
			if (!read_rtc(datum)) {
				if (NTPworking) {
					strday = GetTextFromFlashMemory(RTC_ERROR_MESSAGE);
				
        }
				else {
					strday = GetTextFromFlashMemory(TIME_ERRROR_MESSAGE);
				};
				return strday;
			};
		};
	};
	if ((details & TIME_SYSTEM_CLOCK) == TIME_SYSTEM_CLOCK) {
		if (datum <= 0) {
			datum = now();
		};
	};
	datum = EvaluateDaylightsavingTime(datum);
	strday = ComposeZerosLeadedNumber(day(datum), 2);
	myvalue = month(datum);
	if ((details & TIME_FORMAT_SMTP_DATE) == TIME_FORMAT_SMTP_DATE) {
		strmonth = monthShortStr(myvalue);
	}
	else {
		strmonth = ComposeZerosLeadedNumber(myvalue, 2);
	};
	stryear = String(year(datum));
	strshortyear = String(year(datum) - 2000);
	strhour = ComposeZerosLeadedNumber(hour(datum), 2);
	strminute = ComposeZerosLeadedNumber(minute(datum), 2);
	strsecond = ComposeZerosLeadedNumber(second(datum), 2);
	int tweekday;
	if (details & TIME_RTC_CLOCK) {
		tweekday = rtc_weekday;
	}
	else {
		tweekday = weekday(datum);
	};
	if ((details & TIME_FORMAT_HUMAN) == TIME_FORMAT_HUMAN) {
		strweekday = dayStr(tweekday);
	}
	else {
		strweekday = dayShortStr(tweekday);//dayShortStr(tweekday);
	};
	if ((details & TIME_FORMAT_HUMAN) == TIME_FORMAT_HUMAN) {
		return String(strday + "." + strmonth + "." + stryear + " " + strhour + ":" + strminute + ":" + strsecond + " " + strweekday);
	}
	else {
		if ((details & TIME_FORMAT_HUMAN_SHORT) == TIME_FORMAT_HUMAN_SHORT) {
			return String(strday + "." + strmonth + "." + stryear + " " + strhour + ":" + strminute + ":" + strsecond + " "); //20 characters for LCD
		}
		else {
			if ((details & TIME_FORMAT_TIME_ONLY) == TIME_FORMAT_TIME_ONLY) {
				return String(strhour + ":" + strminute + ":" + strsecond);
			}
			else {
				if ((details & TIME_FORMAT_TIME_ONLY_WO_COLON) == TIME_FORMAT_TIME_ONLY_WO_COLON) {
					return String(strhour + " " + strminute + " " + strsecond);
				}
				else {
					if ((details & TIME_FORMAT_DATE_ONLY) == TIME_FORMAT_DATE_ONLY) {
						return String(strday + "." + strmonth + "." + stryear);
					}
					else {
						if ((details & TIME_FORMAT_SMTP_MSGID) == TIME_FORMAT_SMTP_MSGID) {
							return String(stryear + strmonth + strday + strhour + strminute + strsecond + ".") + String(millis());
						}
						else {
							if ((details & TIME_FORMAT_DATE_SHORT) == TIME_FORMAT_DATE_SHORT) {
								return String(strweekday + " " + strday + "/" + strmonth + "/" + strshortyear);
							}
							else {
								if ((details & TIME_FORMAT_DATE_FILE) == TIME_FORMAT_DATE_FILE) {
									return String(stryear + strmonth + strday);
							}
								else {
								   //return String(strweekday + ", " + strday + ". " + strmonth + "." + stryear + " " + strhour + ":" + strminute + ":" + strsecond + " " + TIME_ZONE);
									return String(" " + strweekday + " " + strday + "/" + strmonth + "/" + stryear);

								};
							};
						};
					};
				};
			};
		};
	};
};
//ComposeTimeStamp////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////
/*-------- DHCP subroutine ----------*/
/////////////////////////////////////////////////////////////////////////////////////
bool checkdhcp() 
{
	
	if (WiFi.begin() == 0) 
	{
		Serial.println((DHCP_FAIL_MESSAGE));
		return false;
	}
	else 
	{
		Serial.println(GetTextFromFlashMemory(DHCP_PASS_MESSAGE));
		Serial.print(GetTextFromFlashMemory(IP_MESSAGE)+ ":");
		for (byte thisByte = 0; thisByte < 4; thisByte++) 
		{
			Serial.print(String(WiFi.localIP()[thisByte], DEC));
			if (thisByte < 3) {Serial.print(F("."));};
		};
	};
	Serial.println();
	Udp.begin(LOCALUDPPORT);
	return true;
};

/////////////////////////////////////////////////////////////////////////////////////
/*-------- NTP subroutines ----------*/
/////////////////////////////////////////////////////////////////////////////////////
#define NTP_PACKET_SIZE 48 // NTP time is in the first 48 bytes of message
/////////////////////////////////////////////////////////////////////////////////////
time_t getNtpTime() {

	if (ntp_interrupt_in_progress == false) {
		ntp_interrupt_in_progress = true;
		Serial.println(GetTextFromFlashMemory(NTP_SEND_MESSAGE));
		while (Udp.parsePacket() > 0); // discard any previously received packets
		byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets 
		memset(packetBuffer, 0, NTP_PACKET_SIZE);
		packetBuffer[0] = 0b11100011;   // LI, Version, Mode
		packetBuffer[1] = 0;     // Stratum, or type of clock
		packetBuffer[2] = 6;     // Polling Interval
		packetBuffer[3] = 0xEC;  // Peer Clock Precision
		packetBuffer[12] = 49;
		packetBuffer[13] = 0x4E;
		packetBuffer[14] = 49;
		packetBuffer[15] = 52;
		Udp.beginPacket(timeServer, 123); //NTP requests are to port 123
		Udp.write(packetBuffer, NTP_PACKET_SIZE);
		Udp.endPacket();
		delay(1500);
		volatile time_t result = 0;
		volatile int size = Udp.parsePacket();
		
		if (size >= NTP_PACKET_SIZE) {
			
			Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
			volatile unsigned long secsSince1900;
			// convert four bytes starting at location 40 to a long integer
			secsSince1900 = (unsigned long)packetBuffer[40] << 24;
			secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
			secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
			secsSince1900 |= (unsigned long)packetBuffer[43];
			result = secsSince1900 - 2208988800UL + TIME_ZONE_INT * SECS_PER_HOUR;
			if (
				(
				(                                                                  
					(month(result) == DAYLIGHTSAVING_START_MONTH)                     
					&& (month(nextSunday(result)) == (DAYLIGHTSAVING_START_MONTH + 1)) 
					&&
					(
					(weekday(result) != 1)
						||
						(hour(result) >= DAYLIGHTSAVING_START_HOUR)                      
						)
					)
					|| (month(result) > DAYLIGHTSAVING_START_MONTH)                
					)
				&&
				(
				(                                                                  
					(month(result) == DAYLIGHTSAVING_STOP_MONTH)                       
					&& (month(nextSunday(result)) == DAYLIGHTSAVING_STOP_MONTH)       
					)
					||
					(
					(month(result) == DAYLIGHTSAVING_STOP_MONTH)
						&& (month(nextSunday(result)) == (DAYLIGHTSAVING_STOP_MONTH + 1))
						&& (weekday(result) == 1)
						&& (hour(result) < DAYLIGHTSAVING_STOP_HOUR)
						)
					|| (month(result) < DAYLIGHTSAVING_STOP_MONTH)                 
					)
				) {
				result = result + DAYLIGHTSAVING_TIMESHIFT * SECS_PER_HOUR;
				TIME_ZONE = TIME_ZONE_DST;
			}
			else {
				TIME_ZONE = TIME_ZONE_NODST;
			};
			NTPworking = true;
			Serial.print(GetTextFromFlashMemory(NTP_RESPONSES_MESSAGE));
			Serial.print(ComposeTimeStamp(TIME_SYSTEM_CLOCK | TIME_FORMAT_HUMAN, result));
			Serial.println();
			Serial.print(GetTextFromFlashMemory(RTC_TIME_MESSAGE));
			Serial.println(ComposeTimeStamp(TIME_RTC_CLOCK | TIME_FORMAT_HUMAN, 0));
		}
		else {
			NTPworking = false;
			Serial.println(GetTextFromFlashMemory(NTP_NO_RESPONSE_MESSAGE));
					}
		ntp_interrupt_in_progress = false;
		return result;
	};
	return true; // *
};
/////////////////////////////////////////////////////////////////////////////////////

bool checkntp()
{
	
	Serial.print(GetTextFromFlashMemory(NTP_WAIT_MESSAGE));
	for (byte thisByte = 0; thisByte < 4; thisByte++) {
		Serial.print(timeServer[thisByte], DEC);
		if (thisByte < 3) {
			Serial.print(F("."));
		};
	};
	Serial.println(F(" ..."));
	setTime(getNtpTime());
	get_rtc_date();
	get_rtc_time();
	time_t avr_now = now();
	setTime(rtc_hour, rtc_minute, rtc_second, rtc_day, rtc_month, rtc_year);
	time_t rtc_now = now();
	setTime(avr_now);
	if (rtc_now == avr_now) {
		Serial.println(GetTextFromFlashMemory(RTC_OK_MESSAGE));
		Serial.println();
		return true;
	}
	else {
		if (NTPworking) {
			rtc_second = second();
			rtc_minute = minute();
			rtc_hour = hour();
			rtc_weekday = weekday();
			rtc_day = day();
			rtc_month = month();
			rtc_year = year();
			if (!set_rtc_time()) {
				Serial.println(GetTextFromFlashMemory(RTC_TF_MESSAGE));
			}
			else {
				if (!set_rtc_date()) {
					Serial.println(GetTextFromFlashMemory(RTC_DF_MESSAGE));
				}
				else {
					Serial.println(GetTextFromFlashMemory(RTC_SYNC_MESSAGE));
				};
			};
			return true;
		}
		else {
			Serial.println(GetTextFromFlashMemory(ERROR_MESSAGE));
			return false;
		};
	};
};

bool  MaintainTimeSources(bool force) {/////////////////////////////////////////////////////////////////////////////////////
	
	String OutString = "";
	if ((NTPasked) || (force)) {
		NTPasked = false;
		time_t avr_now = 0;
		time_t timediff = 0;
		if (!read_rtc(rtc_now)) {
			rtc_now = 0;
		};
		avr_now = now();
		if (NTPworking) {
			Serial.print(GetTextFromFlashMemory(NTP_RESPONSES_MESSAGE));
			Serial.print(ComposeTimeStamp(TIME_SYSTEM_CLOCK | TIME_FORMAT_HUMAN, avr_now));
			Serial.print(", SYS Epoch: " + String(avr_now) + "\r\n");
		}
		else {
			Serial.print(GetTextFromFlashMemory(NTP_ERROR_MESSAGE) + "\r\n");
			return false;
		};
		if (RTCworking) {
			Serial.print(GetTextFromFlashMemory(RTC_TIME_MESSAGE));
			Serial.print(ComposeTimeStamp(TIME_RTC_CLOCK | TIME_FORMAT_HUMAN, rtc_now));
			Serial.print(", RTC Epoch: " + String(rtc_now) + "\r\n");
			String strSign = "";
			if (rtc_now > avr_now) {
				timediff = rtc_now - avr_now;
				strSign = "+";
			}
			else {
				timediff = avr_now - rtc_now;
				strSign = "-";
			};

			if (timediff == 0) {
				OutString = GetTextFromFlashMemory(RTC_OK_MESSAGE);
				Serial.println(OutString);
			}
			else {
				OutString = GetTextFromFlashMemory(RTC_NEED_SYNC_MESSAGE) + strSign + String(timediff) + GetTextFromFlashMemory(SECONDS_MESSAGE);
				Serial.println(OutString);
					#if defined(ENABLE_RTC_UPDATE)
									rtc_second = second();
									rtc_minute = minute();
									rtc_hour = hour();
									rtc_weekday = weekday();
									rtc_day = day();
									rtc_month = month();
									rtc_year = year();
									if (!set_rtc_time()) {
										Serial.print(GetTextFromFlashMemory(RTC_TF_MESSAGE) + "\r\n");
									}
									else {
										if (!set_rtc_date()) {
											Serial.print(GetTextFromFlashMemory(RTC_DF_MESSAGE) + "\r\n");
										}
										else {
											Serial.print(GetTextFromFlashMemory(RTC_SYNC_MESSAGE) + "\r\n");
										};
									};
					#endif    
			};

		}
		else {
			Serial.print(GetTextFromFlashMemory(RTC_NOT_PRESENT) + "\r\n");
		};
		return true;
	};
	return false;
};



/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float ComposeTemperatureValue(byte sensor){
	float TemperatureC1=0;
		
		switch(sensor){
			case TEMPERATURE_SENSOR_DS3231:
			TemperatureC1 = get_rtc_temperature();
			break;
	
	}		
	return TemperatureC1;
};
//ComposeTemperatureValue////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
String ComposeTemperatureString(byte sensor, bool displaytype) {
	String TempStr = "";
	float TemperatureC = ComposeTemperatureValue(sensor);
	
	if (TemperatureC < MAX_TEMPERATURE_EXCEED) {
		char buffer[16];
		TempStr = dtostrf(TemperatureC, 5, 2, buffer);
		if (displaytype) {
			TempStr = TempStr + "\xDF\x43"; //oC for Asian Character Set
			
		}
		else {
			
			TempStr = TempStr + "\xC2\xB0\x43"; //oC UTF-8 Encode
		};
	}
	else {
		//Error message
		TempStr = GetTextFromFlashMemory(TEMPERATURE_ERRROR_MESSAGE);
	};
	return TempStr;
};
//ComposeTemperatureString///////////////////////////////////////////////////////////////////////////////////////////






void ShowTime(bool ForceShowTemperature) {/////////////////////////////////////////////////////////////////////////////////////
	
	if (time_last_lcd + LCD_UPDATE_PERIOD <= now()) {
		time_last_lcd = now();
		String PrtString;

		if ((time_last_reportDS + REPORTDS_TIME_PERIOD <= now()) || (ForceShowTemperature)) {
			time_last_reportDS = now();

			//order to avoid distortion running NTP process console print is omited
			if (!NTPasked) {
				Serial.print(GetTextFromFlashMemory(INFO_MESSAGE));
				Serial.print(ComposeTimeStamp(TIME_RTC_CLOCK | TIME_FORMAT_HUMAN, 0) + " ");
				Serial.print(ComposeTemperatureString(TEMPERATURE_SENSOR_DS3231, false)+ "\r\n");
			};
		};
	};
};





void setup() {
  Serial.begin(BAUDRATE, SERIAL_8N1);
  Wire.begin();
  pinMode(LED_BUILTIN, OUTPUT);
  
  connectWiFi(WIFI_SSID,WIFI_PASS,WIFI_HOSTNAME);
  
  

  
  time_t rtc_uninitialized_time = 0;
	
	if (read_rtc(rtc_uninitialized_time)) {setTime(rtc_uninitialized_time);};
	time_t rtc_test = 0;
	read_rtc(rtc_test);
 
Serial.print(GetTextFromFlashMemory(WELCOME_MESSAGE));
Serial.print(GetTextFromFlashMemory(UNINITIALIZED_RTC_MESSAGE) + " " + ComposeTimeStamp(TIME_RTC_CLOCK | TIME_FORMAT_HUMAN, rtc_uninitialized_time) + " " + ComposeTemperatureString(TEMPERATURE_SENSOR_DS3231, false) + "\r\n"); // first serial message
//Serial.print(GetTextFromFlashMemory(UNINITIALIZED_RTC_MESSAGE) + " " + ComposeTimeStamp(TIME_RTC_CLOCK | TIME_FORMAT_HUMAN, rtc_uninitialized_time) + "\r\n"); // first serial message
 
}


void loop() {
	if (checkdhcp()) 
	{
		while (true) 
		{
			if (checkntp()) 
			{
				time_last_ntp_check = now();
				NTPavailable = true;
			}
			else 
			{
				time_last_ntp_check = now() - NTP_CHECK_PERIOD + NTP_RETRY_CHECK_PERIOD;
				NTPavailable = false;
			};
			
			while(time_last_ntp_check + NTP_CHECK_PERIOD > now()) {
				ShowTime(true);
				MaintainTimeSources(false); //if NTP server was asked by system time updater, show results
				//delay(5000);
			};
		};
	}
	else 
	{
		if (rtc_epoch == 0) 
		{
			if (read_rtc(rtc_epoch)) 
			{
				setTime(rtc_epoch);
			};
		};
		time_dhcp_retry = now();
		bool FirstTime = true;
		while (time_dhcp_retry + DHCP_RETRY_PERIOD >= now()) 
		{
			ShowTime(FirstTime);
			//WriteTime();
			if (FirstTime) 	{FirstTime = false;};
		};
	};
};
