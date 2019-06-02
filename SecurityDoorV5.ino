#include <SD.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Keypad.h>
#include <Adafruit_Fingerprint.h>
#include <Wire.h>
#include "RTClib.h"
#include "Display.h"

RTC_DS3231 rtc;
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

#define RSTPIN 49
#define SSPIN 53
MFRC522 rc(SSPIN, RSTPIN);

const byte ROWS = 4;
const byte COLS = 4;
const int chipSelect = 43;

int readsuccess, x = 0, mode = 1;
int fingerPrintMode = 0;
int buttonPin = 46;

int id;
String sFingerID = "";
int fingerPrintX = 0;

String password = "56610*";
String password2 = "1111*";
String pw = "";
String pwEnter = "";
String nameEnter = "";

char keys[ROWS][COLS] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}};

byte rowPins[ROWS] = {5, 4, 3, 2};
byte colPins[COLS] = {9, 8, 7, 6};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);
Display display = Display(48, 45, 47, 44);

byte defcard[][4] = {{0xBB, 0xA7, 0x2E, 0x01}, {0x96, 0x68, 0x47, 0xF4}, {0xD6, 0xBA, 0x45, 0xF4}}; //for multiple cards
int N = 3;                                                                                          //change this to the number of cards/tags you will use
byte readcard[4];

SoftwareSerial mySerial(10, 11);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

void setup()
{
  
    Serial.begin(9600);
    Serial.println("@@@@@@@@@@@@@@@@@@@@@1");
    SPI.begin();
    rc.PCD_Init();                //initialize the receiver
    Serial.println("@@@@@@@@@@@@@@@@@@@@@2");
    rc.PCD_DumpVersionToSerial(); //show details of card reader module

    Serial.println("@@@@@@@@@@@@@@@@@@@@@");
    
    finger.begin(57600); //Initiate Fingerprint Scanner

    pinMode(relayPin, OUTPUT);
    pinMode(buttonPin, INPUT_PULLUP);
    pinMode(ledPin, OUTPUT);
    pinMode(redPin, OUTPUT);
    pinMode(buzzerPin, OUTPUT);

    keypad.addEventListener(keypadEvent); //add an event listener for this keypad

    display.lcdStart();
    digitalWrite(redPin, HIGH);
    digitalWrite(relayPin, HIGH);
    digitalWrite(ledPin, LOW);

    ///////////////////////////////////////
    Serial.print("Initialize");
    // see if the card is present and can be initialized:
    if (!SD.begin(chipSelect))
    {
        Serial.println("Nao ha cartao disponivel");
        // don't do anything more:
        return;
    }

    ///////////////////////////////////////
    Wire.begin();
    rtc.begin();
    if (!rtc.begin())
    {
        Serial.println("Couldn't find RTC");
        while (1)
            ;
        rtc.adjust(DateTime(2019, 5, 31, 15, 16, 0));
        // This line sets the RTC with an explicit date & time, for example to set
        // January 21, 2014 at 3am you would call:
        // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
    }
}

void loop()
{
    char printKey = keypad.getKey();
    rfidMode();
    getFingerprintID();
    if (!digitalRead(buttonPin))
    {
        delay(190);
        display.open();
        display.lcdAuthorised();
        delay(3000);
        display.close();
        nameEnter = "Button Override";
        writeSD(nameEnter);
    }

    Serial.println(printKey);
}

// Taking care of some special events.
void keypadEvent(KeypadEvent key)
{
    if (fingerPrintMode == 1)
    {
        Serial.println("Entered fingerprint mode");

        while (fingerPrintX >= 0 && fingerPrintX <= 5)
        {
            Serial.println("Entered fingerprint modemodemodemode");
            Serial.println(fingerPrintX);
            switch (keypad.getState())
            {
            case PRESSED:
                fingerPrintX++;
                sFingerID += key;
                display.fpNewID(sFingerID);

                Serial.print("Key Entered: ");
                Serial.println(key);
                Serial.print("FingerprintX: ");
                Serial.println(fingerPrintX);
                Serial.print("String finger ID: ");
                Serial.println(sFingerID);

                switch (key)
                {
                case '*':
                    Serial.println(sFingerID);
                    sFingerID = sFingerID.substring(0, sFingerID.length() - 1);
                    id = sFingerID.toInt();
                    Serial.println(id);
                    //delay(5000);
                    if (id >= 1 && id <= 127)
                    {
                        Serial.println("INSIDE 1 TO 127 LOOP");

                        while (!getFingerprintEnroll())
                            ;

                        fingerPrintMode = 0;
                        fingerPrintX = 10;
                        sFingerID = "";
                        display.close();
                    }
                    else
                    {
                        display.lcdInvalidID();
                        fingerPrintMode = 0;
                        fingerPrintX = 10;
                        sFingerID = "";
                    }

                    break;

                default:
                    break;
                }

                break;

            default:
                break;
            }

            keypad.getKey();
            delay(100);
        }
    }
    if (x == 0)
    {
        switch (keypad.getState())
        {
        case PRESSED:
            x++;
            display.lcdPassword();
            keypad.getKey();

            break;

        default:
            break;
        }
    }
    while (x >= 1 && x <= 6)
    {

        if (!digitalRead(buttonPin))
        {
            delay(190);
            display.open();
            display.lcdAuthorised();
            delay(3000);
            display.close();
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
            display.printPassword(pw);
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

                break;
            }

            break;

        default:
            break;
        }

        keypad.getKey();
        delay(100);
    }

    if (x == 7)
    {
        x = 0;

        display.lcdDenied();
        display.close();
        delay(2000);
        pw = "";
        pwEnter = "";
    }
}

