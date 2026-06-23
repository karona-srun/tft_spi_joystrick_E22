# Wiring

This sketch targets an ESP32-S3 N16R8 module with:

- ILI9341 SPI TFT display
- SD card on the same SPI bus
- One-wire analog resistor-ladder joystick
- EByte E22-900T33S LoRa UART module
- GPS module over UART, with optional I2C/Qwiic detection

Pin definitions come from `TFT_SPI_JOYStrick_E22.ino`.

## ESP32-S3 Pin Map

| Signal | ESP32-S3 GPIO | Notes |
| --- | ---: | --- |
| TFT CS | 10 | ILI9341 chip select |
| TFT DC | 8 | ILI9341 data/command |
| TFT RST | 9 | ILI9341 reset |
| TFT MOSI | 11 | TFT SPI data in |
| TFT SCLK | 12 | TFT SPI clock |
| TFT MISO | 13 | TFT SPI data out |
| TFT BL | 7 | Backlight PWM |
| SD CS | 5 | SD card chip select |
| SD MOSI | 6 | SD SPI data in |
| SD SCLK | 16 | SD SPI clock |
| SD MISO | 3 | SD SPI data out |
| Joystick analog | 4 | Analog input with `INPUT_PULLUP` |
| Battery sense | 19 | ADC input from battery voltage divider |
| Back button | 38 | Button to GND, uses `INPUT_PULLUP` |
| Home button | 39 | Button to GND, uses `INPUT_PULLUP` |
| Buzzer | 37 | Active buzzer output |
| E22 RXD | 17 | ESP32 RX, connect to E22 TXD |
| E22 TXD | 18 | ESP32 TX, connect to E22 RXD |
| E22 AUX | 21 | E22 status input |
| E22 M0 | 14 | E22 mode control output |
| E22 M1 | 15 | E22 mode control output |
| GPS UART RXD | 35 | ESP32 RX, connect to GPS TX |
| GPS UART TXD | 36 | ESP32 TX, connect to GPS RX |
| GPS I2C SDA | 1 | Optional GPS I2C data |
| GPS I2C SCL | 2 | Optional GPS I2C clock |

## TFT ILI9341

| TFT pin | ESP32-S3 GPIO |
| --- | ---: |
| VCC | 3V3 |
| GND | GND |
| CS | 10 |
| RST / RESET | 9 |
| DC / D-C | 8 |
| MOSI / SDI / DIN | 11 |
| SCK / CLK | 12 |
| LED / BL | 7 |
| MISO / SDO | 13 |


The sketch uses the default hardware SPI bus for the TFT with:

```cpp
SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TFT_CS);
```

## SD Card

| SD pin | ESP32-S3 GPIO |
| --- | ---: |
| VCC | 3V3 |
| GND | GND |
| CS | 5 |
| MOSI / DI | 6 |
| SCK / CLK | 16 |
| MISO / DO | 3 |

The SD card uses a separate SPI bus from the TFT:

```cpp
sdSPI.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);
```

## Analog Joystick

| Joystick pin | ESP32-S3 GPIO |
| --- | ---: |
| Signal / OUT | 4 |
| VCC | 3V3 |
| GND | GND |

This sketch expects a resistor-ladder joystick on a single analog pin. Direction thresholds are read from GPIO4 in `SystemInput.ino`.

## Back and Home Buttons

| Button | ESP32-S3 GPIO | Wiring |
| --- | ---: | --- |
| Back | 38 | Button between GPIO38 and GND |
| Home | 39 | Button between GPIO39 and GND |

The sketch enables internal pullups, so a pressed button reads LOW. Back behaves like joystick LEFT. Home returns to the Home tab.

## Battery Sense

| Battery sense signal | ESP32-S3 GPIO | Notes |
| --- | ---: | --- |
| Divider output | 19 | ADC input |
| GND | GND | Common ground |

Important:

- Do not connect a Li-ion/LiPo battery directly to GPIO19.
- Use a voltage divider so the GPIO19 input stays below 3.3V.
- The sketch assumes a 2:1 divider by default, with 3.30V as empty and 4.20V as full.
- If your resistor divider is different, update `BATTERY_DIVIDER_RATIO` in `TFT_SPI_JOYStrick_E22.ino`.

## Buzzer

| Buzzer pin | ESP32-S3 GPIO | Notes |
| --- | ---: | --- |
| Signal / + | 37 | Active buzzer output |
| - / GND | GND | Common ground |

The sketch is configured for an active buzzer with `ACTIVE_BUZZER true`.

## EByte E22-900T33S LoRa

| E22 pin | ESP32-S3 GPIO | Direction |
| --- | ---: | --- |
| TXD | 17 | E22 to ESP32 RX |
| RXD | 18 | ESP32 TX to E22 |
| AUX | 21 | E22 to ESP32 |
| M0 | 14 | ESP32 to E22 |
| M1 | 15 | ESP32 to E22 |
| GND | GND | Common ground |
| VCC | External 5V supply | High-current power |

Important:

- Cross UART lines: ESP32 GPIO17 RX connects to E22 TXD, and ESP32 GPIO18 TX connects to E22 RXD.
- Use an external 5V high-current supply for the E22-900T33S.
- Connect the external supply ground to ESP32 GND.
- The sketch sets M0 and M1 LOW for normal mode.

## GPS

| GPS signal | ESP32-S3 GPIO | Direction |
| --- | ---: | --- |
| TX | 35 | GPS to ESP32 RX |
| RX | 36 | ESP32 TX to GPS |
| SDA | 1 | Optional I2C data |
| SCL | 2 | Optional I2C clock |
| GND | GND | Common ground |
| VCC | Module supply | Match your GPS breakout requirements |

Important:

- Cross UART lines: ESP32 GPIO35 RX connects to GPS TX, and ESP32 GPIO36 TX connects to GPS RX.
- The sketch reads standard NMEA sentences at 115200 baud on UART.
- I2C is initialized on GPIO1/GPIO2 and scanned to show whether a GPS/Qwiic device ACKs, but location data is parsed from UART NMEA.
- Make sure the GPS module logic level is compatible with ESP32-S3 3.3V GPIO.

## Power Notes

- Power the ESP32-S3, TFT, SD card, and joystick from stable 3.3V logic rails unless your breakout boards explicitly include regulators and level shifting.
- The E22-900T33S should not be powered from the ESP32-S3 3.3V pin. Use a separate 5V supply that can handle transmit current.
- All modules must share a common GND.
