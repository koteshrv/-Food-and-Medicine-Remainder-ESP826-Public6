/*
 * currentDate keys  
 * 0 morningStatus
 * 1 afternoonStatus
 * 2 nightStatus
 * 3 morningTelegramAlert
 * 4 afternoonTelegramAlert
 * 5 nightTelegramAlert
 * 6 morningStatusT
 * 7 afternoonStatusT
 * 8 nightStatusT
*/

/*
 * 
 * timings keys
 * 0 morningTabletAlertH
 * 1 morningTabletAlertM
 * 2 afternoonTabletAlertH
 * 3 afternoonTabletAlertM
 * 4 nightTabletAlertH
 * 5 nightTabletAlertM
 * 6 morningTiffinAlertH
 * 7 morningTiffinAlertM
 * 8 afternoonLunchAlertH
 * 9 afternoonLunchAlertM
 * 10 nightDinnerAlertH
 * 11 nightDinnerAlertM
 */
 
#include <WiFiClient.h>
//#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <LiquidCrystal_I2C.h>
#include <FirebaseESP8266.h>
#include <TM1637Display.h>  
#include <ESP8266WiFi.h> 
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Wire.h> 
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

#define CLK D0                       
#define DIO D3
#define buzzer D5
#define pushButton D6
#define pir D7
#define WIFI_SSID ""
#define WIFI_PASSWORD ""
#define API_KEY ""
#define DATABASE_URL "" 
#define USER_EMAIL ""
#define USER_PASSWORD ""
            
struct TIME {
   int s;
   int m;
   int h;
};

int n = 0;
int pushButtonVal = 0;
int morningTiffinAlertH, morningTiffinAlertM, morningTabletAlertH, morningTabletAlertM;
int afternoonLunchAlertH, afternoonLunchAlertM, afternoonTabletAlertH, afternoonTabletAlertM;
int nightDinnerAlertH, nightDinnerAlertM, nightTabletAlertH, nightTabletAlertM, snoozeT;
bool morningTiffin = false, afternoonLunch = false, nightDinner = false;
String currentDate;
long buttonTimer = 0;
long longPressTime = 3000;
boolean buttonActive = false;
boolean longPressActive = false;
String months[12] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
const long utcOffsetInSeconds = 19802; 
bool morningStatus = false, afternoonStatus = false, nightStatus = false;
int pirStatus;


FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);
TM1637Display display = TM1637Display(CLK, DIO); 
LiquidCrystal_I2C lcd(0x27, 16, 2);
FirebaseJson json;
FirebaseJsonData result;

void buttonPressed(int t) {
    lcd.clear();
    lcd.backlight();
    lcd.print("Button pressed");
    delay(t * 1000);
    lcd.clear();
    lcd.noBacklight();
}

int alert(int count, String currentDate, int statusIndex, int timeIndex) {
  while(count--) {
      alert_sound();
      if (digitalRead(pushButton) == HIGH) {
        if (buttonActive == false) {
          buttonActive = true;
          buttonTimer = millis();
        }
        if ((millis() - buttonTimer > longPressTime) && (longPressActive == false)) {
          buttonPressed(3);
          longPressActive = true;
          Firebase.setBool(fbdo, currentDate + "/" + String(statusIndex), true);
          lcdDisplay("Took Medicine", "at time!", 3); 
          timeClient.update();                 
          int hours = timeClient.getHours();
          int minutes = timeClient.getMinutes();
          int seconds = timeClient.getSeconds();
          String t = String(hours) + ":" + String(minutes) + ":" + String(seconds);
          Firebase.setString(fbdo, currentDate + "/" + String(timeIndex), t);
          return 1;
        }
      } else {
        if (buttonActive == true) {
          if (longPressActive == true) {
            longPressActive = false;
          } else {
              buttonPressed(3);
              Firebase.setBool(fbdo, currentDate + "/" + String(statusIndex), true);
              lcdDisplay("Took Medicine", "at time!", 3); 
              timeClient.update();                 
              int hours = timeClient.getHours();
              int minutes = timeClient.getMinutes();
              int seconds = timeClient.getSeconds();
              String t = String(hours) + ":" + String(minutes) + ":" + String(seconds);
              Firebase.setString(fbdo, currentDate + "/" + String(timeIndex), t);
              return 1;
          }
          buttonActive = false;
        }
      }
  }
  return 0;
}

