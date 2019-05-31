#include <SD.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Keypad.h>
#include <Adafruit_Fingerprint.h>
#include <Wire.h>
#include <LiquidCrystal_PCF8574.h>
#include "RTClib.h"

LiquidCrystal_PCF8574 lcd(0x27);

RTC_DS3231 rtc;
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

#define RSTPIN 49
#define SSPIN 53
MFRC522 rc(SSPIN, RSTPIN);

const byte ROWS = 4;
const byte COLS = 4;
const int chipSelect = 43;

int readsuccess, x = 0, mode = 1;
int relayPin = 48;
int buttonPin = 46;
int ledPin = 45;
int redPin = 47;
int relayState = LOW;
int buttonState;
int buzzerPin = 44;

String password = "56610*";
String password2 = "1111*";
String pw = "";
String pwEnter = "";
String nameEnter = "";

char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

byte rowPins[ROWS] = {5, 4, 3, 2};
byte colPins[COLS] = {9, 8, 7, 6};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

byte defcard[][4] = {{0xBB, 0xA7, 0x2E, 0x01}, {0x96, 0x68, 0x47, 0xF4}, {0xD6, 0xBA, 0x45, 0xF4}}; //for multiple cards
int N = 3;                                                               //change this to the number of cards/tags you will use
byte readcard[4];

SoftwareSerial mySerial(10, 11);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

void setup()
{
  lcd.setBacklight(1);
  Serial.begin(9600);
  SPI.begin();
  rc.PCD_Init();                //initialize the receiver
  rc.PCD_DumpVersionToSerial(); //show details of card reader module

  finger.begin(57600);          //Initiate Fingerprint Scanner

  pinMode(relayPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT);
  pinMode(redPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);

  keypad.addEventListener(keypadEvent); //add an event listener for this keypad

  lcdStart();
  digitalWrite(redPin, HIGH);
  digitalWrite(relayPin, HIGH);
  digitalWrite(ledPin, LOW);

  ///////////////////////////////////////
  Serial.print("Initialize");
  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
    Serial.println("Nao ha cartao disponivel");
    // don't do anything more:
    return;
  }

  ///////////////////////////////////////
  Wire.begin();
  rtc.begin();
  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
    rtc.adjust(DateTime(2019, 5, 31, 15, 16, 0));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  }
}

void loop()
{
  keypad.getKey();
  rfidMode();
  getFingerprintID();

  if (!digitalRead(buttonPin)) {
    delay(190);
    open();
    lcdAuthorised();
    delay(3000);
    close();
    nameEnter = "Button Override";
    writeSD(nameEnter);
  }
}

// Taking care of some special events.
void keypadEvent(KeypadEvent key)
{

  if (x == 0) {
    switch (keypad.getState())
    {
      case PRESSED:
        x++;
        lcd.begin(16, 2);
        lcd.print("Enter Password");
        lcd.setCursor(0, 1);
        lcd.print("Press * to Enter");
        keypad.getKey();

        break;

      default:
        break;
    }
  }
  while (x >= 1 && x <= 6)
  {

    if (!digitalRead(buttonPin)) {
      delay(190);
      open();
      lcdAuthorised();
      delay(3000);
      close();
      x = 0;
      pw = "";
      nameEnter = "Button Override";
      writeSD(nameEnter);
    }

    switch (keypad.getState())
    {
      case PRESSED:
        x++;
        pw += "*";
        lcd.begin(16, 2);
        lcd.print("Enter Password");
        lcd.setCursor(0, 1);
        lcd.print(pw);
        Serial.println("@@@@@@@@");
        Serial.println(key);
        pwEnter += key;


        switch (key)
        {
          case '*':
            Serial.println(pwEnter);
            checkPassword();
            x = 0;
            break;

          default:
            //password.append(key);
            //password2.append(key);
            break;
        }

        break;

      default:
        break;
    }

    keypad.getKey();
    delay(100);
  }

  if (x == 7) {
    x = 0;

    lcdDenied();
    close();
    delay(2000);
    pw = "";
    pwEnter = "";

  }
}

void checkPassword()
{
  if (pwEnter == password || pwEnter == password2)
  {
    open();
    lcdAuthorised();
    delay(3000);
    close();
    pw = "";
    pwEnter = "";
    nameEnter = "Private PW";
    writeSD(nameEnter);
  }

  else
  {
    lcdDenied();
    buzzer();
    delay(2000);
    pw = "";
    pwEnter = "";
    nameEnter = "Wrong PW Entered";
    writeSD(nameEnter);
    lcdStart();
  }
}

