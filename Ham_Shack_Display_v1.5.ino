/* HAM Shack Information Display  Sketch version 1.4   02-13-2010
 **************************************************************************************************  
 *  Sketch to display Callsign, Date, UTC and Local Time, Temperature, Humidity and Barometric 
 *  Airpressure on a 20 x 4 I2C LCD display
 *  This sketch is designed on a Arduino Nano. Arduino Uno is tested without any problem.
 *  Using a DS1307 Clockmodule, a AM2320 (DHT22) or similar Temperature and Humidity sensor and a BMP250/BME280 Barometric
 *  Airpressure sensor on Arduino.
 *  For the clockmodule you can also use a DS3231 RTC without need to change the library
 *  This sketch provides automatic switching of Summer- and Wintertime based on Central Europeen Time
 ****************************************************************************************************
   
   *** Clockmodule *** 
   Initialisation of date and time, based on UTC, has te be done before running this sketch
   For this purpose, you can use the SetTime example in the library of the DS1307 Clockmodule
   You have to do this only once
   
   *** Initialisation Date and Time (UTC) in Clockmodule ***
   ************************************* 
   * Switch your PC to UTC time first ! * 
   * ***********************************
   Go to the DS1307 Library amd select Examples
   Select the directory SetTime and the sketch TimeSet.ino
   Put on the Serial Monitor and run this sketch
   Date and time will be set automaticaly.
   Close this temporary sketch and run this HAM Shack Display sketch
   
***************************************************************************************************

   *** Personal info and Selections ***
   Via the serial monitor you can set your Callsign and preferences for the Date format
   Help information is provided
   
   ***  CALLSIGN ***
   Put in your own Callsign
   
   *** Date format ***
   Select your preferences to display the Date Format
   You can select : dd-mm-yyy or ddMMMyyyy ( Example: 27-11-2018 of 10NOV2018
   
   *** Backlight *** (Modified 02-13-2019)
   Its possible to switch On and Off the Backlight via a push button.
   Pin D4 is input. Take Pin D4 with a 100K resistor to ground ( also if you don't use the push Button ! )
   Connect the push button between +5 V and Pin D4 ( Arduino Nano)
   Extra: Build in automatic switch off backlight during night hours
   A lines 143 and 144 you can define start and stoptime of automatic backlight switch
    
   *** Barometric module ***
   In this sketch the Adafruit BMP280 is used, because this sensor can use 5V power and I2C bus
   Barometric Airpressure is displayed in hPa
   If you use Barometric module BME280 instead of BMP280, you have to use another Library and make some changes  
   in this sketch on lines 71/72 and 184/185

   *** Corrections of displayed values ***
   Not all available sensors are equal
   If necessary, you can correct the displayed value of Temperature, Humidity and Barometric Air Pressure
   for your environment. See the mentioned lines in the sketch
   
   ***********************************************************************************************
   This sketch is based on the original simple design "Clock with Thermometer using Arduino" by Timofte Andrei, 
   Translated and modified by Cor Struyk PA0GTB. The code is altered and further structured by Edwin Arts, PA7FRN  
   Final Version 1.0 15 April 2017
   **********************************************************************************************
*/
   
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <AM2320.h>          // temperature / humidity sensor
#include <Time.h>
#include <Timezone.h>    
#include <DS1307RTC.h>       // Clockmodule
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h> // Barometric sensor BMP280
//  #include <Adafruit_BME280.h> // Barometric sensor BME280
#include <EEPROM.h>

#define VERSION_STRING "V1.1"

#define BMP_SCK 13
#define BMP_MISO 12
#define BMP_MOSI 11 
#define BMP_CS 10

#define LED_OFF 0
#define LED_ON 1

#define LCD_ROW_COUNT  4
#define LCD_COL_COUNT 20

#define LCD_ROW_DATE 0
#define LCD_ROW_CALL 0
#define LCD_ROW_UTC  1
#define LCD_ROW_TIME 1
#define WEATHER_ROW   2

#define PRESSURE_ROW 3

//positions in LCD row 0
#define LCD_COL_CALL   1
#define LCD_EMPTY_CALL "          "
#define LCD_COL_DATE   10
#define LCD_EMPTY_DATE "          "

//positions in LCD row 1
#define LCD_COL_UTC   0
#define LCD_COL_TIME 10