void checkPassword()
{
    if (pwEnter == password || pwEnter == password2)
    {
        display.open();
        display.lcdAuthorised();
        delay(3000);
        display.close();
        pw = "";
        pwEnter = "";
        nameEnter = "Private PW";
        writeSD(nameEnter);
    }

    else
    {
        display.lcdDenied();
        display.buzzer();
        delay(2000);
        pw = "";
        pwEnter = "";
        nameEnter = "Wrong PW Entered";
        writeSD(nameEnter);
        display.lcdStart();
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
            if (cardID == 0)
            {
                Serial.println("This is a Kon10");
                Serial.println("CARD AUTHORISED");
                display.open();
                display.lcdAuthorised();
                delay(3000);
                Serial.println("Turn Off the Lock");
                readsuccess = 0;
                nameEnter = "KON10";
                writeSD(nameEnter);
                display.close();
            }
            else if (cardID == 1)
            {
                Serial.println("This is a Printcess");
                Serial.println("CARD AUTHORISED");
                display.open();
                display.lcdAuthorised();
                delay(3000);
                Serial.println("Turn Off the Lock");
                readsuccess = 0;
                nameEnter = "Printcess";
                writeSD(nameEnter);
                display.close();
            }
            else if (cardID == 2)
            {
                Serial.println("This is a FingerprintCard");
                fingerPrintMode = 1;
                fingerPrintX = 0;
                display.fpNewID(sFingerID);
                keypadEvent(fingerPrintMode);
            }
        }
        else //match ==0
        {
            Serial.println("CARD NOT Authorised");
            display.lcdDenied();
            display.buzzer();
            nameEnter = "Unauthorised CARD";
            writeSD(nameEnter);
            display.close();
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

uint8_t getFingerprintID()
{
    uint8_t p = finger.getImage();
    Serial.println(p);
    switch (p)
    {
    case FINGERPRINT_OK:
        Serial.println("Image taken");
        break;

    default:
        Serial.println("Unknown error");

        return p;
    }

    // OK success!

    p = finger.image2Tz();
    Serial.println(p);
    switch (p)
    {
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
    Serial.println(p);
    if (p == FINGERPRINT_OK)
    {
        Serial.println("Found a print match!");
    }
    else if (p == FINGERPRINT_NOTFOUND)
    {
        Serial.println("Did not find a match");
        return p;
    }
    else
    {
        Serial.println("Unknown error");

        return p;
    }

    // found a match!
    Serial.print("Found ID #");
    Serial.print(finger.fingerID);
    Serial.print(" with confidence of ");
    Serial.println(finger.confidence);
    int fingerID = finger.fingerID;
    switch (finger.fingerID)
    {
    case 1:
        display.lcdFpAccess(fingerID);
        nameEnter = "Andy";
        writeSD(nameEnter);
        break;
    case 2:
        display.lcdFpAccess(fingerID);
        nameEnter = "ChiaLing";
        writeSD(nameEnter);
        break;
    case 3:
        display.lcdFpAccess(fingerID);
        nameEnter = "Boonie";
        writeSD(nameEnter);
        break;
    case 4:
        display.lcdFpAccess(fingerID);
        nameEnter = "Li En";
        writeSD(nameEnter);
        break;
    case 5:
        display.lcdFpAccess(fingerID);
        nameEnter = "Kevin";
        writeSD(nameEnter);
        break;
    case 6:
        display.lcdFpAccess(fingerID);
        nameEnter = "Jeff";
        writeSD(nameEnter);
        break;
    default:
        break;
    }
    display.open();
    delay(3000);
    display.close();
    return finger.fingerID;
}

void writeSD(String idName)
{
    File dataFile = SD.open("Log.txt", FILE_WRITE);
    Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
    DateTime now = rtc.now();
    if (dataFile)
    {
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
uint8_t getFingerprintEnroll()
{

    int p = -1;
    Serial.print("Waiting for valid finger to enroll as #");
    Serial.println(id);
    while (p != FINGERPRINT_OK)
    {
        p = finger.getImage();
        switch (p)
        {
        case FINGERPRINT_OK:
            Serial.println("Image taken");
            break;
        case FINGERPRINT_NOFINGER:
            Serial.println(".");
            Serial.println("@@@@@@@@@@@@@@@@@@@@@@@@@@!@!@!@@!");
            if (!digitalRead(buttonPin))
            {
                delay(190);
                display.close();
                nameEnter = "Quit Fingerprint Session";
                writeSD(nameEnter);
                return -1;
            }
            break;
        case FINGERPRINT_PACKETRECIEVEERR:
            Serial.println("Communication error");
            break;
        case FINGERPRINT_IMAGEFAIL:
            Serial.println("Imaging error");
            break;
        default:
            Serial.println("Unknown error");
            break;
        }
    }

    // OK success!

    p = finger.image2Tz(1);
    switch (p)
    {
    case FINGERPRINT_OK:
        Serial.println("Image converted");
        break;
    case FINGERPRINT_IMAGEMESS:
        Serial.println("Image too messy");
        return p;
    case FINGERPRINT_PACKETRECIEVEERR:
        Serial.println("Communication error");
        return p;
    case FINGERPRINT_FEATUREFAIL:
        Serial.println("Could not find fingerprint features");
        return p;
    case FINGERPRINT_INVALIDIMAGE:
        Serial.println("Could not find fingerprint features");
        return p;
    default:
        Serial.println("Unknown error");
        return p;
    }

    Serial.println("Remove finger");
    delay(2000);
    p = 0;
    while (p != FINGERPRINT_NOFINGER)
    {
        p = finger.getImage();
    }
    Serial.print("ID ");
    Serial.println(id);
    p = -1;
    Serial.println("Place same finger again");
    while (p != FINGERPRINT_OK)
    {
        p = finger.getImage();
        switch (p)
        {
        case FINGERPRINT_OK:
            Serial.println("Image taken");
            break;
        case FINGERPRINT_NOFINGER:
            Serial.print(".");
            break;
        case FINGERPRINT_PACKETRECIEVEERR:
            Serial.println("Communication error");
            break;
        case FINGERPRINT_IMAGEFAIL:
            Serial.println("Imaging error");
            break;
        default:
            Serial.println("Unknown error");
            break;
        }
    }

    // OK success!

    p = finger.image2Tz(2);
    switch (p)
    {
    case FINGERPRINT_OK:
        Serial.println("Image converted");
        break;
    case FINGERPRINT_IMAGEMESS:
        Serial.println("Image too messy");
        return p;
    case FINGERPRINT_PACKETRECIEVEERR:
        Serial.println("Communication error");
        return p;
    case FINGERPRINT_FEATUREFAIL:
        Serial.println("Could not find fingerprint features");
        return p;
    case FINGERPRINT_INVALIDIMAGE:
        Serial.println("Could not find fingerprint features");
        return p;
    default:
        Serial.println("Unknown error");
        return p;
    }

    // OK converted!
    Serial.print("Creating model for #");
    Serial.println(id);

    p = finger.createModel();
    if (p == FINGERPRINT_OK)
    {
        Serial.println("Prints matched!");
    }
    else if (p == FINGERPRINT_PACKETRECIEVEERR)
    {
        Serial.println("Communication error");
        return p;
    }
    else if (p == FINGERPRINT_ENROLLMISMATCH)
    {
        Serial.println("Fingerprints did not match");
        return p;
    }
    else
    {
        Serial.println("Unknown error");
        return p;
    }

    Serial.print("ID ");
    Serial.println(id);
    p = finger.storeModel(id);
    if (p == FINGERPRINT_OK)
    {
        Serial.println("Stored!");
        nameEnter = "Fingerprint Stored ";
        Serial.println(nameEnter);
        nameEnter += id;
        Serial.println(nameEnter);

        writeSD(nameEnter);
        display.lcdFpStored();
        delay(2000);
        return -1;
    }
    else if (p == FINGERPRINT_PACKETRECIEVEERR)
    {
        Serial.println("Communication error");
        return p;
    }
    else if (p == FINGERPRINT_BADLOCATION)
    {
        Serial.println("Could not store in that location");
        return p;
    }
    else if (p == FINGERPRINT_FLASHERR)
    {
        Serial.println("Error writing to flash");
        return p;
    }
    else
    {
        Serial.println("Unknown error");
        return p;
    }
}