void lcdDisplay(String a, String b, int d) {
  lcd.clear();
  lcd.backlight();
  lcd.setCursor(0,0);
  lcd.print(a);
  if(b != "") {
      lcd.setCursor(0,1);
      lcd.print(b);  
  }
  if(d != 0) {
    delay(d * 1000);
    lcd.clear();
    lcd.noBacklight();
  }
}

void error_sound() {
  tone(buzzer, 1000); 
  delay(1000);       
}

void alert_sound() {
  tone(buzzer, 1000); 
  delay(1000);       
  noTone(buzzer);     
  delay(1000); 
}

void foodRemainder(int n) {
    bool status = true;
    while(n-- && status) {
      tone(buzzer, 2000); 
      delay(500); 
      noTone(buzzer); 
      delay(200); 
      tone(buzzer, 2000); 
      delay(300); 
      noTone(buzzer); 
      delay(100);
      if (digitalRead(pushButton) == HIGH) {
        if (buttonActive == false) {
          buttonActive = true;
          buttonTimer = millis();
        }
        if ((millis() - buttonTimer > longPressTime) && (longPressActive == false)) {
          buttonPressed(3);
          longPressActive = true;
          lcdDisplay("Stopped", "food remainder", 3);
          status = false;
        }
      } 
      else {
        if (buttonActive == true) {
          if (longPressActive == true) {
            longPressActive = false;
          }
          buttonActive = false;
        }
      }
    }
}

void setup(){
  ArduinoOTA.begin();
  Serial.begin(115200);
  pinMode(D6, INPUT);
  pinMode(D7, INPUT);
  display.clear();
  display.setBrightness(7);  
  lcd.begin();  // sda=0, scl=2
  lcdDisplay("Medicine", "Remainder", 1);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connecting to Wi-Fi");
    lcd.backlight();
    lcd.setCursor(0,0);
    lcd.print("Connecting");
    int i = 0;
    while (WiFi.status() != WL_CONNECTED) {
        lcd.setCursor(i++, 1);
        lcd.print(".");
        Serial.print(".");
        delay(500);
    }
    lcd.clear();
    Serial.println();
    Serial.print("connected: ");
    Serial.println(WiFi.localIP());
    Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);
    lcd.setCursor(0,0);
    lcd.print("Connected");
    alert_sound(); 
    lcd.clear();
    lcd.noBacklight();
    
    config.api_key = API_KEY;
    auth.user.email = USER_EMAIL;
    auth.user.password = USER_PASSWORD;
    config.database_url = DATABASE_URL;
    config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h
    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);
    Firebase.setDoubleDigits(5);

    timeClient.update();  
    delay(1000);
    timeClient.update();               
    int hours = timeClient.getHours();
    int minutes = timeClient.getMinutes();
    int seconds = timeClient.getSeconds();
    unsigned long epochTime = timeClient.getEpochTime();
    struct tm *ptm = gmtime ((time_t *)&epochTime); 
    int monthDay = ptm->tm_mday;
    int currentMonth = ptm->tm_mon+1;
    String currentMonthName = months[currentMonth-1];
    int currentYear = ptm->tm_year+1900;
    currentDate = String(monthDay) + " " + String(currentMonthName) + " " + String(currentYear);
    Firebase.setString(fbdo, "startTime", currentDate + " " + String(hours) + ":" + String(minutes) + ":" + String(seconds));
    Firebase.setInt(fbdo, "Offline", 2);
}

