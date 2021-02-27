/*
 *  A Quick test to see how well millis() keeps time.
 */


/*
 * Usable Beetle BLE pins are
 * A0 FLASH CS
 * A1 TFT DC
 * A2 TFT RST
 * A3 TFT CS
 * 2  BT RX
 * 3  TFT BL (GND for off or 3.3 for on)
 * 4  BT TX
 * 5
 * 
 */

#define BT_RX    2
#define TFT_BL   3
#define BT_TX    4
 #define TFT_3V   5
#define FLASH_CS A0
#define TFT_DC   A1
#define TFT_RST  A2 
#define TFT_CS   A3
#define BT_STATE A4
#define SCR_WD   240
#define SCR_HT   240

const uint16_t PROGMEM brain_pal[] = {
13276,13275,15419,19514,23867,27966,23869,17370,32091,34140,34139,32092,27866,13372,17372,21692,27964,25820,32188,34236,34235,27963,32187,28091,32089,27961,32185,30006,27797,19510,21554,19345,19278,21550,27730,36022,36214,34137,21169,34233,15414,27570,27606,13169,36057,19414,15254,15411,12941,12906,32050,38233,8617,4457,4422,38328,19117,13102,13097,13127,27609,10918,21353,27530,29774,36176,36179,29770,35945,27533,21547,35982,48494,46642,52689,46481,36174,15060,32047,38225,36275,36273,54866,60849,38323,35878,63154,19082,38226,36006,37922,35714,38321,54966,63189,35876,46146,48450,63350,46710,52694,35954,54594,62785,4650,6733,12945,39972,29541,31782,62625,65401,57177,57143,29505,21282,27265,54369,44405,39974,63193,54937,6736,62789,37742,35765,44182,52696,19077,62629,35761,45921,52129,21121,8621,14881,21317,35717,20865,12577,40502,54373,46149,45957,30014,29285,60326,37441,60353,54181,51809,43649,29025,48698,44209,62793,43653,65394,56941,46378,60617,37216,63311,54633,56906,56677,48420,35204,29060,45408,54409,54413,60620,53569,43913,52138,62829,37024,26753,37706,54638,61070,45185,59681,46121,53377,59522,59492,28837,45381,53348,60037,58955,53861,51557,53865,60109,59978,59718,62344,53641,54195,62710,60530,27277,27273,47432,54156,62937,62870,27345,46189,56402,60334,35501,37477,53932,37585,45156,59970,43725,41674,37545,45932,49257,34950,18789,43080,34921,18593,18564,20873,46003,35213,30856,44217,45973,41100,39117,37227,29066,8257,45717,50068,41421,50228,10694,63325,61180,46425,
};

