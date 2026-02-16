// Hardware
#include <Adafruit_GFX.h> // Downloaded dependency, library "Adafruit GFX Library"
#include <Adafruit_SH110X.h> // Downloaded dependency, library "Adafruit SH110X"
#include <Arduino.h>
#include <MFRC522DriverPinSimple.h> // part of MFRC522v2
#include <MFRC522DriverSPI.h>       // part of MFRC522v2
#include <MFRC522v2.h> // Downloaded dependency, "library MFRC522v2"
#include <SPI.h>

// Connectivity
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// JSON parser
#include <ArduinoJson.h>

// Custom scheduler I built
// (out of boredom, please don't judge)
#include "scheduler.h"

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
// Wifi configuration moved to system env
// set using
// $env:WIFI_SSID = ""
// do the same with WIFI_PASS

// Scheduler init
Scheduler scheduler;
// Note on statesID:
// Please be consistent on which schedule ID is use.
/*  Current Schedule ID assignment:
    0: Timeout Scheduler
    1:
    2:
    3:
    4:
    5: Reset to idle (any state)
    6:
    7:
    8:
    9:
    10:
    11:
    12:
    13:
    14: Debug helper
*/

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
    OTHERS,         // Unspecified Error
    DATABASE_ERROR, // Error communicating with the database
    INVALID_CARD,   // Card is not found in database
    INVALID_BOOK,   // Book is not found in database
    BOOK_BORROWED,  // Book is borrowed but not by this user
    DEVICE_ERROR,   // Not implemented yet, used when self test failed
    NETWORK_ERROR,  // I mean, it's in the name...
};

States sysState = IDLE;
States screenState = IDLE;
Errors sysError = OTHERS;

// String cardUID;
// String bookCode;
// String memberId;
// bool borrowing = true; // Set by system, false -> returning

struct BorrowingData {
    String cardID;
    String memberId;
    String memberName;
    String bookCode;
    String bookTitle;
    bool borrowing;
};

BorrowingData currentBorrowingData;

void displayErrorDetail() {
    switch (sysError) {
    case OTHERS:
        display.println("An unknown error occured.");
        break;

    case DATABASE_ERROR:
        display.println(
            "Something went wrong communicating with the database.");
        break;

    case INVALID_CARD:
        display.println("Your card is not registered in the system.");
        break;

    case INVALID_BOOK:
        display.println("This book is not found in the database");
        break;

    case BOOK_BORROWED:
        display.println("This book has already been borrowed");
        break;

    case DEVICE_ERROR:
        display.println("The device or a component of the device is not "
                        "responding as it should.");
        break;

    case NETWORK_ERROR:
        display.println("Could not connect to the database.");
        break;

    default:
        break;
    }
}

void changeScreen(States newScreenState) {
    display.clearDisplay();
    display.setCursor(0, 0);

    // TODO: Prettier screen
    switch (newScreenState) {
    case IDLE:
        display.println("Please scan your");
        display.println("member card to start");
        break;

    case VERIFY_CARD:
        display.println("Checking card...");
        break;

    case SCAN_BOOK:
        display.println("Welcome, " + currentBorrowingData.memberName);
        display.println("Please scan the book");
        display.println("you want to borrow");
        display.println("or return");
        break;

    case VERIFY_BOOK:
        display.println("Checking book status...");
        break;

    case CONFIRMATION:
        display.println("Scanned book: " + currentBorrowingData.bookTitle);
        display.println("Are you sure you want");
        if (currentBorrowingData.borrowing) {
            display.println("to borrow this book?");
        } else {
            display.println("to return this book?");
        }
        break;

    case DONE:
        if (currentBorrowingData.borrowing) {
            display.println("Book borrowed");
        } else {
            display.println("Book returned");
        }
        break;

    case CANCELLED:
        display.println("Cancelled.");
        display.println("Scan card again to restart.");
        break;

    case ERROR:
        display.println("Error:");
        displayErrorDetail();
        break;

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

    String converted = "";
    for (byte i = 0; i < mfrc522.uid.size; i++) {
        converted.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? "0" : ""));
        converted.concat(String(mfrc522.uid.uidByte[i], HEX));
    }

    if (converted.isEmpty()) {
        return false;
    }

    converted.toUpperCase();
    converted.trim();

    // cardUID = converted;
    currentBorrowingData.cardID = converted;

    mfrc522.PICC_HaltA();

    // Stop encryption on PCD
    mfrc522.PCD_StopCrypto1();

    return true;
}

bool checkCardValid() {
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;

    http.begin(client, "https://pltusmdkhoplqwnyjxvr.supabase.co/functions/v1/"
                       "check_member_card");

    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", "Bearer " SUPABASE_KEY);
    http.addHeader("apikey", SUPABASE_KEY);

    int resCode = http.POST("{\"id\":\"" + currentBorrowingData.cardID + "\"}");

    if (resCode == HTTP_CODE_OK) {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, http.getString());

        if (err) {
            sysError = DATABASE_ERROR;
            http.end();
            return false;
        }

        currentBorrowingData.memberId = doc["member_id"].as<String>();
        currentBorrowingData.memberName = doc["member_name"].as<String>();

        http.end();
        return true;
    } else if (resCode == HTTP_CODE_NOT_FOUND) {
        sysError = INVALID_CARD;
    } else {
        sysError = DATABASE_ERROR;
    }

    http.end();
    return false;
}

