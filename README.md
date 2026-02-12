# Library Borrowing Kiosk
An ESP32 based IoT project to make borrowing books convinient.

## Hardware
- ESP32
- RC522 RFID Module
- GM65 QR/Barcode Scanner (On UART/Serial Output mode)
- 128x64 I2C display (SH1106G driver)
    - You can use SSD1306 with some minor tweak.
- 2x Button

## Dependencies
- MFRC522v2
- Adafruit GFX Library
- Adafruit SH110X

## Pin Layout
| **Module** | **Device Pin** | **ESP32 Pin** | **Notes** |
|:---:|:---:|:---:|:---:|
| **RC522** | SDA/SS | GPIO5 | Change `SS_PIN` if you move this pin. |
|  | SCK | GPIO18 | Do *not* change - the library expects SPI-0. |
|  | MOSI | GPIO23 | Do *not* change - the library expects SPI-0. |
|  | MISO | GPIO19 | Do *not* change – the library expects SPI‑0. |
|  | IRQ | None (do **not** connect) | Unused; MFRC522v2 doesn’t use it for card detection. |
|  | GND | GND |  |
|  | RST | GPIO4 | Unused by library, but handy for a hard reset if needed. |
|  | 3.3V | 3V3 |  |
| **GM65** | VCC/5V | VIN (power input) | When not powered via USB, VIN becomes an *input*; keep this in mind when wiring to the VIN line. |
|  | TX | GPIO16 | Change `RX_PIN` if you move this pin. |
|  | RX | GPIO17 | Change `TX_PIN` if you move this pin. |
|  | GND | GND |  |
| **I2C Display** | SDA | GPIO21 | Change `SDA_PIN` if you move this pin. |
|  | SCL | GPIO22 | Change `SCL_PIN` if you move this pin. |
|  | GND | GND |  |
|  | VCC | 3V3 |  |
| **Button** | Left Button Pin | GPIO26 | Change `LBUTTON_PIN` if you move this pin. |
|  | Right Button Pin | GPIO27 | Change `RBUTTON_PIN` if you move this pin. |