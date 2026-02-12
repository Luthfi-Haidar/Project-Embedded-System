#include <Adafruit_GFX.h> // Downloaded dependency, library "Adafruit GFX Library"
#include <Adafruit_SH110X.h> // Downloaded dependency, library "Adafruit SH110X"
#include <Arduino.h>
#include <MFRC522DriverPinSimple.h> // part of MFRC522v2
#include <MFRC522DriverSPI.h>       // part of MFRC522v2
#include <MFRC522v2.h> // Downloaded dependency, "library MFRC522v2"
#include <SPI.h>
#include <WiFi.h>

// Pin configuration
// Refer to README.md for what each does and what to change
#define SS_PIN 5
#define RX_PIN 16
#define TX_PIN 17
#define SDA_PIN 21
#define SCL_PIN 22
#define LBUTTON_PIN 26
#define RBUTTON_PIN 27

// RC522
MFRC522DriverPinSimple ss_pin(SS_PIN);
MFRC522DriverSPI driver{ss_pin};
MFRC522 mfrc522{driver};

// I2C
Adafruit_SH1106G display(128, 64, &Wire, -1);

// Wifi configuration
// TODO: (only if this project goes beyond prototype) consider adding a way to
//       reconfigure Wi-Fi without re-flashing. Potentially using QR code
const char *ssid = "Halo";
const char *password = "IThinkYoureOnTheWrongHouseMate";

// States/Steps
enum States {
    IDLE,         // System is not used, scan card to start
    VERIFY_CARD,  // System checks if card is valid
    SCAN_BOOK,    // User scans book
    VERIFY_BOOK,  // Check if book is valid, also infer if this is borrowing,
                  // returning, or borrowed by someone else
    CONFIRMATION, // Confirm if they actually want to borrow/return
    DONE,         // Success
    CANCELLED,    // Transaction cancelled
    ERROR         // Something went wrong
};

enum Errors {
    OTHERS,        // Unspecified Error
    DEVICE_ERROR,  // Not implemented yet, used when self test failed
    CARD_INVALID,  // Card invalid
    BOOK_INVALID,  // Book invalid
    BOOK_BORROWED, // Book already borrowed
    NETWORK_ERROR, // I mean, it's in the name...
};

States systemState = IDLE;
States screenState = IDLE;
Errors sysError = OTHERS;
bool borrowing = true; // Set by system, false -> returning

String cardUID;
String bookCode;

void changeScreen(States newScreenState) {
    display.clearDisplay();
    display.setCursor(0, 0);

    switch (newScreenState) {
    case IDLE:
        // TODO: Center text
        display.println("Please scan your");
        display.println("member card to start");
        break;

    case VERIFY_CARD:
        display.println("Checking card...");
        break;

    case SCAN_BOOK:
        display.println("Please scan the book");
        display.println("you want to borrow");
        display.println("or return");
        break;

    case VERIFY_BOOK:
        display.println("Checking book status...");
        break;

    case CONFIRMATION:
        display.println("Are you sure you want");
        if (borrowing) {
            display.println("to borrow this book?");
        } else {
            display.println("to return this book?");
        }
        break;

    case DONE:
        if (borrowing) {
            display.println("Book borrowed");
        } else {
            display.println("Book returned");
        }
        break;

    case CANCELLED:
        display.println("Cancelled.");
        display.println("Scan card again to restart.");
        break;

        // TODO: Error screen

    default:
        break;
    }

    display.display();
    screenState = newScreenState;
}

bool checkForCard() {
    // Reset the loop if no new card present on the sensor/reader. This saves
    // the entire process when idle.
    if (!mfrc522.PICC_IsNewCardPresent()) {
        return false;
    }

    // Select one of the cards.
    if (!mfrc522.PICC_ReadCardSerial()) {
        return false;
    }

    Serial.print("UID tag :");
    String converted = "";
    for (byte i = 0; i < mfrc522.uid.size; i++) {
        converted.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " "));
        converted.concat(String(mfrc522.uid.uidByte[i], HEX));
    }
    Serial.println(converted);

    cardUID = converted;

    mfrc522.PICC_HaltA();

    // Stop encryption on PCD
    mfrc522.PCD_StopCrypto1();

    return true;
}

bool checkForBook() {
    if (Serial2.available()) {
        String code = Serial2.readStringUntil('\n');
        // process barcode
        bookCode = code;
        return true;
    }
    return false;
}

void setup() {
    pinMode(LBUTTON_PIN, INPUT_PULLUP);
    pinMode(RBUTTON_PIN, INPUT_PULLUP);

    Serial.begin(115200); // Initialize Serial for debug console

    Serial2.begin(9600, SERIAL_8N1, RX_PIN,
                  TX_PIN); // Initialize GM65 Serial

    WiFi.begin(ssid, password);
    Serial.print("Connecting");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    mfrc522.PCD_Init(); // Initialize PCD for RC522

    // I2C Init
    Wire.begin(SDA_PIN, SCL_PIN);

    display.begin(0x3C);
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);

    changeScreen(IDLE);

    // TODO: Self test
}

void loop() {
    if (systemState != screenState) {
        changeScreen(systemState);
    }

    switch (systemState) {
    case IDLE:
        if (checkForCard()) {
            systemState = VERIFY_CARD;
        }
        break;

    case VERIFY_CARD:
        // TODO: Implement card verification
        delay(5000);
        systemState = SCAN_BOOK;
        break;

    case SCAN_BOOK:
        if (checkForBook()) {
            systemState = VERIFY_BOOK;
        }
        break;

    case VERIFY_BOOK:
        // TODO: Implement book verification
        delay(5000);
        borrowing = true;
        systemState = CONFIRMATION;
        break;

    case CONFIRMATION:
        // TODO: implement storing to database
        if (digitalRead(RBUTTON_PIN) == LOW) {
            systemState = DONE;
        } else if (digitalRead(LBUTTON_PIN) == LOW) {
            systemState = CANCELLED;
        }
        break;

    case DONE:
    case CANCELLED:
        Serial.println(cardUID);
        Serial.println(bookCode);
        delay(5000);
        systemState = IDLE;
        break;

    default:
        break;
    }
}
