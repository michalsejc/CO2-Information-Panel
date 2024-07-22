#include <strings.h>
#include <SPI.h>
#include <SD.h>
#include "paulvha_SCD30.h"
#include <DMDESP.h>
#include <fonts/Mono5x7.h>
#include <Wire.h>
#include <RTClib.h>

RTC_DS3231 rtc;
// pouzitie D0 vedie ku poskodeniu dat
const int chipSelect = D8; // pouzit D0 pre Wemos D1 Mini
const int SPIselect = D4;  // riadiaci signal vyhybky SPI
File root;
char zaznam[15];  // 14 znakov a null
#define scd_debug 0
#define SCD30WIRE Wire
SCD30 airSensor;
//SETUP DMD
#define DISPLAYS_WIDE 1 // pocet stlpcov
#define DISPLAYS_HIGH 1 // pocet riadkov
DMDESP Disp(DISPLAYS_WIDE, DISPLAYS_HIGH); 
// nastavuje dlzku vysvietenia jednotlivych hodnot
// kedze sa zobrazuju 3 hodnoty, tak trvanie celej slucky je ~ 3*DUR_VAL
#define DUR_VAL 2500

void setup() {
  // otvorenie seriovej linky
  Serial.begin(9600);
  //pripojenie pamatovej karty na zbernicu
  pinMode(SPIselect, OUTPUT);
  digitalWrite(SPIselect, HIGH);
  // nastavenie pociatocneho casu na cas a datum kompilacie programu
  // pre pripad ze RTC nefunguje
  DateTime now = DateTime(F(__DATE__), F(__TIME__));
  // overenie funkcnosti RTC
  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
  }
  // vycitanie casu
  else
    now = rtc.now();
  // overenie napajania RTC
  if (rtc.lostPower()) {
    Serial.println("RTC lost power, lets set the time!");
    // do modulu RTC nahra cas a datum kedy bol program skompilovany
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  // inicializacia pamatovej karty
  Serial.println("cakam na inicializaciu pametovej karty...");
  if (!SD.begin(chipSelect)) { // CS is D8 in this example
    Serial.println("inicializacia zlyhala!");
    return;
  }
  Serial.println("inicializacia hotova");
  // vyhladanie a pripisanie do, pripadne vytvorenie datoveho suboru
  Serial.println("hladam 'data.txt'");
  if (SD.exists("data.txt")) {
    Serial.println("subor najdeny");
    Serial.println("otvaram 'data.txt' za ucelom pripisania zaznamu");
    File testfile = SD.open("data.txt", FILE_WRITE);
    if (testfile) {
      Serial.println("pripisujem zaznam:");
      Serial.println("New session");
      testfile.println("New session");
      testfile.close();
      Serial.println("zatvaram subor");
    }
    else
      Serial.println("otvorenie zlyhalo!"); 
  }
  // ak neexistuje tak ho vytvori a zapise do neho cas a datum
  else {
    Serial.println("subor neexistuje");
    Serial.println("vytvaram 'data.txt'");
    File testfile = SD.open("data.txt", FILE_WRITE); 
    if (testfile) {
      sprintf(zaznam, "%02d.%02d.%4d", now.day(), now.month(), now.year());
      Serial.println("pripisujem zaznam:");
      Serial.println(zaznam);
      testfile.println(zaznam);
      testfile.close();
      Serial.print("zatvaram subor\n");   
    }
    else
      Serial.println("otvorenie zlyhalo!"); 
  }
  // pred odpojenim karty od zbernice treba zatvorit session
  // inak by nebolo mozne znova inicializovat kartu
  SD.end();
  // pripojenie displeja na zbernicu
  digitalWrite(SPIselect, LOW);
  // inicializacia plynoveho senzora
  SCD30WIRE.begin();
  airSensor.setDebug(scd_debug);
  //This will cause readings to occur every two seconds
  if (! airSensor.begin(SCD30WIRE)) {
    Serial.println(F("The SCD30 did not respond. Please check wiring."));
    while(1);
  }
  // inicializacia displeja
  Disp.start();
  Disp.setBrightness(5);
  Disp.setFont(Mono5x7);
}

uint16_t last_minute = 61;
uint32_t CO2 = 0;
uint32_t CO2acc = 0;
uint32_t CO2avg = 0;
uint32_t CO2max = 0;
uint32_t CO2min = 0xFFFF;
uint32_t CO2tab[10000];   // senzor meria ~ kazdych 6 sekund
uint32_t i = 0;
uint32_t j = 0;