void rfidMode()
{
  readsuccess = getid();

  if (readsuccess == 1)
  {
    int match = 0;
    int cardID;
    //this is the part where compare the current tag with pre defined tags
    for (int i = 0; i < N; i++)
    {
      Serial.print("Testing Against Authorised card no: ");
      Serial.println(i + 1);
      if (!memcmp(readcard, defcard[i], 4))
      {
        match++;
        cardID = i;
      }
    }

    if (match) //match ==1
    {
      if (cardID == 0) {
        Serial.println("This is a Kon10");
        Serial.println("CARD AUTHORISED");
        open();
        lcdAuthorised();
        delay(3000);
        Serial.println("Turn Off the Lock");
        readsuccess = 0;
        nameEnter = "KON10";
        writeSD(nameEnter);
        close();
      }
      else if (cardID == 1) {
        Serial.println("This is a Printcess");
        Serial.println("CARD AUTHORISED");
        open();
        lcdAuthorised();
        delay(3000);
        Serial.println("Turn Off the Lock");
        readsuccess = 0;
        nameEnter = "Printcess";
        writeSD(nameEnter);
        close();

      }
      else if (cardID == 2) {
        Serial.println("This is a FingerprintCard");
      }
    }
    else //match ==0
    {
      Serial.println("CARD NOT Authorised");
      lcdDenied();
      buzzer();
      nameEnter = "Unauthorised CARD";
      writeSD(nameEnter);
      close();
      delay(2000);
    }
  }
  else
  {
    delay(100);
  }
}

//function to get the UID of the card
int getid()
{
  rc.PCD_Init();
  if (!rc.PICC_IsNewCardPresent())
  {
    return 0;
  }
  if (!rc.PICC_ReadCardSerial())
  {
    return 0;
  }

  Serial.println("THE UID OF THE SCANNED CARD IS:");

  for (int i = 0; i < 4; i++)
  {
    readcard[i] = rc.uid.uidByte[i]; //storing the UID of the tag in readcard
    Serial.print(readcard[i], HEX);
  }
  Serial.println("");
  Serial.println("Now Comparing with Authorised cards");
  rc.PICC_HaltA();

  return 1;
}

uint8_t getFingerprintID() {
  uint8_t p = finger.getImage();
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      break;

    default:
      Serial.println("Unknown error");
      return p;
  }

  // OK success!

  p = finger.image2Tz();
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");

      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  // OK converted!
  p = finger.fingerFastSearch();
  if (p == FINGERPRINT_OK) {
    Serial.println("Found a print match!");
  }
  else if (p == FINGERPRINT_NOTFOUND) {
    Serial.println("Did not find a match");
    return p;
  }
  else {
    Serial.println("Unknown error");

    return p;
  }

  // found a match!
  Serial.print("Found ID #"); Serial.print(finger.fingerID);
  Serial.print(" with confidence of "); Serial.println(finger.confidence);

  switch (finger.fingerID) {
    case 1:
      lcd.begin(16, 2);
      lcd.print("Welcome Andy");
      nameEnter = "Andy";
      writeSD(nameEnter);
      break;
    case 2:
      lcd.begin(16, 2);
      lcd.print("Welcome ChiaLing");
      nameEnter = "ChiaLing";
      writeSD(nameEnter);
      break;
    case 3:
      lcd.begin(16, 2);
      lcd.print("Welcome Boonie");
      nameEnter = "Boonie";
      writeSD(nameEnter);
      break;
    case 4:
      lcd.begin(16, 2);
      lcd.print("Welcome Li En");
      nameEnter = "Li En";
      writeSD(nameEnter);
      break;
    case 5:
      lcd.begin(16, 2);
      lcd.print("Welcome Kevin");
      nameEnter = "Kevin";
      writeSD(nameEnter);
      break;
    case 6:
      lcd.begin(16, 2);
      lcd.print("Welcome Jeff");
      nameEnter = "Jeff";
      writeSD(nameEnter);
      break;
    default:
      break;
  }
  open();
  delay(3000);
  close();
  return finger.fingerID;
}

void lcdStart()
{
  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  lcd.print("   Welcome to");
  lcd.setCursor(0, 1);
  //Print a message to second line of LCD
  lcd.print("    Incubator");
}

void lcdAuthorised()
{
  lcd.begin(16, 2);
  lcd.print("Access Granted");
}

void lcdDenied()
{
  lcd.begin(16, 2);
  lcd.print("Access Denied");
}

void open() {
  digitalWrite(relayPin, LOW);
  digitalWrite(redPin, LOW);
  digitalWrite(ledPin, HIGH);
  buzzer();
}
void close() {
  digitalWrite(redPin, HIGH);
  digitalWrite(relayPin, HIGH);
  digitalWrite(ledPin, LOW);
  buzzer();
  lcdStart();
}

void buzzer() {
  digitalWrite(buzzerPin, HIGH);
  delay(50);
  digitalWrite(buzzerPin, LOW);
  delay(50);
  digitalWrite(buzzerPin, HIGH);
  delay(50);
  digitalWrite(buzzerPin, LOW);
}

void writeSD(String idName) {
  File dataFile = SD.open("Log.txt", FILE_WRITE);
  Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
  DateTime now = rtc.now();
  if (dataFile) {
    dataFile.print(idName);
    dataFile.print(" ");
    dataFile.print(now.day(), DEC);
    dataFile.print('/');
    dataFile.print(now.month(), DEC);
    dataFile.print('/');
    dataFile.print(now.year(), DEC);
    dataFile.print(' ');
    dataFile.print(daysOfTheWeek[now.dayOfTheWeek()]);
    dataFile.print(' ');
    dataFile.print(now.hour(), DEC);
    dataFile.print(':');
    dataFile.print(now.minute(), DEC);
    dataFile.print(':');
    dataFile.print(now.second(), DEC);
    dataFile.println();
  }
  dataFile.close();
}