void loop() {
  ArduinoOTA.handle();
  //MDNS.update();
  if(Firebase.ready()) {

      if(digitalRead(pir) == 1)  Firebase.setBool(fbdo, "PIR", true);

      if (digitalRead(pushButton) == HIGH) {
        if (buttonActive == false) {
          buttonActive = true;
          buttonTimer = millis();
        }
        if ((millis() - buttonTimer > longPressTime) && (longPressActive == false)) {
          buttonPressed(3);
          longPressActive = true;
          Firebase.setBool(fbdo, "SOS", true);
          lcdDisplay("Sending msg", "to telegram", 3);
        }
      } else {
        if (buttonActive == true) {
          if (longPressActive == true) {
            longPressActive = false;
          } 
          buttonActive = false;
        }
      }

      int hours, minutes, seconds;
      timeClient.update();                 
      hours = timeClient.getHours();
      minutes = timeClient.getMinutes();
      seconds = timeClient.getSeconds();
      int h, A;
       if (hours == 0) {
        A = 12 * 100 + minutes;
      }
      else if (hours >= 13) {
          h = hours - 12;
          A = h * 100 + minutes;
      } 
      else A = hours * 100 + minutes;
      if((seconds % 2) == 0) display.showNumberDecEx(A, 0b01000000 , false, 4, 0); 
      else display.showNumberDecEx(A, 0b00000000 , false, 4, 0); 

      if(!(seconds % 10)) {
        Firebase.setString(fbdo, "time", String(hours) + ":" + String(minutes) + ":" + String(seconds));
        Firebase.getArray(fbdo, currentDate);
        FirebaseJsonArray &arr = fbdo.jsonArray();
        FirebaseJsonData jsonData;

        arr.get(jsonData, 0);
        morningStatus = jsonData.boolValue;
        arr.get(jsonData, 1);
        afternoonStatus = jsonData.boolValue;
        arr.get(jsonData, 2);
        nightStatus = jsonData.boolValue;
        arr.clear();
      }
      
      int runOnce = true;
      if((!(minutes % 5)) || runOnce){
        runOnce = false;
        
        Firebase.getArray(fbdo, "timings");

        FirebaseJsonArray &timingsArr = fbdo.jsonArray();
        FirebaseJsonData timingsData;
        
        timingsArr.get(timingsData, 0);
        morningTabletAlertH = timingsData.intValue;
        timingsArr.get(timingsData, 1);
        morningTabletAlertM = timingsData.intValue;
        timingsArr.get(timingsData, 2);
        afternoonTabletAlertH = timingsData.intValue;
        timingsArr.get(timingsData, 3);
        afternoonTabletAlertM = timingsData.intValue;
        timingsArr.get(timingsData, 4);
        nightTabletAlertH = timingsData.intValue;
        timingsArr.get(timingsData, 5);
        nightTabletAlertM = timingsData.intValue;
        timingsArr.get(timingsData, 6);
        morningTiffinAlertH = timingsData.intValue;
        timingsArr.get(timingsData, 7);
        morningTiffinAlertM = timingsData.intValue;
        timingsArr.get(timingsData, 8);
        afternoonLunchAlertH = timingsData.intValue;
        timingsArr.get(timingsData, 9);
        afternoonLunchAlertM = timingsData.intValue;
        timingsArr.get(timingsData, 10);
        nightDinnerAlertH = timingsData.intValue;
        timingsArr.get(timingsData, 11);
        nightDinnerAlertM = timingsData.intValue;
        timingsArr.get(timingsData, 12);
        snoozeT = timingsData.intValue;
        timingsArr.clear();

        Firebase.getString(fbdo, "date");
        String date = fbdo.stringData();
        int dateLen = date.length();
        date = date.substring(1, dateLen - 1);
        unsigned long epochTime = timeClient.getEpochTime();
        struct tm *ptm = gmtime ((time_t *)&epochTime); 
        int monthDay = ptm->tm_mday;
        int currentMonth = ptm->tm_mon+1;
        String currentMonthName = months[currentMonth-1];
        int currentYear = ptm->tm_year+1900;
        currentDate = String(monthDay) + " " + String(currentMonthName) + " " + String(currentYear);
        
        if(currentDate != date)
        {      
            FirebaseJsonArray statusArr;
            Firebase.setString(fbdo, "date", currentDate);
            statusArr.add(false,false,false,0,0,0,"NA","NA","NA");
            Firebase.setArray(fbdo, currentDate, statusArr);
            statusArr.clear();
        }
      }
        
      if(!morningTiffin && hours == morningTiffinAlertH && minutes == morningTiffinAlertM) {
       lcdDisplay("Take morning", "tiffin", 0);
       foodRemainder(20); 
       morningTiffin = true;
       lcd.clear();
       lcd.noBacklight();
      }
    
      if(!afternoonLunch && hours == afternoonLunchAlertH && minutes == afternoonLunchAlertM) {
        lcdDisplay("Take afternoon", "lunch", 0);
        foodRemainder(20);
        afternoonLunch = true;
        lcd.clear();
        lcd.noBacklight();
      }

      if(!nightDinner && hours == nightDinnerAlertH && minutes == nightDinnerAlertM) {
         lcdDisplay("Take night", "dinner", 0);
         foodRemainder(20);
         nightDinner = true;
         lcd.clear();
         lcd.noBacklight();
      }

      if(hours == morningTabletAlertH && minutes == morningTabletAlertM && !morningStatus) {
        lcdDisplay("Take morning", "tablets", 0);
        if(!alert(60, currentDate, 0, 6)) {
          lcd.clear();
          lcdDisplay("Take morning", "tablets in " + String(snoozeT) + " min", 3);
        }
        else {
            morningStatus = true;
            Firebase.setBool(fbdo, currentDate + "/3", 1);
            //set telegramstatus to 1 if taken else 2 not taken
        }
      }

      if(hours == afternoonTabletAlertH && minutes == afternoonTabletAlertM && !afternoonStatus) {
        lcdDisplay("Take afternoon", "tablets", 0);
        if(!alert(60, currentDate, 1, 7)) {
          lcd.clear();
          lcdDisplay("Take afternoon", "tablets in " + String(snoozeT) + " min", 3);
        }
        else {
            afternoonStatus = true;
            Firebase.setBool(fbdo, currentDate + "/4", 1);
        }
      }
    
      if(hours == nightTabletAlertH && minutes == nightTabletAlertM && !nightStatus) {
        lcdDisplay("Take night", "tablets", 0);
        if(!alert(60, currentDate, 2, 8)) {
          lcdDisplay("Take night", "tablets in " + String(snoozeT) + " min", 3);
        }
        else {
            nightStatus = true;
            Firebase.setBool(fbdo, currentDate + "/5", 1);
        }
      }
    
      if(hours == morningTabletAlertH && minutes == morningTabletAlertM + snoozeT && !morningStatus) {
        lcdDisplay("Take morning", "tablets", 0);
        if(!alert(60, currentDate, 0, 6)) {
            Firebase.setInt(fbdo, currentDate + "/3", 2);
            lcdDisplay("Missed morning", "tablets", 3);
        }
        else {
            morningStatus = true;
            Firebase.setInt(fbdo, currentDate + "/3", 1);
        }
            
      }
    
      if(hours == afternoonTabletAlertH && minutes == afternoonTabletAlertM + snoozeT && !afternoonStatus) {
        lcdDisplay("Take afternoon", "tablets", 0);
        if(!alert(60, currentDate, 1, 7)) {
            Firebase.setInt(fbdo, currentDate + "/4", 2);
            lcdDisplay("Missed morning", "tablets", 2);
        }
        else {
          afternoonStatus = true;
          Firebase.setInt(fbdo, currentDate + "/4", 1);  
        }
      }
    
      if(hours == nightTabletAlertH && minutes == nightTabletAlertH + snoozeT && !nightStatus) {
        lcdDisplay("Take night", "tablets", 0);
        if(!alert(60, currentDate, 2, 8)) {
            Firebase.setInt(fbdo, currentDate + "/5", 2);
            lcdDisplay("Missed morning", "tablets", 2);
            lcd.clear();
            lcdDisplay("Sent to", "telegram", 2);
            lcd.clear();
            lcd.noBacklight();
        }
        else {
            nightStatus = true;
            Firebase.setInt(fbdo, currentDate + "/5", 1);
        }
      }

  }    
  else {
    lcdDisplay("Firebase ", "error occured", 5);
    error_sound();
  }
}