void loop() {
  String CO2str = "    ";
  int32_t CO2temp;
  if (i >= 3 * DUR_VAL) {
    if (airSensor.dataAvailable()) {
      // vycitanie casu
      DateTime now = rtc.now();
      // ulozenie vycitanej hodnoty do RAM
      CO2 = airSensor.getCO2();
      // hoci senzor tesne po zapnuti  uz indikuje ze ma pripravene hodnoty
      // tak CO2 pri prvom vycitani ich pripravene nema
      if (CO2 > 0) {
        CO2avg = CO2acc += CO2tab[j] = CO2;
        CO2avg /= ++j;  // pouzitim i++ by pri spusteni nastalo delenie nulou
        if(CO2 > CO2max)
          CO2max = CO2;
        if(CO2 < CO2min)
          CO2min = CO2;
        // ak presla minuta, tak namerany udaj spolu s casom zapise na kartu
        if(last_minute != now.minute()) {
          last_minute = now.minute();
          // pripojenie karty na zbernicu
          digitalWrite(SPIselect, HIGH);
          // inicializacia pamatovej karty
          //Serial.println("cakam na inicializaciu pametovej karty...");
          // bez tohto riadku by vykonanie funkcie disp.loop() zmarilo inicializaciu
          if (!SD.begin(chipSelect)) {
            //Serial.println("inicializacia zlyhala!");
            return;
          }
          //Serial.println("inicializacia hotova");
          // ak odbila polnoc, tak najprv zapise novy datum
          if(now.hour() == 0 && now.minute() == 0) {
            //Serial.println("otavaram 'data.txt' za ucelom pripisania datumu");
            File testfile = SD.open("data.txt", FILE_WRITE);
            if (testfile) {
              sprintf(zaznam, "%02d.%02d.%4d", now.day(), now.month(), now.year());
              testfile.println(zaznam);
              testfile.close();
              //Serial.println("pripisujem zaznam:");
              //Serial.println(zaznam);
              //Serial.println("zatvaram subor");         
            }
            //else
            //  Serial.println("otvorenie zlyhalo!"); 
          }
          //Serial.println("otvaram 'data.txt' za ucelom pripisania CO2");
          File testfile = SD.open("data.txt", FILE_WRITE);
          if (testfile) {
            sprintf(zaznam, "%02d:%02d - %5d", now.hour(), now.minute(), CO2);
            testfile.println(zaznam);
            testfile.close();
            //Serial.println("pripisujem zaznam:");
            //Serial.println(zaznam);
            //Serial.println("zatvaram subor");
          }
          //else
          //  Serial.println("otvorenie zlyhalo!"); 
          // pred odpojenim karty od zbernice treba zatvorit session
          // inak by nebolo mozne znova inicializovat kartu
          SD.end();
          // odpojenie karty a pripojenie displeja na zbernicu SPI
          digitalWrite(SPIselect, LOW);
        }
      }
    }
    else
      Serial.println("senzor nema pripravene data!");
    i = 0;
  }
  // vypis indikatorov spodneho riadka - max / avg / min
  if (i < DUR_VAL) {
      CO2str = String(CO2temp = CO2max);
      Disp.drawText(1,9,"M ");  // medzera prepise pozostatok 4. cislice
  }
  else if(i < 2 * DUR_VAL) {
    CO2str = String(CO2temp = CO2avg);
    Disp.drawText(1,8,"a ");
    Disp.drawText(7,9," ");    // medzera prepise pozostatok 4. cislice
  }
  else if(i < 3 * DUR_VAL) {
    CO2str = String(CO2temp = CO2min);
    Disp.drawText(1,8,"m");
    Disp.drawText(7,9," ");    // medzera prepise pozostatok 4. cislice
  }
  // vypis aktualnej hodnoty CO2 aj s indikatormi prekrocenia
  if(CO2 < 800) {
    Disp.drawText(1,1,"  ");   // medzera prepise pozostatok 4. cislice
    Disp.drawText(14,1,String(CO2));
  }
  else if(CO2 < 1000) {
    Disp.drawText(1,1,"! ");   // medzera prepise pozostatok 4. cislice
    Disp.drawText(14,1,String(CO2));
  }
  else if(CO2 < 10000) {
    Disp.drawText(1,0,"x");
    Disp.drawText(8,1,String(CO2));
  }
  else if(CO2 >= 10000) {
    Disp.drawText(1,0,"x");
    Disp.drawText(8,1,"9999");
  }
  // vypis dlhodobej hodnoty CO2
  if(CO2temp < 1000) {
    Disp.drawText(14,9,CO2str);
  }
  else if(CO2temp < 10000)
    Disp.drawText(8,9,CO2str);
  else
    Disp.drawText(8,9,"9999");
  Disp.loop(); // display refresh
  i++;
}