//positions in LCD row 2
#define THERM_SYMBOL_COL     0
#define THERM_VALUE_COL      2
#define GRAD_SYMBOL_COL      6
#define HUMIDITY_SYMBOL_COL 10
#define HUMIDITY_VAL_COL    12
#define PROC_SYMBOL_COL     16

//positions in LCD row 3
#define PRESSURE_SYMBOL_COL  4
#define PRESSURE_VAL_COL     7
#define MBAR_COL            14

#define THERM_SYMBOL    1
#define HUMIDITY_SYMBOL 2
#define PRESSURE_SYMBOL 3

#define DATE_FORMAT_dd_mm_yyyy 0
#define DATE_FORMAT_ddMMMyyyy  1
#define DATE_FORMAT_MAX        1

#define CMD_CALL    "call"
#define CMD_DF      "df"

#define ADDR_CALL        0
#define ADDR_DATE_FORMAT 10

#define BTN_STATE_CLOSING 0
#define BTN_STATE_CLOSED  1
#define BTN_STATE_OPENING 2
#define BTN_STATE_OPENED  3

#define MIN_CLOSING_TIME 10  // milliseconds
#define MIN_OPENING_TIME 50  // milliseconds

// Define start an stoptime for automatic Backligh switch
#define BG_ON_HOUR   8
#define BG_OFF_HOUR 24

int dateFormat = DATE_FORMAT_dd_mm_yyyy;

// change these strings if you want another language than Dutch
String strMonth[12] = {
	"JAN", "FEB", "MRT",  "APR",  "MEI",  "JUN",  "JUL",  "AUG",  "SEP",  "OKT",  "NOV",  "DEC"
};

// Design of special Symbols

byte thermometer[8] = // Thermometer Symbol
{
    B00100,
    B01010,
    B01010,
    B01110,
    B01110,
    B11111,
    B11111,
    B01110
};

byte humiditySymbol[8] = // Humidity Symbol 
{
    B00100,
    B00100,
    B01010,
    B01010,
    B10001,
    B10001,
    B10001,
    B01110,
};

byte pressure[8] = // Airpressure Symbol 
{
    B11111,
    B10001,
    B10001,
    B10001,
    B11110,
    B10000,
    B10000,
    B10000
};

// Declare several Objects

// Temperature and Humidity Sensor
AM2320 th;

// Barometric sensor - Choose the Right Module !

Adafruit_BMP280 bme; // I2C bus for BMP280
//  Adafruit_BME280 bme; // I2C bus voor BME280

//Central European Time (Amsterdam, Frankfurt, Paris)
TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 120};     //Central European Summer Time
TimeChangeRule CET = {"CET ", Last, Sun, Oct, 3, 60};       //Central European Standard Time
Timezone CE(CEST, CET);

TimeChangeRule *tcr;        //pointer to the time change rule, use to get the TZ abbrev
time_t utc;

const unsigned long taskTime = 2000;
unsigned long taskTimer = 0;
String callsign = "<callsign>";


/* Display
   set the LCD address to 0x27 for a 20 chars 4 line display
   Set the pins on the I2C chip used for LCD connections: 
*/

LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);  

// Initital state of inputs for switching backlight display by button
const int btnBacklight = 4; 
int buttonState = BTN_STATE_OPENED;
boolean backlightManual = false;
boolean backLightOn = false;
unsigned long timeBtnStateClosing = 0;
unsigned long timeBtnStateOpening = 0;