#define TIME_MSG_LEN  11   // time sync to PC is HEADER followed by unix time_t as ten ascii digits
const char *wDays[]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
const char *wDaysFull[]={"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Arduino_ST7789_Fast.h>
#include <SoftwareSerial.h>
SoftwareSerial BTserial(BT_RX, BT_TX); // RX | TX
#include<SPIMemory.h>
uint32_t arrayAddr;

// init flash
SPIFlash flash(FLASH_CS);
bool readSerialStr(String &inputStr);
String outputString = "";

int dx,dy,dp;
uint16_t lineBuffer[240];
uint8_t lineBuffer8[240];

long int now;
int myTime[6]={0,0,0,0,0,0};

Arduino_ST7789 lcd = Arduino_ST7789(TFT_DC, TFT_RST, TFT_CS);

// Interrupt is called once a millisecond, 
SIGNAL(TIMER0_COMPA_vect) 
{
  if(millis() >= now+1000){
    if(++myTime[2]==60){
      myTime[2]=0;
      if(++myTime[1]==60){
        myTime[1]=0;
        if(++myTime[0]==24){
          myTime[0]=0;
        }
      }
    }
    now=millis();
  }
}

void setup(void) 
{

  Serial.begin(9600);
  BTserial.begin(9600);        // The Bluetooth Mate defaults to 115200bps
  while(!BTserial){;}
  Serial.println("BT Connected");

  pinMode(TFT_CS, OUTPUT);
  pinMode(BT_STATE,INPUT);

  // screen
  digitalWrite(TFT_BL, true);
  digitalWrite(TFT_3V, true);
  lcd.init(SCR_WD, SCR_HT);
  lcd.fillScreen(BLACK);
  lcd.setCursor(0, 0);
  lcd.setTextColor(WHITE,BLACK);
  lcd.setTextSize(4);
  //lcd.println("HELLO WORLD");
  now=millis(); // read RTC initial value  
  
  // set up interrupt request at same speed as millis()
  OCR0A = 0xAF;
  TIMSK0 |= _BV(OCIE0A);

  SPI.begin();
  SPI.setDataMode(0);
  SPI.setBitOrder(MSBFIRST);

  flash.begin();

  arrayAddr = 0;//random(0, flash.getCapacity());


}

int incomingByte;      // a variable to read incoming serial data into
char incoming[32];

int getImage=0;
int tempNumber=0;

void loop()
{

  int availableBytes = BTserial.available();

  if(availableBytes>1){
    for(int i=0; i<availableBytes; i++){
      incoming[i] = BTserial.read();
      incoming[i+1] = '\0'; // Append a null
      // 'Header character' = T for Time
      if(incoming[0]=='T'){
        Serial.println("Found T");
        getTime(&incoming[1]);
      }
      // 'Header character' = D for Date
      if(incoming[0]=='D'){
        Serial.println("Found D");
        getDate(&incoming[1]);
      }
      // 'Header character' = I for Image
      if(incoming[0]=='I'){
        Serial.println("Found I");
        getImage=1;
      }

    }
  }

  while(BTserial.available() > 0 || getImage==1){
    int currentByte=BTserial.read();
    int addTo = 0;
    switch(currentByte){
      case '0':
        addTo = 0;
        tempNumber *=10;
        tempNumber +=addTo;
      break;
      case '1':
        addTo = 1;
        tempNumber *=10;
        tempNumber +=addTo;
      break;
      case '2':
        addTo = 2;
        tempNumber *=10;
        tempNumber +=addTo;
      break;
      case '3':
        addTo = 3;
        tempNumber *=10;
        tempNumber +=addTo;
      break;
      case '4':
        addTo = 4;
        tempNumber *=10;
        tempNumber +=addTo;
      break;
      case '5':
        addTo = 5;
        tempNumber *=10;
        tempNumber +=addTo;
      break;
      case '6':
        addTo = 6;
        tempNumber *=10;
        tempNumber +=addTo;
      break;
      case '7':
        addTo = 7;
        tempNumber *=10;
        tempNumber +=addTo;
      break;
      case '8':
        addTo = 8;
        tempNumber *=10;
        tempNumber +=addTo;
      break;
      case '9':
        addTo = 9;
        tempNumber *=10;
        tempNumber +=addTo;
      break;
      case ',':
        lineBuffer[dx] = pgm_read_word(&brain_pal[tempNumber]);
        lineBuffer8[dx] = tempNumber;
        //lcd.drawPixel(dx, dy, brain_pal[tempNumber]);
        if(++dx==240){
          dx=0;
          if(dy==0){
            Serial.println("Erasing 64k for image data");
            if(flash.eraseBlock64K(0)){ // erase first 64k, full screen image takes 56.25k
              Serial.println("Erasing complete");
            }else{
              Serial.println("Erasing failed");
            }
          }
          if (flash.writeByteArray(arrayAddr, lineBuffer8, 240)) {  //240 is the size of the array
            arrayAddr+=240;
            Serial.print(dy);
            Serial.println(" - Data successfully written to chip");
          }else{
            Serial.println("Data not written to chip");
          }
          lcd.drawImage(0,dy,240,1,lineBuffer);
          if(++dy==240){
            getImage=0;
          }
        }
        tempNumber =0;
      break;
    }

  }


  updateScreen();
}




bool getTime(const char *str){
  int Hour, Min, Sec;
  if (sscanf(str, "%d:%d:%d", &Hour, &Min, &Sec) != 3) return false;
  myTime[0] = Hour;
  myTime[1] = Min;
  myTime[2] = Sec;
  return true;
}
bool getDate(const char *str){
  int Year, Month, Day;
  if (sscanf(str, "%d:%d:%d", &Year, &Month, &Day) != 3) return false;
  myTime[3] = Year;
  myTime[4] = Month;
  myTime[5] = Day;
  return true;
}

#define LEAP_YEAR(Y)     ( (Y>0) && !(Y%4) && ( (Y%100) || !(Y%400) ))     // from time-lib
int dayOfWeek(uint16_t year, uint8_t month, uint8_t day)
{
  uint16_t months[] = {
    0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365         };   // days until 1st of month

  uint32_t days = year * 365;        // days until year
  for (uint16_t i = 4; i < year; i += 4) if (LEAP_YEAR(i) ) days++;     // adjust leap years, test only multiple of 4 of course

  days += months[month-1] + day;    // add the days of this year
  if ((month > 2) && LEAP_YEAR(year)) days++;  // adjust 1 if this year is a leap year, but only after febr

  return days % 7;   // remove all multiples of 7
}


void updateScreen(){

  char tempText[10];
  sprintf(tempText,"%02d:%02d:%02d ",myTime[0],myTime[1],myTime[2]);
  lcd.setCursor(20, 110);
  lcd.setTextColor(WHITE,BLACK);
  lcd.setTextSize(4);
  lcd.print(tempText);
  sprintf(tempText,"%02d:%02d:%04d ",myTime[5],myTime[4],myTime[3]);
  lcd.setCursor(20, 110+32);
  lcd.setTextColor(YELLOW,BLACK);
  lcd.setTextSize(3);
  lcd.print(tempText);

  lcd.setCursor(20, 110+32+24);
  lcd.setTextColor(GREEN,BLACK);
  lcd.setTextSize(3);
  sprintf(tempText,"%s   ",wDaysFull[dayOfWeek(myTime[3],myTime[4],myTime[5])]);
  lcd.print(tempText);

  //sprintf(tempText,"Button:%02d  ",butt);
  lcd.setTextSize(2);
  lcd.setCursor(0, 240-24);
  lcd.print(outputString);

  if(digitalRead(BT_STATE)){
    lcd.setTextSize(2);
    lcd.setCursor(0, 240-24);
    lcd.print("BT Connected");
  }else{
    lcd.setTextSize(2);
    lcd.setCursor(0, 240-24);
    lcd.print("BT NOT Connected");
  }
}