bool checkForBook() {
    if (Serial2.available()) {
        String code = Serial2.readStringUntil('\n');
        // process barcode
        code.trim();
        currentBorrowingData.bookCode = code;
        return true;
    }
    return false;
}

bool checkBookValid() {
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;

    http.begin(
        client,
        "https://pltusmdkhoplqwnyjxvr.supabase.co/functions/v1/check_status");

    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", "Bearer " SUPABASE_KEY);
    http.addHeader("apikey", SUPABASE_KEY);

    int resCode =
        http.POST("{\"member_id\":\"" + currentBorrowingData.memberId +
                  "\",\"isbn\":\"" + currentBorrowingData.bookCode + "\"}");

    if (resCode == HTTP_CODE_OK) {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, http.getString());

        if (err) {
            sysError = DATABASE_ERROR;
            http.end();
            return false;
        }

        currentBorrowingData.bookTitle = doc["book_name"].as<String>();
        currentBorrowingData.borrowing = doc["borrowing"].as<bool>();

        http.end();
        return true;
    } else if (resCode == HTTP_CODE_NOT_FOUND) {
        sysError = INVALID_BOOK;
    } else if (resCode == HTTP_CODE_CONFLICT) {
        sysError = BOOK_BORROWED;
    } else {
        sysError = DATABASE_ERROR;
    }

    http.end();
    return false;
}

bool borrow_book() {
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;

    http.begin(
        client,
        "https://pltusmdkhoplqwnyjxvr.supabase.co/functions/v1/borrow_book");

    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", "Bearer " SUPABASE_KEY);
    http.addHeader("apikey", SUPABASE_KEY);

    int resCode =
        http.POST("{\"member_id\":\"" + currentBorrowingData.memberId +
                  "\",\"isbn\":\"" + currentBorrowingData.bookCode + "\"}");

    if (resCode == HTTP_CODE_OK) {
        http.end();
        return true;
    } else if (resCode == HTTP_CODE_NOT_FOUND) {
        sysError = INVALID_BOOK;
    } else if (resCode == HTTP_CODE_CONFLICT) {
        sysError = BOOK_BORROWED;
    } else {
        sysError = DATABASE_ERROR;
    }

    http.end();
    return false;
}

bool return_book() {
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;

    http.begin(
        client,
        "https://pltusmdkhoplqwnyjxvr.supabase.co/functions/v1/return_book");

    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", "Bearer " SUPABASE_KEY);
    http.addHeader("apikey", SUPABASE_KEY);

    int resCode =
        http.POST("{\"member_id\":\"" + currentBorrowingData.memberId +
                  "\",\"isbn\":\"" + currentBorrowingData.bookCode + "\"}");

    if (resCode == HTTP_CODE_OK) {
        http.end();
        return true;
    } else if (resCode == HTTP_CODE_CONFLICT) {
        sysError = BOOK_BORROWED;
    } else {
        sysError = DATABASE_ERROR;
    }

    http.end();
    return false;
}

void setup() {
    pinMode(LBUTTON_PIN, INPUT_PULLUP);
    pinMode(RBUTTON_PIN, INPUT_PULLUP);

    Serial.begin(115200); // Initialize Serial for debug console

    Serial2.begin(9600, SERIAL_8N1, RX_PIN,
                  TX_PIN); // Initialize GM65 Serial

    WiFi.begin(WIFI_SSID, WIFI_PASS);

    mfrc522.PCD_Init(); // Initialize PCD for RC522

    // I2C Init
    Wire.begin(SDA_PIN, SCL_PIN);

    // Display Init
    display.begin(0x3C);
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);

    // TODO: Self test

    changeScreen(IDLE);
}

void loop() {
    if (sysState != screenState) {
        changeScreen(sysState);
    }

    if (WiFi.status() != WL_CONNECTED && sysState != ERROR) {
        sysState = ERROR;
        sysError = NETWORK_ERROR;
        return;
    } else if (WiFi.status() == WL_CONNECTED && sysState == ERROR &&
               sysError == NETWORK_ERROR) {
        sysState = IDLE;
    }

    Serial.println(sysState);

    switch (sysState) {
    case IDLE:
        if (checkForCard()) {
            sysState = VERIFY_CARD;
        }
        break;

    case VERIFY_CARD:
        // checkCardValid is blocking due to http request
        // so preemptively update screen
        changeScreen(VERIFY_CARD);

        sysState = checkCardValid() ? SCAN_BOOK : ERROR;
        break;

    case SCAN_BOOK:
        if (checkForBook()) {
            sysState = VERIFY_BOOK;
        }
        break;

    case VERIFY_BOOK:
        // checkBookValid is blocking due to http request
        // so preemptively update screen
        changeScreen(VERIFY_BOOK);

        sysState = checkBookValid() ? CONFIRMATION : ERROR;
        break;

    case CONFIRMATION:
        if (digitalRead(RBUTTON_PIN) == LOW) {
            sysState = DONE;

            bool success =
                currentBorrowingData.borrowing ? borrow_book() : return_book();

            sysState = success ? DONE : ERROR;
        } else if (digitalRead(LBUTTON_PIN) == LOW) {
            sysState = CANCELLED;
        }
        break;

    case DONE:
    case CANCELLED:
    case ERROR:
        if (!scheduler.active(5)) {
            scheduler.schedule(
                5,
                [](void *ctx) {
                    States *sysState = static_cast<States *>(ctx);
                    *sysState = IDLE;
                },
                &sysState, 5000);
        }
        break;

    default:
        break;
    }

    scheduler.step();
}