void setup() {
  Serial.begin(9600); 
  Wire.begin();
  !bme.begin();
  
  pinMode(btnBacklight, INPUT);  // input for switching Backlight

  // Initialisation of display and used Symbols
  lcd.begin(LCD_COL_COUNT, LCD_ROW_COUNT);
  lcd.setBacklight(LED_OFF);  // Switch backlight OFF
  
  lcd.clear();
  lcd.createChar(THERM_SYMBOL,thermometer);
  lcd.createChar(HUMIDITY_SYMBOL,humiditySymbol);
  lcd.createChar(PRESSURE_SYMBOL,pressure);

  // Get time from RTC
  Wire.beginTransmission(0x68);
  Wire.write(0x07); // move pointer to SQW address
  Wire.write(0x10); // sends 0x10 (hex) 00010000 (binary) to control register - turns on square wave
  Wire.endTransmission();

  setSyncProvider(RTC.get);  

  callsign = stringFromEeprom(ADDR_CALL);
  dateFormat = EEPROM.read(ADDR_DATE_FORMAT);
  if (dateFormat > DATE_FORMAT_MAX) {
    dateFormat = 0;
  }

  lcd.setCursor(LCD_COL_CALL, LCD_ROW_CALL);
  lcd.print(callsign);

  lcd.setCursor(THERM_SYMBOL_COL, WEATHER_ROW);
  lcd.write(THERM_SYMBOL);

  lcd.setCursor(GRAD_SYMBOL_COL, WEATHER_ROW);
  lcd.print((char)223); //temperature degree symbol
  lcd.print("C");

  lcd.setCursor(HUMIDITY_SYMBOL_COL, WEATHER_ROW);
  lcd.write(HUMIDITY_SYMBOL);

  lcd.setCursor(PROC_SYMBOL_COL, WEATHER_ROW);
  lcd.print("%");

  lcd.setCursor(PRESSURE_SYMBOL_COL, PRESSURE_ROW);
  lcd.write(PRESSURE_SYMBOL);
  lcd.print("=");
  lcd.setCursor(MBAR_COL, PRESSURE_ROW);
  lcd.print("hPa");

  Serial.println("Ham_Shack_Information_Display " + String(VERSION_STRING));
  Serial.println("Settings can be made via this serial interface.");
  handleCommand("", "");
}

void loop() {   // begin of loop
  utc = now();
  time_t t = CE.toLocal(utc, &tcr);

  checkBtnBacklight();
  
  if (!backlightManual) {
	backLightOn = (hour(t) >= BG_ON_HOUR) && (hour(t) < BG_OFF_HOUR);
  }
  
  if (backLightOn) {
    lcd.setBacklight(LED_ON);
  }
  else {
    lcd.setBacklight(LED_OFF);
  }
   
  printTime(utc, LCD_COL_UTC , LCD_ROW_UTC );
  printTime(t  , LCD_COL_TIME, LCD_ROW_TIME);
  printDate(t  , LCD_COL_DATE, LCD_ROW_DATE);

  checkSerialInput();
  
  unsigned long currentMillis = millis();
  if (currentMillis - taskTimer >= taskTime) {
    taskTimer = currentMillis;
    show_weather();       //displaying temperature, humidity and air pressure
  }
}

void checkBtnBacklight() {
  int newPinState = digitalRead(btnBacklight);
  unsigned long timeCurrent = millis();

  switch (buttonState) {
    case BTN_STATE_CLOSING:
      if (newPinState == LOW) {
        buttonState = BTN_STATE_OPENED;
      }
      else {
        if (timeCurrent - timeBtnStateClosing >= MIN_CLOSING_TIME) {
          buttonState = BTN_STATE_CLOSED;
		  backlightManual = !backlightManual;
		  backLightOn = !backLightOn;
        }
      }
      break;
    case BTN_STATE_CLOSED:
      if (newPinState == LOW) {
         timeBtnStateOpening = millis();
         buttonState = BTN_STATE_OPENING;
      } 
      break;
    case BTN_STATE_OPENING:  
      if (newPinState == HIGH) {
        buttonState = BTN_STATE_CLOSED;
      }
      else if (timeCurrent - timeBtnStateOpening >= MIN_OPENING_TIME) {
        buttonState = BTN_STATE_OPENED;
      }
      break;
    case BTN_STATE_OPENED:  
      if (newPinState == HIGH) {
         timeBtnStateClosing = millis();
         buttonState = BTN_STATE_CLOSING;
      }
      break;
  }  
}
void checkSerialInput() {
  String input = Serial.readString();
  if (input != "") {
    Serial.println();
    Serial.println(input);
    input.trim();
    int idx = input.indexOf(" ");
    String cmd = input;
    String par = "";
    if (idx > -1) {
      cmd = input.substring(0, idx);
      par = input.substring(idx+1);
    }
    cmd.trim();
    par.trim();
    handleCommand(cmd, par);
  }
}

void handleCommand(String cmd, String par) {
  if (cmd == CMD_CALL) {
    if ((par == "?") || (par == "")) {
      Serial.print("type ");
      Serial.print(CMD_CALL);
      Serial.println(" followed by callsign, like:");
      Serial.print(CMD_CALL);
      Serial.println(" PA0GTB");
    }
    else {
      callsign = par;
      lcd.setCursor(LCD_COL_CALL, LCD_ROW_CALL);
      lcd.print(LCD_EMPTY_CALL);
      lcd.setCursor(LCD_COL_CALL, LCD_ROW_CALL);
      lcd.print(callsign);
      stringToEeprom(ADDR_CALL, callsign);
      Serial.print(callsign);
      Serial.println(" set");
    }
  }
  else if (cmd == CMD_DF) {
    if (par == "dd_mm_yyyy") {
      lcd.setCursor(LCD_COL_DATE, LCD_ROW_DATE);
      lcd.print(LCD_EMPTY_DATE);
      dateFormat = DATE_FORMAT_dd_mm_yyyy;
      EEPROM.write(ADDR_DATE_FORMAT, dateFormat);
      Serial.print(par);
      Serial.println(" set");
    }
    else if (par == "ddMMMyyyy") {
      lcd.setCursor(LCD_COL_DATE, LCD_ROW_DATE);
      lcd.print(LCD_EMPTY_DATE);
      dateFormat = DATE_FORMAT_ddMMMyyyy;
      EEPROM.write(ADDR_DATE_FORMAT, dateFormat);
      Serial.print(par);
      Serial.println(" set");
    }
    else {
      Serial.print("type ");
      Serial.print(CMD_DF);
      Serial.println(" followed by format. Valid formats are:");
      Serial.print(CMD_DF);
      Serial.println(" dd_mm_yyyy");
      Serial.print(CMD_DF);
      Serial.println(" ddMMMyyyy");
    }
  }
   else {
     Serial.println("Valid commands are:");
     Serial.print("  ");
     Serial.print(CMD_CALL);
     Serial.println(" to set the callsign"   );
     Serial.print("  ");
     Serial.print(CMD_DF);
     Serial.println(" to set the date format");
     Serial.print("  ");
  }
}

void show_weather() {
  th.Read();

  // Show Temperature and Humidity
  sPrintRightAlign((th.t-2), 4, 1, THERM_VALUE_COL , WEATHER_ROW); // If neccesay, correction of temperature t-x / t+x on display ( shown -2)
  sPrintRightAlign(th.h+0, 4, 1, HUMIDITY_VAL_COL, WEATHER_ROW);   // If neccesary, correction of humidity h-x / h+x on display   ( shown +0)
  
  // Show Barometric Airpressure 
  sPrintRightAlign((bme.readPressure()/100-1.11), 4, 1, PRESSURE_VAL_COL, PRESSURE_ROW); // If neccesary, correction of air pressure hPa on display (shown -1,11)
}

void printTime(time_t t, int col, int row) {
  lcd.setCursor(col, row);
  sPrintDigits(hour(t));
  lcd.print(":");
  sPrintDigits(minute(t));
  lcd.print(":");
  sPrintDigits(second(t));
}

void printDate(time_t t, int col, int row) {
  lcd.setCursor(col, row);
  switch (dateFormat) {
    case DATE_FORMAT_dd_mm_yyyy:
      sPrintDigits(day(t));
      lcd.print("-");
      sPrintDigits(month(t));
      lcd.print("-");
      lcd.print(String(year(t)));
  	  break;
  	case DATE_FORMAT_ddMMMyyyy:
      sPrintDigits(day(t));
      lcd.print(strMonth[month(t)-1]);
      lcd.print(String(year(t)));
      break;
  }
}

void sPrintDigits(int val) {
  if(val < 10) {
    lcd.print("0");
  }
  lcd.print(String(val));
}

void sPrintRightAlign(float val, int positions, int decimals, int col, int row) {
  lcd.setCursor(col, row);
  String strVal = String(val, decimals);
  while (strVal.length() < positions) {
    strVal = " " + strVal;
  }
  lcd.print(strVal);
}

void stringToEeprom(int eepromAddress, String aString) {
  int stringLength = aString.length();
  EEPROM.write(eepromAddress, stringLength);
  for (int i=0; i<stringLength; i++) {
    EEPROM.write(eepromAddress+1+i, aString[i]);
  }
}

String stringFromEeprom(int eepromAddress) {
  String resultString = "";
  int stringLength = EEPROM.read(eepromAddress);
  for (int i=0; i<stringLength; i++) {
    resultString = resultString + " ";
    resultString[i] = EEPROM.read(eepromAddress+1+i);
  }
  return resultString;

}
  
/* ( THE END ) */
