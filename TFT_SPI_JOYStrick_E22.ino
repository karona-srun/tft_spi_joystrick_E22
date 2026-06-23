#include <SPI.h>
#include <Wire.h>
#include <SD.h>
#include <FS.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <Preferences.h>
#include <WiFi.h>
#define ENABLE_BLE_NETWORK 0
#if ENABLE_BLE_NETWORK
#include <NimBLEDevice.h>
#endif
#include <mbedtls/base64.h>
#include <time.h>
#define FREQUENCY_915
#define E22_30
#include <LoRa_E22.h>
#include "sidebar_icons.h"
#include "launcher_icons.h"
#include "startup_logo.h"

#if defined(E22_30)
const uint8_t POWER_22 = POWER_30;
#endif

// Target board: ESP32-S3 N16R8 module (16 MB flash, 8 MB OPI PSRAM).
#if !defined(CONFIG_IDF_TARGET_ESP32S3)
#error "Select an ESP32-S3 board target for this sketch."
#endif

// ===================== PIN SETUP =====================

#define TFT_CS    10
#define TFT_RST   9
#define TFT_DC    8
#define TFT_MOSI  11
#define TFT_SCLK  12
#define TFT_BL    7
#define TFT_MISO  13

#define SD_CS     5
#define SD_MOSI   6
#define SD_SCLK   16
#define SD_MISO   3

#define JOY_PIN   4
#define BACK_BTN_PIN 38
#define HOME_BTN_PIN 39
#define BUZZER_PIN 37
#define ACTIVE_BUZZER true
#define BATTERY_PIN 19

#if TFT_CS == SD_CS
#error "TFT_CS and SD_CS must be different."
#endif
#if TFT_DC == SD_CS || TFT_RST == SD_CS || TFT_BL == SD_CS || JOY_PIN == SD_CS || BACK_BTN_PIN == SD_CS || HOME_BTN_PIN == SD_CS || TFT_MOSI == SD_CS || TFT_SCLK == SD_CS || TFT_MISO == SD_CS
#error "SD_CS conflicts with a TFT or input pin."
#endif
#if BACK_BTN_PIN == HOME_BTN_PIN || BACK_BTN_PIN == JOY_PIN || HOME_BTN_PIN == JOY_PIN
#error "Back, Home, and joystick inputs must use different GPIO pins."
#endif

#if SD_MOSI == SD_SCLK || SD_MOSI == SD_MISO || SD_SCLK == SD_MISO
#error "SD SPI MOSI, SCLK, and MISO must be three different GPIO pins."
#endif
#if TFT_MOSI == TFT_CS || TFT_SCLK == TFT_CS || TFT_MISO == TFT_CS
#error "TFT SPI data pins must not be reused as TFT_CS."
#endif
#if SD_MOSI == SD_CS || SD_SCLK == SD_CS || SD_MISO == SD_CS
#error "SD SPI data pins must not be reused as SD_CS."
#endif
#if SD_MOSI == TFT_MOSI || SD_MOSI == TFT_SCLK || SD_MOSI == TFT_MISO || SD_SCLK == TFT_MOSI || SD_SCLK == TFT_SCLK || SD_SCLK == TFT_MISO || SD_MISO == TFT_MOSI || SD_MISO == TFT_SCLK || SD_MISO == TFT_MISO
#error "SD SPI pins must not share TFT SPI pins in this wiring."
#endif
#if SD_MOSI == TFT_CS || SD_MOSI == TFT_DC || SD_MOSI == TFT_RST || SD_MOSI == TFT_BL || SD_MOSI == JOY_PIN || SD_MOSI == BACK_BTN_PIN || SD_MOSI == HOME_BTN_PIN || SD_SCLK == TFT_CS || SD_SCLK == TFT_DC || SD_SCLK == TFT_RST || SD_SCLK == TFT_BL || SD_SCLK == JOY_PIN || SD_SCLK == BACK_BTN_PIN || SD_SCLK == HOME_BTN_PIN || SD_MISO == TFT_CS || SD_MISO == TFT_DC || SD_MISO == TFT_RST || SD_MISO == TFT_BL || SD_MISO == JOY_PIN || SD_MISO == BACK_BTN_PIN || SD_MISO == HOME_BTN_PIN
#error "SD SPI pins conflict with a TFT control, backlight, or joystick pin."
#endif

// E22-900T33S local LoRa UART wiring for ESP32-S3.
// ESP32 RXD GPIO17 connects to E22 TXD.
// ESP32 TXD GPIO18 connects to E22 RXD.
// Keep the T33S on an external 5V/high-current supply with common GND.
#define E22_RXD   17
#define E22_TXD   18
#define E22_AUX   21
#define E22_M0    14
#define E22_M1    15

// GPS wiring.
// ESP32 RXD GPIO35 connects to GPS TX.
// ESP32 TXD GPIO36 connects to GPS RX.
// Optional GPS I2C/Qwiic: SDA GPIO1, SCL GPIO2.
#define GPS_UART_RXD 35
#define GPS_UART_TXD 36
#define GPS_I2C_SDA  1
#define GPS_I2C_SCL  2
#define GPS_BAUD     115200

#define BL_FREQ     20000
#define BL_RES      10
#define BL_DUTY_MAX ((1 << BL_RES) - 1)
#define MIN_BRIGHTNESS_PERCENT 35
#define SD_SPI_FREQ 4000000
#define BATTERY_DIVIDER_RATIO 2.0f
#define BATTERY_EMPTY_MV 3300
#define BATTERY_FULL_MV 4200

const char* HARDWARE_TARGET = "ESP32-S3 N16R8";

uint8_t MY_ADDH = 0x00;
uint8_t MY_ADDL = 0x09;     // Default Node ID
uint8_t TARGET_ADDH = 0xFF;
uint8_t TARGET_ADDL = 0xFF; // Default Target: all broadcast
uint8_t REPEATER_ADDH = 0xFF;
uint8_t REPEATER_ADDL = 0xFF;
uint8_t NETWORK_ID = 0x01;  // Default Network ID
uint8_t CHANNEL = 0x41;     // Default Channel 915MHz
uint16_t CRYPT = 0x8002;    // KEY 32770
bool USE_REPEATER = false;  // Repeater mode
uint8_t E22_TX_POWER = POWER_30;

Adafruit_ILI9341 tft = Adafruit_ILI9341(&SPI, TFT_DC, TFT_CS, TFT_RST);
SPIClass sdSPI(HSPI);
LoRa_E22 e22ttl(E22_RXD, E22_TXD, &Serial2, E22_AUX, E22_M0, E22_M1, UART_BPS_RATE_9600, SERIAL_8N1);
HardwareSerial gpsSerial(1);
Preferences storage;

// ===================== LOMHOR OS PALETTE =====================
// Screen is landscape 320x240

#define C_BG         0x0000      // desktop black
#define C_SIDEBAR    0x1084      // dock rail
#define C_NAV_SLOT   0x2947      // idle chrome
#define C_CARD_BG    0x0000      // black card/menu surface
#define C_BORDER     0x3A2A      // glass border
#define C_ACCENT     0x253F      // OS blue
#define C_TEXT       0xFFFF      // white
#define C_MUTED      0xA534      // readable muted text
#define C_ICON_ACT   0x0000      // active icon on green
#define C_ICON_IDLE  0x8C71      // idle icon grey
#define C_HEADER_BG  0x0863      // title bar
#define C_WARN       0xFD20
#define C_ERROR      0xF800
#define C_SEL_BG     0x0000      // selected cards stay black; blue border/text show focus
#define C_PANEL      0x0841
#define C_TILE_BG    0x1084
#define C_TILE_ALT   0x1886
#define C_GREEN      0x47E9
#define C_YELLOW     0xFEA0
#define C_CYAN       0x8E7F
#define C_RED_SOFT   0xF9E7
#define C_RESTART_BG 0xF800

// Mobile-style full-screen content. The old sidebar is replaced by Home app icons.
#define SIDEBAR_W    0
#define CONTENT_X    0
#define CONTENT_W    (320 - CONTENT_X)
#define PANEL_PAD    4
#define PANEL_X      (CONTENT_X + PANEL_PAD)
#define PANEL_Y      4
#define PANEL_W      (320 - PANEL_X - PANEL_PAD)
#define PANEL_H      (240 - PANEL_Y - PANEL_PAD)
#define PANEL_INSET  8
#define TITLE_Y      (PANEL_Y + 7)
#define BODY_Y       (PANEL_Y + 30)
#define FOOTER_Y     (PANEL_Y + PANEL_H - 12)

// ===================== SETTINGS =====================

int brightnessPercent = 100;

void applyBrightness() {
  if (brightnessPercent < MIN_BRIGHTNESS_PERCENT) brightnessPercent = MIN_BRIGHTNESS_PERCENT;
  if (brightnessPercent > 100) brightnessPercent = 100;
  // ESP32 Arduino core 3.x: ledcWrite(pin, duty)
  ledcWrite(TFT_BL, brightnessPercent * BL_DUTY_MAX / 100);
}

// ===================== STATE =====================

#define MAX_FILES 40
String fileNames[MAX_FILES];
bool   fileIsDir[MAX_FILES];
int    fileCount     = 0;
int    selectedIndex = 0;
int    topIndex      = 0;
int    prevSelectedIndex = -1;
int    prevTopIndex      = -1;

String currentPath = "/";
bool   sdReady     = false;
String textViewerPath = "";
int    textViewerTopLine = 0;
int    textViewerLineCount = 0;

enum ScreenMode {
  SCREEN_HOME,
  SCREEN_HOME_DETAILS,
  SCREEN_USER,
  SCREEN_CHAT_PRESETS,
  SCREEN_CHAT_KEYBOARD,
  SCREEN_CHAT_SEND_TO,
  SCREEN_MAP,
  SCREEN_LOCATION,
  SCREEN_FILE_MANAGER,
  SCREEN_TEXT_VIEWER,
  SCREEN_JOYSTICK_TEST,
  SCREEN_SD_INFO,
  SCREEN_CONFIG,
  SCREEN_SETTINGS,
  SCREEN_ABOUT,
  SCREEN_OPTION_EDITOR
};

ScreenMode screenMode = SCREEN_HOME;
ScreenMode textViewerReturnScreen = SCREEN_FILE_MANAGER;

void returnToHomeTab();
void pollE22Radio(bool force = false);
bool handleLocalPacket(String packet, int16_t rssi);
void showRestartLoadingScreen();
void drawHomeDetailsContent(bool fullRedraw = true);
void initNetworkDevices();

// App index/tab mapping (0=Home,1=User,2=Chat,3=Map,4=Location,5=Records,6=Config,7=Settings,8=About,9=Restart)
int sidebarTab = 0;
const int SIDEBAR_TABS = 10;
int sidebarTopTab = 0;
const int SIDEBAR_VISIBLE_TABS = 6;
const int SIDEBAR_TAB_Y = 12;
const int SIDEBAR_TAB_STEP = 38;
int homeAppIndex = 0;
int homeAppTopIndex = 0;
const int HOME_APP_COUNT = 10;
const int HOME_APP_COLS = 5;
const int HOME_APP_ROWS = 2;
const int HOME_APP_VISIBLE = HOME_APP_COLS * HOME_APP_ROWS;
int homeDetailsTopIndex = 0;
int prevHomeDetailsThumbY = -1;
int prevHomeDetailsThumbH = 0;
const int HOME_DETAILS_ROW_COUNT = 16;
const int HOME_DETAILS_VISIBLE_ROWS = 7;

// Config menu
int configIndex = 0;
const int configCount = 2;
int configModeIndex = 0;   // Client, Repeater
int configPowerIndex = 0;  // E22-900T33S: 30dBm, 27dBm, 24dBm, 21dBm

// Settings menu
int settingsIndex = 0;
const int settingsCount = 6;
int displayTextSize = 1;
bool alertSoundOn = true;
int sleepModeIndex = 4;    // 10s, 30s, 1m, 5m, Never
bool wifiApEnabled = true;
bool bluetoothEnabled = false;
bool wifiApReady = false;
bool bluetoothReady = false;
String wifiApPassword = "12345678";
unsigned long lastActivityTime = 0;
unsigned long lastBrightnessRefresh = 0;
unsigned long lastHomeClockRefresh = 0;
unsigned long lastHomeTopStatusRefresh = 0;
bool displaySleeping = false;

void initBuzzer() {
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
}

void beep(int duration, int frequency = 2500) {
  if (!alertSoundOn) return;

  if (ACTIVE_BUZZER) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(duration);
    digitalWrite(BUZZER_PIN, LOW);
  } else {
    tone(BUZZER_PIN, frequency);
    delay(duration);
    noTone(BUZZER_PIN);
  }

  delay(120);
}

void messageNotification() {
  beep(120, 2200);
  delay(80);
  beep(120, 2600);
}

void sosAlarm() {
  for (int i = 0; i < 3; i++) {
    beep(150, 2500);
  }

  delay(250);

  for (int i = 0; i < 3; i++) {
    beep(600, 2500);
  }

  delay(250);

  for (int i = 0; i < 3; i++) {
    beep(150, 2500);
  }
}

void playReceivedMessageSound(String message) {
  message.trim();
  if (message.length() == 0) return;

  if (message.equalsIgnoreCase("SOS")) {
    sosAlarm();
  } else {
    messageNotification();
  }
}

int optionGroup = 0;       // 0=settings, 1=config
int optionItem = 0;

const char* configModes[] = {"Client", "Repeater"};
const char* configPowers[] = {"E22-900T33S Max", "E22 27dBm", "E22 24dBm", "E22 21dBm"};
const char* textSizes[] = {"Font size 1", "Font size 2", "Font size 3"};
const char* alertSounds[] = {"OFF", "ON"};
const char* sleepModes[] = {"10s", "30s", "1m", "5m", "Never"};
const char* appVersion = "v0.0.20";
const char* username = "Karona SRUN";
const char* STORAGE_NAMESPACE = "lomhor";
const char* CHAT_LOG_DIR = "/chat";
const char* BLE_SERVICE_UUID = "74f90001-05a5-4a65-9d6a-7f1d8f224001";

bool e22Ready = false;
String e22StatusText = "Radio starting";
String lastRxMessage = "No local messages";
String lastTxMessage = "Nothing sent";
uint32_t sentCount = 0;
uint32_t receivedCount = 0;
int16_t lastRssiSignal = -255;
unsigned long lastRadioPoll = 0;
unsigned long lastNodeBeacon = 0;
unsigned long lastNodeUiRefresh = 0;
uint16_t txMessageCounter = 0;
String awaitingAckId = "";
bool awaitingAckReceived = false;
int userTopIndex = 0;

bool gpsUartReady = false;
bool gpsI2cReady = false;
bool gpsHasFix = false;
bool gpsDateValid = false;
String gpsStatusText = "GPS starting";
String gpsLineBuffer = "";
double gpsLatitude = 0.0;
double gpsLongitude = 0.0;
float gpsAltitudeM = 0.0f;
float gpsSpeedKmh = 0.0f;
float gpsCourseDeg = 0.0f;
float gpsHdop = 0.0f;
uint8_t gpsSatellites = 0;
uint8_t gpsFixQuality = 0;
String gpsUtcTime = "--:--:--";
String gpsUtcDate = "----/--/--";
String lastGpsSentence = "No NMEA";
unsigned long lastGpsCharMs = 0;
unsigned long lastGpsSentenceMs = 0;
unsigned long lastGpsFixMs = 0;
unsigned long lastGpsUiRefresh = 0;
int locationTopIndex = 0;
const int LOCATION_ROW_COUNT = 10;
const int LOCATION_VISIBLE_ROWS = 3;
const uint32_t GPS_BAUD_CANDIDATES[] = {GPS_BAUD, 115200};
const uint8_t GPS_BAUD_CANDIDATE_COUNT = sizeof(GPS_BAUD_CANDIDATES) / sizeof(GPS_BAUD_CANDIDATES[0]);
uint8_t gpsBaudIndex = 0;
uint32_t gpsActiveBaud = GPS_BAUD;

void clampStoredOptions() {
  if (brightnessPercent < MIN_BRIGHTNESS_PERCENT) brightnessPercent = MIN_BRIGHTNESS_PERCENT;
  if (brightnessPercent > 100) brightnessPercent = 100;
  if (displayTextSize < 1) displayTextSize = 1;
  if (displayTextSize > 3) displayTextSize = 3;
  if (sleepModeIndex < 0 || sleepModeIndex > 4) sleepModeIndex = 4;
  if (configModeIndex < 0 || configModeIndex > 1) configModeIndex = 0;
  if (configPowerIndex < 0 || configPowerIndex > 3) configPowerIndex = 0;
  if (wifiApPassword.length() < 8 || wifiApPassword.length() > 63) wifiApPassword = "12345678";
}

void loadStoredOptions() {
  if (!storage.begin(STORAGE_NAMESPACE, true)) {
    Serial.println("Storage open failed, using defaults");
    return;
  }

  brightnessPercent = storage.getInt("bright", brightnessPercent);
  displayTextSize = storage.getInt("txtSize", displayTextSize);
  alertSoundOn = storage.getBool("alert", alertSoundOn);
  sleepModeIndex = storage.getInt("sleep", sleepModeIndex);
  wifiApEnabled = storage.getBool("wifiAp", wifiApEnabled);
  wifiApPassword = storage.getString("wifiPass", wifiApPassword);
  bluetoothEnabled = storage.getBool("ble", bluetoothEnabled);
  configModeIndex = storage.getInt("mode", configModeIndex);
  configPowerIndex = storage.getInt("power", configPowerIndex);
  storage.end();

  clampStoredOptions();
  syncE22UiSettings();
  Serial.println("Settings loaded from ESP32 storage");
}

void saveSettingsToStorage() {
  clampStoredOptions();
  if (!storage.begin(STORAGE_NAMESPACE, false)) {
    Serial.println("Settings save failed: storage open");
    return;
  }

  storage.putInt("bright", brightnessPercent);
  storage.putInt("txtSize", displayTextSize);
  storage.putBool("alert", alertSoundOn);
  storage.putInt("sleep", sleepModeIndex);
  storage.putBool("wifiAp", wifiApEnabled);
  storage.putString("wifiPass", wifiApPassword);
  storage.putBool("ble", bluetoothEnabled);
  storage.end();
  Serial.println("Settings saved to ESP32 storage");
}

void saveConfigToStorage() {
  clampStoredOptions();
  syncE22UiSettings();
  if (!storage.begin(STORAGE_NAMESPACE, false)) {
    Serial.println("Config save failed: storage open");
    return;
  }

  storage.putInt("mode", configModeIndex);
  storage.putInt("power", configPowerIndex);
  storage.end();
  Serial.println("Config saved to ESP32 storage");
}

const unsigned long NODE_BEACON_INTERVAL_MS = 5000UL;
const unsigned long NODE_ONLINE_TIMEOUT_MS = 10000UL;
const unsigned long ACK_WAIT_MS = 2500UL;
const unsigned long SEND_DIALOG_VISIBLE_MS = 2000UL;
const uint8_t ACK_RETRY_MAX = 2;
const size_t MAX_CHAT_BODY_BYTES = 150;
const int MAX_ONLINE_NODES = 20;

struct OnlineNode {
  uint8_t addh;
  uint8_t addl;
  uint8_t netId;
  uint8_t channel;
  int16_t rssi;
  String name;
  unsigned long lastSeen;
  bool isRepeater;
  bool used;
};

OnlineNode onlineNodes[MAX_ONLINE_NODES];

// Chat preset menu
int chatPresetIndex = 0;
int chatPresetTopIndex = 0;
const int chatPresetCount = 10;
const int CHAT_VISIBLE_PRESETS = 5;
const char* chatPresets[] = {
  "Free Text",
  "Hi!",
  "Help me!",
  "On the way",
  "Bye...",
  "Yes",
  "No",
  "Okay",
  "SOS",
  "Warning!"
};

String freeTextMessage = "";
String pendingChatMessage = "";
bool pendingChatFromFreeText = false;
String wifiPasswordEdit = "";
bool keyboardEditingWifiPassword = false;
bool wifiPasswordTooShort = false;
int keyboardIndex = 0;
int sendTargetIndex = 0;
int sendTargetTopIndex = 0;
const int SEND_TARGET_VISIBLE = 4;
const int keyboardKeyCount = 48;
const int KEYBOARD_COLS = 8;
const char* keyboardKeys[] = {
  "A", "B", "C", "D", "E", "F", "G", "H",
  "I", "J", "K", "L", "M", "N", "O", "P",
  "Q", "R", "S", "T", "U", "V", "W", "X",
  "Y", "Z", "0", "1", "2", "3", "4", "5",
  "6", "7", "8", "9", ".", ",", "?", "!",
  "@", "#", "&", "/", "SP", "DEL", "BACK", "SEND"
};

// ===================== JOYSTICK =====================

unsigned long lastMoveTime = 0;
const unsigned long moveDelay = 200;

// ===================== ICON DRAWING =====================
// Small pixel icons drawn with primitives

void drawIconHome(int x, int y, uint16_t col) {
  tft.drawLine(x+7, y, x+14, y+6, col);
  tft.drawLine(x+7, y, x, y+6, col);
  tft.drawLine(x+2, y+6, x+2, y+13, col);
  tft.drawLine(x+12, y+6, x+12, y+13, col);
  tft.drawLine(x+2, y+13, x+12, y+13, col);
  tft.fillRect(x+6, y+9, 3, 5, col);
}

void drawIconNodes(int x, int y, uint16_t col) {
  tft.drawLine(x+7, y+4, x+3, y+10, col);
  tft.drawLine(x+7, y+4, x+12, y+10, col);
  tft.drawLine(x+3, y+10, x+12, y+10, col);
  tft.fillCircle(x+7, y+3, 3, col);
  tft.fillCircle(x+3, y+11, 3, col);
  tft.fillCircle(x+12, y+11, 3, col);
}

void drawIconMsg(int x, int y, uint16_t col) {
  tft.drawRoundRect(x, y+2, 15, 10, 2, col);
  tft.drawLine(x+3, y+5, x+7, y+8, col);
  tft.drawLine(x+7, y+8, x+12, y+5, col);
  tft.drawLine(x+5, y+12, x+3, y+15, col);
}

void drawIconMap(int x, int y, uint16_t col, int size = 16) {
  const uint16_t mapGreen = 0x47E9;
  const uint16_t mapYellow = 0xFEA0;
  const uint16_t mapBlue = 0x8E7F;
  const uint16_t pinRed = 0xF9E7;
  const uint16_t pinShadow = 0xE124;
  uint16_t holeCol = (col == C_ICON_ACT) ? C_HEADER_BG : C_BG;
  if (size < 16) size = 16;

  int x0 = x;
  int x3 = x + 3 * size / 16;
  int x4 = x + 4 * size / 16;
  int x7 = x + 7 * size / 16;
  int x8 = x + 8 * size / 16;
  int x9 = x + 9 * size / 16;
  int x10 = x + 10 * size / 16;
  int x11 = x + 11 * size / 16;
  int x13 = x + 13 * size / 16;
  int x14 = x + 14 * size / 16;
  int x15 = x + 15 * size / 16;
  int y6 = y + 6 * size / 16;
  int y7 = y + 7 * size / 16;
  int y8 = y + 8 * size / 16;
  int y10 = y + 10 * size / 16;
  int y14 = y + 14 * size / 16;
  int y15 = y + 15 * size / 16;
  int rPin = 6 * size / 16;
  int rShadow = 4 * size / 16;
  int rHole = 3 * size / 16;

  tft.fillTriangle(x0, y15, x4, y8, x7, y15, mapBlue);
  tft.fillTriangle(x4, y8, x9, y8, x7, y15, mapYellow);
  tft.fillTriangle(x7, y15, x9, y8, x15, y15, mapGreen);
  tft.fillTriangle(x9, y8, x15, y8, x15, y15, mapYellow);
  tft.drawLine(x4, y8, x14, y15, C_BORDER);

  tft.fillTriangle(x8, y15, x3, y7, x13, y7, pinRed);
  tft.fillCircle(x8, y6, rPin, pinRed);
  tft.fillTriangle(x10, y14, x13, y7, x11, y10, pinShadow);
  tft.fillCircle(x10, y6, rShadow, pinShadow);
  tft.fillCircle(x8, y6, rHole, holeCol);
}

void drawIconSettings(int x, int y, uint16_t col) {
  tft.drawCircle(x+7, y+7, 4, col);
  tft.fillCircle(x+7, y+7, 1, col);
  tft.drawLine(x+7, y, x+7, y+2, col);
  tft.drawLine(x+7, y+12, x+7, y+14, col);
  tft.drawLine(x, y+7, x+2, y+7, col);
  tft.drawLine(x+12, y+7, x+14, y+7, col);
  tft.drawLine(x+2, y+2, x+4, y+4, col);
  tft.drawLine(x+10, y+10, x+12, y+12, col);
}

void drawIconConfig(int x, int y, uint16_t col) {
  tft.drawFastVLine(x+3, y+1, 13, col);
  tft.drawFastVLine(x+8, y+1, 13, col);
  tft.drawFastVLine(x+13, y+1, 13, col);
  tft.fillCircle(x+3, y+5, 2, col);
  tft.fillCircle(x+8, y+10, 2, col);
  tft.fillCircle(x+13, y+4, 2, col);
}

void drawIconFile(int x, int y, uint16_t col) {
  tft.drawRect(x, y, 12, 15, col);
  tft.drawLine(x+8, y, x+12, y+4, col);
  tft.drawLine(x+8, y, x+8, y+4, col);
  tft.drawLine(x+8, y+4, x+12, y+4, col);
}

void drawIconFolder(int x, int y, uint16_t col) {
  tft.drawRoundRect(x, y+3, 15, 11, 2, col);
  tft.drawLine(x+2, y+3, x+5, y, col);
  tft.drawLine(x+5, y, x+9, y, col);
  tft.drawLine(x+9, y, x+11, y+3, col);
}

void drawIconSun(int x, int y, uint16_t col) {
  tft.drawCircle(x+7, y+7, 4, col);
  tft.drawLine(x+7, y, x+7, y+2, col);
  tft.drawLine(x+7, y+12, x+7, y+14, col);
  tft.drawLine(x, y+7, x+2, y+7, col);
  tft.drawLine(x+12, y+7, x+14, y+7, col);
}

void drawIconSignal(int x, int y, uint16_t col) {
  tft.fillRect(x+1, y+10, 2, 4, col);
  tft.fillRect(x+5, y+7, 2, 7, col);
  tft.fillRect(x+9, y+4, 2, 10, col);
  tft.fillRect(x+13, y+1, 2, 13, col);
}

void drawTopWifiIcon(int x, int y, uint16_t col) {
  tft.drawCircle(x + 9, y + 9, 9, col);
  tft.drawCircle(x + 9, y + 9, 6, col);
  tft.drawCircle(x + 9, y + 9, 3, col);
  tft.fillRect(x - 1, y + 9, 22, 11, C_ACCENT);
  tft.fillCircle(x + 9, y + 14, 2, col);
}

void drawTopGpsIcon(int x, int y, uint16_t col) {
  tft.drawCircle(x + 7, y + 6, 5, col);
  tft.fillCircle(x + 7, y + 6, 1, col);
  tft.drawLine(x + 3, y + 10, x + 7, y + 18, col);
  tft.drawLine(x + 11, y + 10, x + 7, y + 18, col);
  tft.drawLine(x + 4, y + 11, x + 10, y + 11, col);
}

void drawTopRadioIcon(int x, int y, uint16_t col) {
  tft.drawFastVLine(x + 9, y + 2, 17, col);
  tft.fillCircle(x + 9, y + 11, 2, col);
  tft.drawCircle(x + 9, y + 11, 7, col);
  tft.drawCircle(x + 9, y + 11, 11, col);
  tft.fillRect(x + 6, y - 1, 7, 24, C_ACCENT);
  tft.drawFastVLine(x + 9, y + 2, 17, col);
  tft.fillCircle(x + 9, y + 11, 2, col);
}

void drawTopBatteryIcon(int x, int y, uint16_t col, int percent) {
  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;
  tft.drawRoundRect(x, y + 3, 10, 18, 2, col);
  tft.fillRect(x + 3, y, 4, 3, col);
  int fillH = percent * 13 / 100;
  tft.fillRect(x + 3, y + 19 - fillH, 4, fillH, col);
}

void drawMiniSignalBars(int x, int y, int bars, uint16_t col) {
  if (bars < 0) bars = 0;
  if (bars > 4) bars = 4;
  for (int i = 0; i < 4; i++) {
    int h = 3 + i * 3;
    uint16_t barCol = i < bars ? col : C_NAV_SLOT;
    tft.fillRect(x + i * 4, y + 12 - h, 3, h, barCol);
  }
}

int e22SignalBars() {
  if (!e22Ready || lastRssiSignal <= -250) return 0;
  if (lastRssiSignal > -65) return 4;
  if (lastRssiSignal > -85) return 3;
  if (lastRssiSignal > -105) return 2;
  return 1;
}

uint16_t readBatteryMv() {
  uint32_t total = 0;
  const int samples = 12;
  for (int i = 0; i < samples; i++) {
    total += analogReadMilliVolts(BATTERY_PIN);
    delayMicroseconds(250);
  }
  float pinMv = (float)total / samples;
  return (uint16_t)(pinMv * BATTERY_DIVIDER_RATIO + 0.5f);
}

int batteryPercentFromMv(uint16_t mv) {
  int percent = map(constrain(mv, BATTERY_EMPTY_MV, BATTERY_FULL_MV), BATTERY_EMPTY_MV, BATTERY_FULL_MV, 0, 100);
  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;
  return percent;
}

int batteryPercent() {
  return batteryPercentFromMv(readBatteryMv());
}

uint16_t batteryColor(int percent) {
  if (percent <= 15) return C_ERROR;
  if (percent <= 30) return C_WARN;
  return C_TEXT;
}

void drawBatteryIcon(int x, int y, int percent) {
  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;
  tft.drawRoundRect(x, y + 2, 22, 10, 2, C_MUTED);
  tft.fillRect(x + 22, y + 5, 2, 4, C_MUTED);
  int fillW = percent * 18 / 100;
  uint16_t fillCol = percent > 20 ? C_ACCENT : C_WARN;
  tft.fillRect(x + 2, y + 4, fillW, 6, fillCol);
}

void drawIconSd(int x, int y, uint16_t col) {
  tft.drawRoundRect(x+2, y, 11, 15, 2, col);
  tft.drawLine(x+9, y, x+13, y+4, col);
  tft.fillRect(x+4, y+3, 1, 3, col);
  tft.fillRect(x+7, y+3, 1, 3, col);
}

void drawIconInfo(int x, int y, uint16_t col) {
  tft.drawCircle(x+7, y+7, 6, col);
  tft.fillRect(x+7, y+6, 1, 5, col);
  tft.drawPixel(x+7, y+3, col);
}

int sidebarBitmapIndex(int tabIndex) {
  if (tabIndex <= 2) return tabIndex;
  if (tabIndex == 3) return -1;
  if (tabIndex == 4) return -1;
  if (tabIndex <= 8) return tabIndex - 2;
  return 5;
}

void drawSidebarIconSized(int tabIndex, int centerX, int centerY, bool active, int size) {
  int bitmapIndex = sidebarBitmapIndex(tabIndex);
  if (bitmapIndex < 0) {
    uint16_t col = active ? C_ICON_ACT : C_ICON_IDLE;
    drawIconMap(centerX - size / 2, centerY - size / 2, col, size);
    return;
  }
  const uint16_t* bitmap = getSidebarIconBitmap(bitmapIndex, active, size);
  tft.drawRGBBitmap(centerX - size / 2, centerY - size / 2, bitmap, size, size);
}

void drawSidebarIcon(int tabIndex, int x, int y, uint16_t col) {
  drawSidebarIconSized(tabIndex, x + 7, y + 7, col == C_ICON_ACT, SIDEBAR_ICON_LARGE);
}

// ===================== LAYOUT PRIMITIVES =====================

void drawHeader(String title) {
  tft.fillRoundRect(PANEL_X, PANEL_Y, PANEL_W, 22, 6, C_HEADER_BG);
  tft.drawRoundRect(PANEL_X, PANEL_Y, PANEL_W, 22, 6, C_BORDER);
  tft.fillCircle(PANEL_X + 10, PANEL_Y + 11, 3, C_ERROR);
  tft.fillCircle(PANEL_X + 20, PANEL_Y + 11, 3, C_WARN);
  tft.fillCircle(PANEL_X + 30, PANEL_Y + 11, 3, C_ACCENT);
  tft.setTextSize(1);
  tft.setTextColor(C_TEXT);
  int cx = PANEL_X + 44;
  tft.setCursor(cx, TITLE_Y);
  tft.print(title);

  String status = e22Ready ? "Radio" : "Off";
  int sx = PANEL_X + PANEL_W - status.length() * 6 - 8;
  if (sx > cx + title.length() * 6 + 8) {
    tft.setTextColor(e22Ready ? C_ACCENT : C_MUTED);
    tft.setCursor(sx, TITLE_Y);
    tft.print(status);
  }
}

void ensureSidebarVisible() {
  if (sidebarTab < sidebarTopTab) sidebarTopTab = sidebarTab;
  if (sidebarTab >= sidebarTopTab + SIDEBAR_VISIBLE_TABS) sidebarTopTab = sidebarTab - SIDEBAR_VISIBLE_TABS + 1;
  if (sidebarTopTab < 0) sidebarTopTab = 0;
  int maxTop = SIDEBAR_TABS - SIDEBAR_VISIBLE_TABS;
  if (maxTop < 0) maxTop = 0;
  if (sidebarTopTab > maxTop) sidebarTopTab = maxTop;
}

void drawSidebarScrollbar() {
  if (SIDEBAR_TABS <= SIDEBAR_VISIBLE_TABS) return;

  int trackY = 6;
  int trackH = 230;
  int thumbH = trackH * SIDEBAR_VISIBLE_TABS / SIDEBAR_TABS;
  if (thumbH < 18) thumbH = 18;
  int denom = SIDEBAR_TABS - SIDEBAR_VISIBLE_TABS;
  int thumbY = trackY + (trackH - thumbH) * sidebarTopTab / denom;
  tft.fillRoundRect(SIDEBAR_W, trackY, 3, trackH, 2, C_NAV_SLOT);
  tft.fillRoundRect(SIDEBAR_W, thumbY, 3, thumbH, 2, C_ACCENT);
}

void drawSidebarSlot(int i, bool active, int iconSize) {
  if (SIDEBAR_W <= 0) return;
  if (i < sidebarTopTab || i >= sidebarTopTab + SIDEBAR_VISIBLE_TABS) return;

  int ty = SIDEBAR_TAB_Y + (i - sidebarTopTab) * SIDEBAR_TAB_STEP;
  int centerX = SIDEBAR_W / 2;
  int centerY = ty + 9;

  tft.fillRect(0, ty - 9, SIDEBAR_W - 1, 41, C_BG);
  if (active) {
    tft.fillRoundRect(5, ty - 4, SIDEBAR_W - 11, 28, 8, C_SEL_BG);
    tft.drawRoundRect(5, ty - 4, SIDEBAR_W - 11, 28, 8, C_ACCENT);
    tft.fillRoundRect(1, ty + 3, 3, 14, 2, C_ACCENT);
    drawSidebarIconSized(i, centerX, centerY, true, iconSize);
  } else {
    tft.fillRoundRect(7, ty - 2, SIDEBAR_W - 15, 24, 7, C_SIDEBAR);
    drawSidebarIconSized(i, centerX, centerY, false, SIDEBAR_ICON_IDLE);
  }

  tft.drawFastVLine(SIDEBAR_W, 0, 240, C_BORDER);
  tft.drawFastVLine(SIDEBAR_W + 1, 0, 240, C_BG);
}

void animateSidebarSelection(int oldTab, int newTab, bool movedWindow) {
  if (SIDEBAR_W <= 0) return;
  const int frameDelay = 8;

  if (movedWindow) {
    drawSidebar();
    delay(frameDelay);
    drawSidebarSlot(newTab, true, SIDEBAR_ICON_STEP1);
    delay(frameDelay);
    drawSidebarSlot(newTab, true, SIDEBAR_ICON_STEP2);
    delay(frameDelay);
    drawSidebarSlot(newTab, true, SIDEBAR_ICON_STEP3);
    delay(frameDelay);
    drawSidebarSlot(newTab, true, SIDEBAR_ICON_STEP4);
    delay(frameDelay);
    drawSidebarSlot(newTab, true, SIDEBAR_ICON_LARGE);
    return;
  }

  drawSidebarSlot(oldTab, true, SIDEBAR_ICON_STEP4);
  drawSidebarSlot(newTab, true, SIDEBAR_ICON_STEP1);
  delay(frameDelay);
  drawSidebarSlot(oldTab, true, SIDEBAR_ICON_STEP3);
  drawSidebarSlot(newTab, true, SIDEBAR_ICON_STEP2);
  delay(frameDelay);
  drawSidebarSlot(oldTab, true, SIDEBAR_ICON_STEP2);
  drawSidebarSlot(newTab, true, SIDEBAR_ICON_STEP3);
  delay(frameDelay);
  drawSidebarSlot(oldTab, true, SIDEBAR_ICON_STEP1);
  drawSidebarSlot(newTab, true, SIDEBAR_ICON_STEP4);
  delay(frameDelay);
  drawSidebarSlot(oldTab, false, SIDEBAR_ICON_IDLE);
  drawSidebarSlot(newTab, true, SIDEBAR_ICON_LARGE);
}

void drawSidebar() {
  if (SIDEBAR_W <= 0) return;
  tft.fillRect(0, 0, SIDEBAR_W, 240, C_BG);
  tft.fillRoundRect(4, 5, SIDEBAR_W - 9, 230, 12, C_SIDEBAR);
  ensureSidebarVisible();

  int visibleCount = SIDEBAR_TABS < SIDEBAR_VISIBLE_TABS ? SIDEBAR_TABS : SIDEBAR_VISIBLE_TABS;
  for (int slot = 0; slot < visibleCount; slot++) {
    int i = sidebarTopTab + slot;
    drawSidebarSlot(i, i == sidebarTab, SIDEBAR_ICON_LARGE);
  }

  drawSidebarScrollbar();
}

void clearContentArea() {
  tft.fillRect(CONTENT_X, 0, 320 - CONTENT_X, 240, C_BG);
}

// Draw card border (rounded rect with teal border)
void drawCardBorder(int x, int y, int w, int h) {
  tft.fillRoundRect(x, y, w, h, 7, C_BG);
  tft.drawRoundRect(x, y, w, h, 7, C_BORDER);
  tft.drawFastHLine(x + 10, y + 24, w - 20, C_NAV_SLOT);
}

void drawStatusRow(int x, int y, int icon, String label, String value, uint16_t valueCol) {
  tft.fillRect(x, y, PANEL_W - PANEL_INSET * 2, 20, C_CARD_BG);
  uint16_t iconCol = valueCol == C_ERROR ? C_ERROR : C_MUTED;
  if (icon == 0) drawIconMsg(x, y + 2, iconCol);
  else if (icon == 1) drawIconNodes(x, y + 2, iconCol);
  else if (icon == 2) drawIconSignal(x, y + 2, iconCol);
  else if (icon == 3) drawIconSd(x, y + 2, iconCol);
  else drawIconInfo(x, y + 2, iconCol);
  tft.setTextSize(1);
  tft.setTextColor(C_MUTED);
  tft.setCursor(x + 22, y);
  tft.print(label);
  tft.setTextColor(valueCol);
  tft.setCursor(x + 22, y + 10);
  tft.print(value);
}

uint16_t colorIf(bool condition, uint16_t trueColor, uint16_t falseColor) {
  return condition ? trueColor : falseColor;
}

void drawMenuRow(int i, int selectedIndexValue, String label, String value, int icon) {
  int y = BODY_Y + 12 + i * 30;
  int x = PANEL_X + PANEL_INSET;
  int w = PANEL_W - PANEL_INSET * 2;
  bool selected = (i == selectedIndexValue);

  // if (selected) {
  //   tft.fillRoundRect(x, y, w, 26, 3, C_CARD_BG);
  //   tft.drawRoundRect(x, y, w, 26, 3, C_ACCENT);
  // } else {
  //   tft.fillRoundRect(x, y, w, 26, 3, C_CARD_BG);
  // }

  uint16_t iconCol = selected ? C_ACCENT : C_MUTED;
  if (icon == 0) drawIconConfig(x + 8, y + 6, iconCol);
  else if (icon == 1) drawIconSignal(x + 8, y + 6, iconCol);
  else if (icon == 2) drawIconSun(x + 8, y + 6, iconCol);
  else if (icon == 3) drawIconInfo(x + 8, y + 6, iconCol);
  else drawIconSettings(x + 8, y + 6, iconCol);

  tft.setTextSize(1);
  tft.setTextColor(selected ? C_ACCENT : C_TEXT);
  tft.setCursor(x + 30, y + 4);
  tft.print(label);
  tft.setTextColor(C_MUTED);
  int vx = x + w - value.length() * 6 - 8;
  if (vx < x + 130) vx = x + 130;
  tft.setCursor(vx, y + 14);
  tft.print(value);
}

String getConfigValue(int i) {
  if (i == 0) return String(configModes[configModeIndex]);
  if (i == 1) return String(configPowers[configPowerIndex]);
  return "";
}

String getSettingsValue(int i) {
  if (i == 0) return String(brightnessPercent) + "%";
  if (i == 1) return alertSoundOn ? "ON" : "OFF";
  if (i == 2) return String(sleepModes[sleepModeIndex]);
  if (i == 3) return wifiApEnabled ? (wifiApReady ? "ON" : "START") : "OFF";
  if (i == 4) return String(wifiApPassword.length()) + " chars";
#if ENABLE_BLE_NETWORK
  if (i == 5) return bluetoothEnabled ? (bluetoothReady ? "ON" : "START") : "OFF";
#else
  if (i == 5) return "Use app3M";
#endif
  return "";
}

byte getE22PowerValue() {
  return E22_TX_POWER;
}

String radioFrequencyText() {
  return String(850 + CHANNEL) + " MHz";
}

String toHex2(uint8_t value) {
  char buf[3];
  snprintf(buf, sizeof(buf), "%02X", value);
  return String(buf);
}

String toHex4(uint16_t value) {
  char buf[5];
  snprintf(buf, sizeof(buf), "%04X", value);
  return String(buf);
}

String networkDeviceName() {
  return String("Lomhor-") + toHex2(MY_ADDH) + toHex2(MY_ADDL);
}

String wifiApSsid() {
  return networkDeviceName() + "-AP";
}

void initWifiAccessPoint() {
  if (!wifiApEnabled) {
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    wifiApReady = false;
    Serial.println("WiFi AP disabled");
    return;
  }

  WiFi.mode(WIFI_AP);
  String ssid = wifiApSsid();
  wifiApReady = WiFi.softAP(ssid.c_str(), wifiApPassword.c_str());
  Serial.print("WiFi AP ");
  Serial.print(wifiApReady ? "ready: " : "failed: ");
  Serial.println(ssid);
  if (wifiApReady) {
    Serial.print("WiFi AP IP: ");
    Serial.println(WiFi.softAPIP());
  }
}

void initBluetoothBle() {
#if ENABLE_BLE_NETWORK
  if (!bluetoothEnabled) {
    if (bluetoothReady) NimBLEDevice::deinit(true);
    bluetoothReady = false;
    Serial.println("Bluetooth BLE disabled");
    return;
  }

  String name = networkDeviceName();
  NimBLEDevice::init(name.c_str());
  NimBLEServer *server = NimBLEDevice::createServer();
  NimBLEService *service = server->createService(BLE_SERVICE_UUID);
  service->start();

  NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();
  advertising->setName(name.c_str());
  advertising->addServiceUUID(BLE_SERVICE_UUID);
  advertising->enableScanResponse(true);
  advertising->start();
  bluetoothReady = true;

  Serial.print("Bluetooth BLE advertising: ");
  Serial.println(name);
#else
  bluetoothReady = false;
  bluetoothEnabled = false;
  Serial.println("Bluetooth BLE disabled at compile time; use app3M partition and set ENABLE_BLE_NETWORK to 1.");
#endif
}

void initNetworkDevices() {
  initWifiAccessPoint();
  initBluetoothBle();
}

int parseHexByte(String text) {
  text.trim();
  if (text.length() == 0 || text.length() > 2) return -1;
  char *endPtr = nullptr;
  long value = strtol(text.c_str(), &endPtr, 16);
  if (*endPtr != '\0' || value < 0 || value > 0xFF) return -1;
  return (int)value;
}

int parseHexWord(String text) {
  text.trim();
  if (text.length() == 0 || text.length() > 4) return -1;
  char *endPtr = nullptr;
  long value = strtol(text.c_str(), &endPtr, 16);
  if (*endPtr != '\0' || value < 0 || value > 0xFFFF) return -1;
  return (int)value;
}

int8_t hexNibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

String radioUnescapeField(const String &text) {
  String out;
  out.reserve(text.length());
  for (size_t i = 0; i < text.length(); i++) {
    char c = text.charAt(i);
    if (c == '%' && i + 2 < text.length()) {
      int8_t hi = hexNibble(text.charAt(i + 1));
      int8_t lo = hexNibble(text.charAt(i + 2));
      if (hi >= 0 && lo >= 0) {
        out += (char)((hi << 4) | lo);
        i += 2;
        continue;
      }
    }
    out += c;
  }
  return out;
}

bool radioBase64DecodeUtf8(const String &text, String &out) {
  size_t outLen = (text.length() * 3) / 4 + 4;
  unsigned char *buf = (unsigned char*) malloc(outLen);
  if (!buf) return false;

  size_t olen = 0;
  int rc = mbedtls_base64_decode(buf, outLen, &olen, (const unsigned char*) text.c_str(), text.length());
  if (rc == 0) {
    buf[olen] = '\0';
    out = String((const char*) buf);
  }
  free(buf);
  return rc == 0;
}

bool isValidUtf8(const String &text) {
  const uint8_t *bytes = (const uint8_t*) text.c_str();
  size_t i = 0;
  size_t len = text.length();

  while (i < len) {
    uint8_t c = bytes[i++];
    if (c <= 0x7F) continue;

    uint8_t continuationCount = 0;
    uint32_t codePoint = 0;
    if ((c & 0xE0) == 0xC0) {
      continuationCount = 1;
      codePoint = c & 0x1F;
      if (codePoint == 0) return false;
    } else if ((c & 0xF0) == 0xE0) {
      continuationCount = 2;
      codePoint = c & 0x0F;
    } else if ((c & 0xF8) == 0xF0) {
      continuationCount = 3;
      codePoint = c & 0x07;
    } else {
      return false;
    }

    if (i + continuationCount > len) return false;
    for (uint8_t n = 0; n < continuationCount; n++) {
      uint8_t next = bytes[i++];
      if ((next & 0xC0) != 0x80) return false;
      codePoint = (codePoint << 6) | (next & 0x3F);
    }

    if ((continuationCount == 1 && codePoint < 0x80) ||
        (continuationCount == 2 && codePoint < 0x800) ||
        (continuationCount == 3 && codePoint < 0x10000) ||
        (codePoint >= 0xD800 && codePoint <= 0xDFFF) ||
        codePoint > 0x10FFFF) {
      return false;
    }
  }
  return true;
}

String structuredMessage(String fromAddr, String toAddr, String body) {
  return fromAddr + ">" + toAddr + " " + body;
}

String formatDateText(int year, int month, int day) {
  if (year < 2000 || year > 2099) year = 2000;
  if (month < 1 || month > 12) month = 1;
  if (day < 1 || day > 31) day = 1;

  char buf[11];
  snprintf(buf, sizeof(buf), "%04u-%02u-%02u", (unsigned)year, (unsigned)month, (unsigned)day);
  return String(buf);
}

String buildDateText() {
  const char* months = "JanFebMarAprMayJunJulAugSepOctNovDec";
  char monthText[4] = {__DATE__[0], __DATE__[1], __DATE__[2], '\0'};
  const char* found = strstr(months, monthText);
  int month = found ? ((found - months) / 3 + 1) : 1;
  int day = atoi(__DATE__ + 4);
  int year = atoi(__DATE__ + 7);
  return formatDateText(year, month, day);
}

String currentDateText() {
  time_t now = time(nullptr);
  struct tm timeInfo;
  if (now > 1704067200 && localtime_r(&now, &timeInfo)) {
    return formatDateText(timeInfo.tm_year + 1900, timeInfo.tm_mon + 1, timeInfo.tm_mday);
  }
  return buildDateText();
}

String currentLogTimeText() {
  time_t now = time(nullptr);
  struct tm timeInfo;
  if (now > 1704067200 && localtime_r(&now, &timeInfo)) {
    char buf[9];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec);
    return String(buf);
  }
  return String("uptime ") + String(millis() / 1000) + "s";
}

String currentHomeTimeText() {
  time_t now = time(nullptr);
  struct tm timeInfo;
  if (now > 1704067200 && localtime_r(&now, &timeInfo)) {
    char buf[6];
    snprintf(buf, sizeof(buf), "%02d:%02d", timeInfo.tm_hour, timeInfo.tm_min);
    return String(buf);
  }

  unsigned long seconds = millis() / 1000;
  unsigned long minutes = seconds / 60;
  char buf[8];
  snprintf(buf, sizeof(buf), "%02lu:%02lu", (minutes / 60) % 24, minutes % 60);
  return String(buf);
}

String currentLauncherDateText() {
  const char* monthNames[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
  time_t now = time(nullptr);
  struct tm timeInfo;
  if (now > 1704067200 && localtime_r(&now, &timeInfo)) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d.%s.%04d", timeInfo.tm_mday, monthNames[timeInfo.tm_mon], timeInfo.tm_year + 1900);
    return String(buf);
  }

  const char* months = "JanFebMarAprMayJunJulAugSepOctNovDec";
  char monthText[4] = {__DATE__[0], __DATE__[1], __DATE__[2], '\0'};
  const char* found = strstr(months, monthText);
  int month = found ? ((found - months) / 3) : 0;
  int day = atoi(__DATE__ + 4);
  int year = atoi(__DATE__ + 7);
  char buf[16];
  snprintf(buf, sizeof(buf), "%02d.%s.%04d", day, monthNames[month], year);
  return String(buf);
}

String uptimeText() {
  unsigned long minutes = millis() / 60000UL;
  unsigned long hours = minutes / 60UL;
  minutes %= 60UL;
  char buf[24];
  snprintf(buf, sizeof(buf), "up time: %02luh %02lum", hours, minutes);
  return String(buf);
}

String chatLogPathForToday() {
  return String(CHAT_LOG_DIR) + "/" + currentDateText() + ".txt";
}

String chatHistoryPowerText() {
  if (configPowerIndex < 0 || configPowerIndex > 3) return "E22 power";
  return String(configPowers[configPowerIndex]);
}

String nodeNameForHex(String addrHex) {
  addrHex.trim();
  addrHex.toUpperCase();
  if (addrHex == "ALL" || addrHex == "FFFF") return "All Broadcast";

  int addr = parseHexWord(addrHex);
  if (addr < 0) return "Node";

  uint8_t addh = (uint8_t)(((uint16_t)addr) >> 8);
  uint8_t addl = (uint8_t)(((uint16_t)addr) & 0xFF);
  if (addh == MY_ADDH && addl == MY_ADDL) return String(username);

  for (int i = 0; i < MAX_ONLINE_NODES; i++) {
    if (onlineNodes[i].used && onlineNodes[i].addh == addh && onlineNodes[i].addl == addl) {
      String name = onlineNodes[i].name;
      name.trim();
      if (name.length() > 0) return name;
    }
  }

  return "Node " + nodeIdText(addh, addl);
}

String namedEndpoint(String addrHex) {
  addrHex.trim();
  addrHex.toUpperCase();
  if (addrHex == "ALL" || addrHex == "FFFF") return "All Broadcast";

  int addr = parseHexWord(addrHex);
  if (addr < 0) return "Node:??:??";

  uint8_t addh = (uint8_t)(((uint16_t)addr) >> 8);
  uint8_t addl = (uint8_t)(((uint16_t)addr) & 0xFF);
  return nodeNameForHex(addrHex) + ":" + nodeIdText(addh, addl);
}

void appendChatHistory(String direction, String sourceHex, String targetHex, String body) {
  if (!sdReady) sdReady = initSDCard();
  if (!sdReady) return;

  prepareSdBus();
  if (!SD.exists(CHAT_LOG_DIR)) SD.mkdir(CHAT_LOG_DIR);

  File file = SD.open(chatLogPathForToday().c_str(), FILE_APPEND);
  if (!file) {
    Serial.println("Chat history open failed");
    return;
  }

  body.replace("\r", " ");
  body.replace("\n", " ");
  file.print(currentDateText());
  file.print(" ");
  file.print(currentLogTimeText());
  file.print(": ");
  file.print(chatHistoryPowerText());
  file.print(" (");
  file.print(direction);
  file.print(") ");
  file.print(namedEndpoint(sourceHex));
  file.print(">");
  file.print(namedEndpoint(targetHex));
  file.print(" ");
  file.println(body);
  file.close();
}

bool waitAuxHigh(unsigned long timeoutMs) {
  unsigned long started = millis();
  while (digitalRead(E22_AUX) == LOW) {
    if (millis() - started >= timeoutMs) return false;
    delay(2);
  }
  return true;
}

void clearE22SerialInput() {
  while (Serial2.available() > 0) Serial2.read();
}

void printE22PinReport() {
  Serial.println("E22 wiring for TFT sketch:");
  Serial.printf("  ESP32-S3 GPIO%d RX  <- E22 TXD\n", E22_RXD);
  Serial.printf("  ESP32-S3 GPIO%d TX  -> E22 RXD\n", E22_TXD);
  Serial.printf("  ESP32-S3 GPIO%d     <- E22 AUX\n", E22_AUX);
  Serial.printf("  ESP32-S3 GPIO%d     -> E22 M0\n", E22_M0);
  Serial.printf("  ESP32-S3 GPIO%d     -> E22 M1\n", E22_M1);
  Serial.println("  E22 VCC must use external high-current 5V, GND common with ESP32.");
}

void prepareE22Pins() {
  pinMode(E22_M0, OUTPUT);
  pinMode(E22_M1, OUTPUT);
  pinMode(E22_AUX, INPUT_PULLUP);

  // Wake/normal mode first so the module is not left in sleep/program mode.
  digitalWrite(E22_M0, LOW);
  digitalWrite(E22_M1, LOW);
  delay(80);
  waitAuxHigh(1000);
}

int16_t e22RssiByteToDbm(uint8_t rawRssi) {
  return (int16_t)rawRssi - 256;
}

String nodeIdText(uint8_t addh, uint8_t addl) {
  return toHex2(addh) + ":" + toHex2(addl);
}

String nodeDisplayName(uint8_t addh, uint8_t addl, String name) {
  name.trim();
  if (name.length() == 0) name = "Node " + nodeIdText(addh, addl);
  return name;
}

void markNodeOnline(uint8_t addh, uint8_t addl, uint8_t netId, uint8_t channel, int16_t rssi, bool isRepeater, String name) {
  if (addh == MY_ADDH && addl == MY_ADDL) return;

  int emptySlot = -1;
  int oldestSlot = 0;
  unsigned long oldestSeen = ULONG_MAX;

  for (int i = 0; i < MAX_ONLINE_NODES; i++) {
    if (onlineNodes[i].used && onlineNodes[i].addh == addh && onlineNodes[i].addl == addl) {
      onlineNodes[i].netId = netId;
      onlineNodes[i].channel = channel;
      onlineNodes[i].rssi = rssi;
      onlineNodes[i].isRepeater = isRepeater;
      onlineNodes[i].name = nodeDisplayName(addh, addl, name);
      onlineNodes[i].lastSeen = millis();
      return;
    }
    if (!onlineNodes[i].used && emptySlot < 0) emptySlot = i;
    if (onlineNodes[i].used && onlineNodes[i].lastSeen < oldestSeen) {
      oldestSeen = onlineNodes[i].lastSeen;
      oldestSlot = i;
    }
  }

  int slot = emptySlot >= 0 ? emptySlot : oldestSlot;
  onlineNodes[slot].addh = addh;
  onlineNodes[slot].addl = addl;
  onlineNodes[slot].netId = netId;
  onlineNodes[slot].channel = channel;
  onlineNodes[slot].rssi = rssi;
  onlineNodes[slot].isRepeater = isRepeater;
  onlineNodes[slot].name = nodeDisplayName(addh, addl, name);
  onlineNodes[slot].lastSeen = millis();
  onlineNodes[slot].used = true;
}

int onlinePeerCount() {
  int count = 0;
  unsigned long now = millis();
  for (int i = 0; i < MAX_ONLINE_NODES; i++) {
    if (onlineNodes[i].used && now - onlineNodes[i].lastSeen <= NODE_ONLINE_TIMEOUT_MS) count++;
  }
  return count;
}

String onlineNodesText() {
  int peers = onlinePeerCount();
  String text = String(peers) + " nodes";
  if (peers == 0) text += " / none close";
  else text += " online";
  return text;
}

bool isOnlineNodeSlot(int slot) {
  if (slot < 0 || slot >= MAX_ONLINE_NODES) return false;
  if (!onlineNodes[slot].used) return false;
  return millis() - onlineNodes[slot].lastSeen <= NODE_ONLINE_TIMEOUT_MS;
}

int sendTargetCount() {
  return 1 + onlinePeerCount();
}

int onlineSlotForTargetIndex(int targetIndex) {
  if (targetIndex <= 0) return -1;

  int seen = 0;
  for (int i = 0; i < MAX_ONLINE_NODES; i++) {
    if (!isOnlineNodeSlot(i)) continue;
    seen++;
    if (seen == targetIndex) return i;
  }
  return -1;
}

int bestOnlineNodeSlot() {
  int bestSlot = -1;
  int16_t bestRssi = -32768;
  unsigned long newestSeen = 0;

  for (int i = 0; i < MAX_ONLINE_NODES; i++) {
    if (!isOnlineNodeSlot(i)) continue;
    if (bestSlot < 0 ||
        onlineNodes[i].rssi > bestRssi ||
        (onlineNodes[i].rssi == bestRssi && onlineNodes[i].lastSeen > newestSeen)) {
      bestSlot = i;
      bestRssi = onlineNodes[i].rssi;
      newestSeen = onlineNodes[i].lastSeen;
    }
  }

  return bestSlot;
}

String sendTargetLabel(int targetIndex) {
  if (targetIndex == 0) {
    return "All Broadcast FF:FF";
  }

  int slot = onlineSlotForTargetIndex(targetIndex);
  if (slot < 0) return "(offline)";

  String label = onlineNodes[slot].name;
  if (label.length() > 14) label = label.substring(0, 14);
  label += " ";
  label += nodeIdText(onlineNodes[slot].addh, onlineNodes[slot].addl);
  return label;
}

void selectSendTargetAddress() {
  if (sendTargetIndex == 0) {
    TARGET_ADDH = 0xFF;
    TARGET_ADDL = 0xFF;
    return;
  }

  int slot = onlineSlotForTargetIndex(sendTargetIndex);
  if (slot < 0) return;

  TARGET_ADDH = onlineNodes[slot].addh;
  TARGET_ADDL = onlineNodes[slot].addl;
}

void ensureSendTargetVisible() {
  int count = sendTargetCount();
  if (sendTargetIndex >= count) sendTargetIndex = count - 1;
  if (sendTargetIndex < 0) sendTargetIndex = 0;
  if (sendTargetIndex < sendTargetTopIndex) sendTargetTopIndex = sendTargetIndex;
  if (sendTargetIndex >= sendTargetTopIndex + SEND_TARGET_VISIBLE) {
    sendTargetTopIndex = sendTargetIndex - SEND_TARGET_VISIBLE + 1;
  }
  if (sendTargetTopIndex < 0) sendTargetTopIndex = 0;
  int maxTop = count - SEND_TARGET_VISIBLE;
  if (maxTop < 0) maxTop = 0;
  if (sendTargetTopIndex > maxTop) sendTargetTopIndex = maxTop;
}

String chatPayload(String msgId, String message) {
  String srcHex = toHex4((((uint16_t)MY_ADDH) << 8) | MY_ADDL);
  return "MSG|" + msgId + "|" + srcHex + "|" + message;
}

String helloPacket() {
  String packet = "HELLO|";
  packet += toHex4((((uint16_t)MY_ADDH) << 8) | MY_ADDL);
  packet += "|";
  packet += toHex2(NETWORK_ID);
  packet += "|";
  packet += toHex2(CHANNEL);
  packet += "|";
  packet += USE_REPEATER ? "R" : "N";
  packet += "|";
  String name = username;
  name.replace("|", "");
  packet += name;
  return packet;
}

bool handleHelloPacket(String packet, int16_t rssi) {
  if (!packet.startsWith("HELLO|")) return false;

  int p1 = packet.indexOf('|');
  int p2 = packet.indexOf('|', p1 + 1);
  int p3 = packet.indexOf('|', p2 + 1);
  int p4 = packet.indexOf('|', p3 + 1);
  if (p1 < 0 || p2 <= p1 || p3 <= p2 || p4 <= p3) return true;

  String addrHex = packet.substring(p1 + 1, p2);
  String netHex = packet.substring(p2 + 1, p3);
  String chHex = packet.substring(p3 + 1, p4);
  int p5 = packet.indexOf('|', p4 + 1);

  String role = p5 < 0 ? packet.substring(p4 + 1) : packet.substring(p4 + 1, p5);
  String name = p5 < 0 ? "" : packet.substring(p5 + 1);
  int next = name.indexOf('|');
  if (next >= 0) name = name.substring(0, next);
  role.trim();
  name.trim();

  int addr = parseHexWord(addrHex);
  int net = parseHexByte(netHex);
  int channel = parseHexByte(chHex);
  if (addr < 0 || net < 0 || channel < 0) return true;

  uint8_t addh = (uint8_t)(((uint16_t)addr) >> 8);
  uint8_t addl = (uint8_t)(((uint16_t)addr) & 0xFF);
  markNodeOnline(addh, addl, (uint8_t)net, (uint8_t)channel, rssi, role.startsWith("R"), name);
  lastRxMessage = "Seen " + nodeDisplayName(addh, addl, name);
  return true;
}

void sendChatAck(String msgId, String srcHex, bool viaRepeater) {
  int srcAddr = parseHexWord(srcHex);
  if (srcAddr < 0) return;

  String ackPayload = "ACK|" + msgId;
  if (viaRepeater) {
    String relayAck = "RELAY|" + srcHex + "|" + ackPayload;
    e22ttl.sendFixedMessage(REPEATER_ADDH, REPEATER_ADDL, CHANNEL, relayAck);
    return;
  }

  uint8_t srcAddh = (uint8_t)(((uint16_t)srcAddr) >> 8);
  uint8_t srcAddl = (uint8_t)(((uint16_t)srcAddr) & 0xFF);
  e22ttl.sendFixedMessage(srcAddh, srcAddl, CHANNEL, ackPayload);
}

bool handleAckPacket(String packet) {
  if (!packet.startsWith("ACK|")) return false;

  String ackId = packet.substring(4);
  int sep = ackId.indexOf('|');
  if (sep >= 0) ackId = ackId.substring(0, sep);
  ackId.trim();

  if (awaitingAckId.length() > 0 && ackId == awaitingAckId) {
    awaitingAckReceived = true;
    e22StatusText = "ACK RX";
  }
  return true;
}

bool handleRelayPacket(String packet, int16_t rssi) {
  if (!packet.startsWith("RELAY|")) return false;

  int p1 = packet.indexOf('|');
  int p2 = packet.indexOf('|', p1 + 1);
  if (p1 < 0 || p2 <= p1) return true;

  String destHex = packet.substring(p1 + 1, p2);
  String inner = packet.substring(p2 + 1);
  String myHex = toHex4((((uint16_t)MY_ADDH) << 8) | MY_ADDL);

  if (destHex == "FFFF") {
    if (USE_REPEATER) {
      e22ttl.sendFixedMessage(0xFF, 0xFF, CHANNEL, inner);
    }
    handleLocalPacket(inner, rssi);
    return true;
  }

  if (destHex == myHex) {
    handleLocalPacket(inner, rssi);
    return true;
  }

  if (USE_REPEATER) {
    int destAddr = parseHexWord(destHex);
    if (destAddr >= 0) {
      uint8_t destAddh = (uint8_t)(((uint16_t)destAddr) >> 8);
      uint8_t destAddl = (uint8_t)(((uint16_t)destAddr) & 0xFF);
      e22ttl.sendFixedMessage(destAddh, destAddl, CHANNEL, inner);
    }
  }

  return true;
}

bool handleMsgPacket(String packet, int16_t rssi) {
  String myHex = toHex4((((uint16_t)MY_ADDH) << 8) | MY_ADDL);

  if (packet.startsWith("MSG3|")) {
    int p1 = packet.indexOf('|');
    int p2 = packet.indexOf('|', p1 + 1);
    int p3 = packet.indexOf('|', p2 + 1);
    int p4 = packet.indexOf('|', p3 + 1);
    if (p1 < 0 || p2 <= p1 || p3 <= p2 || p4 <= p3) return true;

    String msgId = packet.substring(p1 + 1, p2);
    String fromHex = packet.substring(p2 + 1, p3);
    String toHex = packet.substring(p3 + 1, p4);
    String body64 = packet.substring(p4 + 1);
    String body;
    if (!radioBase64DecodeUtf8(body64, body) || !isValidUtf8(body)) body = "[invalid UTF-8 message]";

    int srcAddr = parseHexWord(fromHex);
    if (srcAddr >= 0) {
      uint8_t srcAddh = (uint8_t)(((uint16_t)srcAddr) >> 8);
      uint8_t srcAddl = (uint8_t)(((uint16_t)srcAddr) & 0xFF);
      markNodeOnline(srcAddh, srcAddl, NETWORK_ID, CHANNEL, rssi, false, "Node " + nodeIdText(srcAddh, srcAddl));
    }

    sendChatAck(msgId, fromHex, msgId.endsWith("R"));
    lastRxMessage = structuredMessage(fromHex, toHex, body);
    appendChatHistory("RX", fromHex, toHex, body);
    playReceivedMessageSound(body);
    return true;
  }

  if (packet.startsWith("MSG2|")) {
    int p1 = packet.indexOf('|');
    int p2 = packet.indexOf('|', p1 + 1);
    int p3 = packet.indexOf('|', p2 + 1);
    int p4 = packet.indexOf('|', p3 + 1);
    if (p1 < 0 || p2 <= p1 || p3 <= p2 || p4 <= p3) return true;

    String msgId = packet.substring(p1 + 1, p2);
    String fromHex = packet.substring(p2 + 1, p3);
    String toHex = packet.substring(p3 + 1, p4);
    String body = radioUnescapeField(packet.substring(p4 + 1));
    if (!isValidUtf8(body)) body = "[invalid UTF-8 message]";

    int srcAddr = parseHexWord(fromHex);
    if (srcAddr >= 0) {
      uint8_t srcAddh = (uint8_t)(((uint16_t)srcAddr) >> 8);
      uint8_t srcAddl = (uint8_t)(((uint16_t)srcAddr) & 0xFF);
      markNodeOnline(srcAddh, srcAddl, NETWORK_ID, CHANNEL, rssi, false, "Node " + nodeIdText(srcAddh, srcAddl));
    }

    sendChatAck(msgId, fromHex, false);
    lastRxMessage = structuredMessage(fromHex, toHex, body);
    appendChatHistory("RX", fromHex, toHex, body);
    playReceivedMessageSound(body);
    return true;
  }

  if (!packet.startsWith("MSG|")) return false;

  int p1 = packet.indexOf('|');
  int p2 = packet.indexOf('|', p1 + 1);
  int p3 = packet.indexOf('|', p2 + 1);
  if (p1 < 0 || p2 <= p1 || p3 <= p2) return true;

  String msgId = packet.substring(p1 + 1, p2);
  String srcHex = packet.substring(p2 + 1, p3);
  String body = packet.substring(p3 + 1);
  int srcAddr = parseHexWord(srcHex);
  if (srcAddr >= 0) {
    uint8_t srcAddh = (uint8_t)(((uint16_t)srcAddr) >> 8);
    uint8_t srcAddl = (uint8_t)(((uint16_t)srcAddr) & 0xFF);
    markNodeOnline(srcAddh, srcAddl, NETWORK_ID, CHANNEL, rssi, false, "Node " + nodeIdText(srcAddh, srcAddl));
  }

  sendChatAck(msgId, srcHex, msgId.endsWith("R"));
  lastRxMessage = structuredMessage(srcHex, myHex, body);
  appendChatHistory("RX", srcHex, myHex, body);
  playReceivedMessageSound(body);
  return true;
}

bool handleLocalPacket(String packet, int16_t rssi) {
  if (handleHelloPacket(packet, rssi)) return true;
  if (handleAckPacket(packet)) return true;
  if (handleRelayPacket(packet, rssi)) return true;
  if (handleMsgPacket(packet, rssi)) return true;
  return false;
}

void sendNodeBeacon(bool force = false) {
  if (!e22Ready) return;
  if (!force && millis() - lastNodeBeacon < NODE_BEACON_INTERVAL_MS) return;
  lastNodeBeacon = millis();

  ResponseStatus rs = e22ttl.sendFixedMessage(0xFF, 0xFF, CHANNEL, helloPacket());
  if (rs.code != E22_SUCCESS) e22StatusText = rs.getResponseDescription();
}

void syncE22UiSettings() {
  USE_REPEATER = configModeIndex == 1;
  if (configPowerIndex == 0) E22_TX_POWER = POWER_30;
  else if (configPowerIndex == 1) E22_TX_POWER = POWER_27;
  else if (configPowerIndex == 2) E22_TX_POWER = POWER_24;
  else E22_TX_POWER = POWER_21;
}

bool applyE22Config() {
  if (!e22Ready) return false;
  syncE22UiSettings();
  clearE22SerialInput();
  if (!waitAuxHigh(8000)) {
    e22StatusText = "E22 AUX timeout";
    Serial.println("E22 config failed: AUX stayed LOW. Check AUX wiring/power.");
    return false;
  }
  delay(100);

  ResponseStructContainer c = e22ttl.getConfiguration();
  if (c.status.code != E22_SUCCESS) {
    e22StatusText = c.status.getResponseDescription();
    Serial.print("E22 getConfiguration failed: ");
    Serial.println(e22StatusText);
    return false;
  }

  Configuration configuration = *(Configuration*) c.data;
  c.close();

  configuration.ADDH = MY_ADDH;
  configuration.ADDL = MY_ADDL;
  configuration.NETID = NETWORK_ID;
  configuration.CHAN = CHANNEL;
  configuration.SPED.uartParity = MODE_00_8N1;
  configuration.SPED.uartBaudRate = UART_BPS_9600;
  configuration.SPED.airDataRate = AIR_DATA_RATE_010_24;
  configuration.OPTION.subPacketSetting = SPS_240_00;
  configuration.OPTION.RSSIAmbientNoise = RSSI_AMBIENT_NOISE_DISABLED;
  configuration.OPTION.transmissionPower = getE22PowerValue();
  configuration.TRANSMISSION_MODE.fixedTransmission = FT_FIXED_TRANSMISSION;
  configuration.TRANSMISSION_MODE.enableRepeater = REPEATER_DISABLED;
  configuration.TRANSMISSION_MODE.enableRSSI = RSSI_ENABLED;
  configuration.TRANSMISSION_MODE.enableLBT = LBT_DISABLED;
  configuration.TRANSMISSION_MODE.WORTransceiverControl = WOR_RECEIVER;
  configuration.TRANSMISSION_MODE.WORPeriod = WOR_2000_011;
  configuration.CRYPT.CRYPT_H = highByte(CRYPT);
  configuration.CRYPT.CRYPT_L = lowByte(CRYPT);

  ResponseStatus rs = e22ttl.setConfiguration(configuration, WRITE_CFG_PWR_DWN_SAVE);
  e22StatusText = rs.getResponseDescription();
  delay(300);
  if (rs.code == E22_SUCCESS && !waitAuxHigh(5000)) {
    e22StatusText = "E22 save timeout";
    Serial.println("E22 config failed: save timeout after setConfiguration.");
    return false;
  }
  if (rs.code == E22_SUCCESS) {
    e22ttl.setMode(MODE_0_NORMAL);
    waitAuxHigh(1000);
    clearE22SerialInput();
    e22StatusText = String("Power ") + configPowers[configPowerIndex];
  }
  return rs.code == E22_SUCCESS;
}

void initE22Radio() {
  printE22PinReport();
  prepareE22Pins();
  Serial2.end();
  delay(50);

  e22Ready = e22ttl.begin();
  pinMode(E22_AUX, INPUT_PULLUP);
  if (!e22Ready) {
    e22StatusText = "E22 begin failed";
    Serial.println(e22StatusText);
    return;
  }
  bool configured = applyE22Config();
  if (!configured) {
    e22Ready = false;
    Serial.print("E22 offline: ");
    Serial.println(e22StatusText);
    return;
  }
  e22Ready = true;
  e22ttl.setMode(MODE_0_NORMAL);
  waitAuxHigh(1000);
  clearE22SerialInput();
  sendNodeBeacon(true);
  delay(80);
  sendNodeBeacon(true);
  Serial.print("E22 local channel: ");
  Serial.println(radioFrequencyText());
  Serial.print("E22 status: ");
  Serial.println(e22StatusText);
}

bool waitForAck(String msgId, unsigned long timeoutMs) {
  awaitingAckId = msgId;
  awaitingAckReceived = false;

  unsigned long started = millis();
  while (millis() - started < timeoutMs) {
    pollE22Radio(true);
    if (awaitingAckReceived) {
      awaitingAckId = "";
      return true;
    }
    delay(10);
  }

  awaitingAckId = "";
  return false;
}

bool sendPacketWithRetries(uint8_t addh, uint8_t addl, String packet, String label, bool waitAck, String ackId, bool &acked) {
  bool sent = false;
  acked = false;

  for (uint8_t attempt = 0; attempt <= ACK_RETRY_MAX; attempt++) {
    ResponseStatus rs = e22ttl.sendFixedMessage(addh, addl, CHANNEL, packet);
    e22StatusText = rs.getResponseDescription();
    Serial.print(label);
    Serial.print(attempt + 1);
    Serial.print(" ");
    Serial.println(e22StatusText);

    if (rs.code == E22_SUCCESS) {
      sent = true;
      if (waitAck) {
        acked = waitForAck(ackId, ACK_WAIT_MS);
        if (acked) break;
      } else {
        break;
      }
    }
  }

  return sent;
}

bool sendLocalMessage(String message) {
  message.trim();
  if (message.length() == 0) return false;
  if (message.length() > MAX_CHAT_BODY_BYTES) message = message.substring(0, MAX_CHAT_BODY_BYTES);

  if (!e22Ready) {
    e22StatusText = "E22 offline";
    return false;
  }

  uint8_t finalAddh = TARGET_ADDH;
  uint8_t finalAddl = TARGET_ADDL;
  bool broadcastTarget = finalAddh == 0xFF && finalAddl == 0xFF;

  if (broadcastTarget && onlinePeerCount() > 0) {
    if (!USE_REPEATER) {
      sendNodeBeacon(true);
      delay(80);
    }

    int sentNodes = 0;
    int ackedNodes = 0;
    int totalNodes = 0;
    String fromHex = toHex4((((uint16_t)MY_ADDH) << 8) | MY_ADDL);

    for (int i = 0; i < MAX_ONLINE_NODES; i++) {
      if (!isOnlineNodeSlot(i)) continue;
      totalNodes++;

      String msgId = toHex4(++txMessageCounter);
      if (USE_REPEATER) msgId += "R";
      String packet = chatPayload(msgId, message);
      uint8_t sendAddh = onlineNodes[i].addh;
      uint8_t sendAddl = onlineNodes[i].addl;
      String targetHex = toHex4((((uint16_t)onlineNodes[i].addh) << 8) | onlineNodes[i].addl);

      if (USE_REPEATER) {
        sendAddh = REPEATER_ADDH;
        sendAddl = REPEATER_ADDL;
        packet = "RELAY|" + targetHex + "|" + packet;
      }

      bool acked = false;
      bool sent = sendPacketWithRetries(sendAddh, sendAddl, packet, USE_REPEATER ? "E22 TX all relay try " : "E22 TX all direct try ", true, msgId, acked);
      if (sent) sentNodes++;
      if (acked) ackedNodes++;
      delay(60);
    }

    if (sentNodes > 0) {
      sentCount++;
      lastTxMessage = structuredMessage(fromHex, "ALL", message);
      appendChatHistory("TX", fromHex, "ALL", message);
      e22StatusText = String("SENT ALL ") + String(sentNodes) + "/" + String(totalNodes);
      if (ackedNodes > 0) e22StatusText += " ACK " + String(ackedNodes);
      Serial.println(e22StatusText);
      return true;
    }

    e22StatusText = "ALL SEND FAILED";
    Serial.println(e22StatusText);
    return false;
  }

  String msgId = toHex4(++txMessageCounter);
  if (USE_REPEATER) msgId += "R";
  String packet = chatPayload(msgId, message);
  uint8_t sendAddh = finalAddh;
  uint8_t sendAddl = finalAddl;
  bool relaySend = USE_REPEATER;
  if (relaySend) {
    sendAddh = REPEATER_ADDH;
    sendAddl = REPEATER_ADDL;
    String destHex = broadcastTarget ? "FFFF" : toHex4((((uint16_t)finalAddh) << 8) | finalAddl);
    packet = "RELAY|" + destHex + "|" + packet;
  }

  if (!USE_REPEATER) {
    sendNodeBeacon(true);
    delay(80);
  }

  bool acked = false;
  bool sent = sendPacketWithRetries(
    sendAddh,
    sendAddl,
    packet,
    relaySend ? "E22 TX relay try " : (broadcastTarget ? "E22 TX broadcast try " : "E22 TX direct try "),
    !broadcastTarget,
    msgId,
    acked
  );

  if (sent && !acked && USE_REPEATER && !broadcastTarget) {
    Serial.println("E22 TX relay no ACK, trying direct target fallback");
    String directPacket = chatPayload(msgId, message);
    sent = sendPacketWithRetries(finalAddh, finalAddl, directPacket, "E22 TX direct try ", true, msgId, acked);
  }

  if (sent) {
    sentCount++;
    String toHex = toHex4((((uint16_t)finalAddh) << 8) | finalAddl);
    String fromHex = toHex4((((uint16_t)MY_ADDH) << 8) | MY_ADDL);
    lastTxMessage = structuredMessage(fromHex, toHex, message);
    appendChatHistory("TX", fromHex, toHex, message);
    e22StatusText = broadcastTarget ? "SENT BROADCAST" : (acked ? "SENT ACK" : "SENT NO ACK");
    Serial.print("E22 TX target ");
    Serial.print(toHex);
    Serial.print(" ");
    Serial.println(e22StatusText);
    return true;
  }

  Serial.print("E22 TX failed: ");
  Serial.println(e22StatusText);
  return false;
}

void showRadioReceiveStatus() {
  String shown = "RX: " + lastRxMessage;
  if (shown.length() > 32) shown = shown.substring(0, 32);

  if (screenMode == SCREEN_CHAT_PRESETS) {
    drawChatStatus(shown, C_ACCENT);
  } else if (screenMode == SCREEN_USER) {
    drawUserContent();
  } else if (screenMode == SCREEN_HOME && sidebarTab == 0) {
    drawHomeTopStatusIcons();
  } else if (screenMode == SCREEN_HOME_DETAILS) {
    drawHomeDetailsContent();
  } else if (screenMode == SCREEN_CHAT_SEND_TO) {
    drawSendTargetList();
  } else if (screenMode == SCREEN_MAP) {
    drawMapContent();
  }
}

void pollE22Radio(bool force) {
  if (!e22Ready) return;
  if (!force && millis() - lastRadioPoll < 80) return;
  lastRadioPoll = millis();

  while (e22ttl.available() > 1) {
    ResponseContainer rc = e22ttl.receiveMessageRSSI();
    if (rc.status.code == E22_SUCCESS) {
      String rxPacket = rc.data;
      rxPacket.trim();
      if (rxPacket.length() == 0) rxPacket = "(empty)";
      int16_t rssi = e22RssiByteToDbm(rc.rssi);
      lastRssiSignal = rssi;
      if (!rxPacket.startsWith("ACK|")) receivedCount++;
      if (!handleLocalPacket(rxPacket, rssi)) lastRxMessage = rxPacket;
      Serial.print("E22 RX: ");
      Serial.print(rssi);
      Serial.print(" dBm ");
      Serial.println(lastRxMessage);
      if (!displaySleeping) showRadioReceiveStatus();
    } else {
      e22StatusText = rc.status.getResponseDescription();
      Serial.print("E22 RX failed: ");
      Serial.println(e22StatusText);
    }
  }
}

// ===================== GPS =====================

String nmeaField(String sentence, int fieldIndex) {
  int start = sentence.charAt(0) == '$' ? 1 : 0;
  int current = 0;
  for (int i = start; i <= sentence.length(); i++) {
    char c = i < sentence.length() ? sentence.charAt(i) : ',';
    if (c == ',' || c == '*') {
      if (current == fieldIndex) return sentence.substring(start, i);
      current++;
      start = i + 1;
    }
  }
  return "";
}

bool isValidNmeaChecksum(String sentence) {
  sentence.trim();
  if (!sentence.startsWith("$")) return false;
  int star = sentence.indexOf('*');
  if (star < 0 || star + 2 >= sentence.length()) return true;

  uint8_t checksum = 0;
  for (int i = 1; i < star; i++) checksum ^= (uint8_t)sentence.charAt(i);
  String shown = sentence.substring(star + 1, star + 3);
  int expected = parseHexByte(shown);
  return expected >= 0 && checksum == (uint8_t)expected;
}

double parseNmeaCoordinate(String value, String hemisphere) {
  if (value.length() < 4) return 0.0;
  double raw = atof(value.c_str());
  int degrees = (int)(raw / 100.0);
  double minutes = raw - degrees * 100.0;
  double decimal = degrees + minutes / 60.0;
  if (hemisphere == "S" || hemisphere == "W") decimal = -decimal;
  return decimal;
}

String formatDecimalDegrees(double value) {
  char buf[18];
  snprintf(buf, sizeof(buf), "%.6f", value);
  return String(buf);
}

String formatGpsTime(String hhmmss) {
  if (hhmmss.length() < 6) return "--:--:--";
  return hhmmss.substring(0, 2) + ":" + hhmmss.substring(2, 4) + ":" + hhmmss.substring(4, 6);
}

String formatGpsDate(String ddmmyy) {
  if (ddmmyy.length() < 6) return "----/--/--";
  int year = 2000 + ddmmyy.substring(4, 6).toInt();
  return String(year) + "/" + ddmmyy.substring(2, 4) + "/" + ddmmyy.substring(0, 2);
}

String gpsFixText() {
  if (!gpsUartReady) return "UART off";
  if (millis() - lastGpsCharMs > 4000UL) return "No NMEA";
  if (gpsHasFix) return String("Fix ") + String(gpsSatellites) + " sat";
  if (gpsFixQuality > 0) return "Fix starting";
  return "Searching";
}

void parseGpsRmc(String sentence) {
  String status = nmeaField(sentence, 2);
  String latText = nmeaField(sentence, 3);
  String ns = nmeaField(sentence, 4);
  String lonText = nmeaField(sentence, 5);
  String ew = nmeaField(sentence, 6);
  String speedText = nmeaField(sentence, 7);
  String courseText = nmeaField(sentence, 8);
  String dateText = nmeaField(sentence, 9);

  gpsUtcTime = formatGpsTime(nmeaField(sentence, 1));
  if (dateText.length() >= 6) {
    gpsUtcDate = formatGpsDate(dateText);
    gpsDateValid = true;
  }

  gpsHasFix = status == "A";
  if (gpsHasFix && latText.length() > 0 && lonText.length() > 0) {
    gpsLatitude = parseNmeaCoordinate(latText, ns);
    gpsLongitude = parseNmeaCoordinate(lonText, ew);
    gpsSpeedKmh = atof(speedText.c_str()) * 1.852f;
    gpsCourseDeg = atof(courseText.c_str());
    lastGpsFixMs = millis();
    gpsStatusText = "GPS fix";
  } else if (!gpsHasFix) {
    gpsStatusText = "GPS searching";
  }
}

void parseGpsGga(String sentence) {
  String latText = nmeaField(sentence, 2);
  String ns = nmeaField(sentence, 3);
  String lonText = nmeaField(sentence, 4);
  String ew = nmeaField(sentence, 5);

  gpsUtcTime = formatGpsTime(nmeaField(sentence, 1));
  gpsFixQuality = (uint8_t)nmeaField(sentence, 6).toInt();
  gpsSatellites = (uint8_t)nmeaField(sentence, 7).toInt();
  gpsHdop = atof(nmeaField(sentence, 8).c_str());
  gpsAltitudeM = atof(nmeaField(sentence, 9).c_str());

  if (gpsFixQuality > 0 && latText.length() > 0 && lonText.length() > 0) {
    gpsHasFix = true;
    gpsLatitude = parseNmeaCoordinate(latText, ns);
    gpsLongitude = parseNmeaCoordinate(lonText, ew);
    lastGpsFixMs = millis();
    gpsStatusText = "GPS fix";
  }
}

void handleGpsSentence(String sentence) {
  sentence.trim();
  if (sentence.length() == 0) return;
  lastGpsSentence = sentence;

  if (!isValidNmeaChecksum(sentence)) {
    gpsStatusText = "NMEA checksum";
    return;
  }

  lastGpsSentenceMs = millis();
  String type = nmeaField(sentence, 0);
  if (type.endsWith("RMC")) parseGpsRmc(sentence);
  else if (type.endsWith("GGA")) parseGpsGga(sentence);
}

bool scanGpsI2cBus() {
  bool found = false;
  for (uint8_t address = 0x08; address <= 0x77; address++) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      found = true;
      break;
    }
  }
  return found;
}

void initGps() {
  Wire.begin(GPS_I2C_SDA, GPS_I2C_SCL);
  Wire.setClock(100000);
  gpsI2cReady = scanGpsI2cBus();

  gpsBaudIndex = 0;
  gpsActiveBaud = GPS_BAUD_CANDIDATES[gpsBaudIndex];
  gpsSerial.begin(gpsActiveBaud, SERIAL_8N1, GPS_UART_RXD, GPS_UART_TXD);
  gpsUartReady = true;
  lastGpsSentenceMs = millis();
  gpsStatusText = gpsI2cReady ? "GPS UART/I2C ready" : "GPS UART ready";

  Serial.println("GPS wiring:");
  Serial.printf("  UART RX GPIO%d <- GPS TX, TX GPIO%d -> GPS RX, baud %lu\n", GPS_UART_RXD, GPS_UART_TXD, (unsigned long)gpsActiveBaud);
  Serial.printf("  I2C SDA GPIO%d, SCL GPIO%d: %s\n", GPS_I2C_SDA, GPS_I2C_SCL, gpsI2cReady ? "device found" : "no ACK");
}

void retryGpsBaud() {
  if (GPS_BAUD_CANDIDATE_COUNT < 2) return;
  if (millis() - lastGpsSentenceMs < 7000UL) return;

  gpsBaudIndex = (gpsBaudIndex + 1) % GPS_BAUD_CANDIDATE_COUNT;
  gpsActiveBaud = GPS_BAUD_CANDIDATES[gpsBaudIndex];
  gpsSerial.end();
  delay(20);
  gpsSerial.begin(gpsActiveBaud, SERIAL_8N1, GPS_UART_RXD, GPS_UART_TXD);
  gpsLineBuffer = "";
  lastGpsCharMs = 0;
  lastGpsSentenceMs = millis();
  gpsStatusText = String("Trying ") + String(gpsActiveBaud);
  Serial.printf("GPS: no valid NMEA, trying baud %lu\n", (unsigned long)gpsActiveBaud);
}

void pollGps() {
  if (!gpsUartReady) return;
  retryGpsBaud();

  while (gpsSerial.available() > 0) {
    char c = (char)gpsSerial.read();
    lastGpsCharMs = millis();

    if (c == '$') {
      gpsLineBuffer = "$";
    } else if (c == '\n') {
      if (gpsLineBuffer.length() > 6) handleGpsSentence(gpsLineBuffer);
      gpsLineBuffer = "";
    } else if (c != '\r') {
      if (gpsLineBuffer.length() < 96) gpsLineBuffer += c;
      else gpsLineBuffer = "";
    }
  }
}

// ===================== HOME SCREEN =====================

String fitDashboardText(String text, int maxChars) {
  if (text.length() <= maxChars) return text;
  if (maxChars <= 2) return text.substring(0, maxChars);
  return text.substring(0, maxChars - 2) + "..";
}

void drawDashboardTile(int x, int y, int w, int h, int icon, String label, String value, uint16_t valueCol, bool wide = false) {
  uint16_t accentCol = valueCol == C_ERROR ? C_ERROR : (valueCol == C_WARN ? C_WARN : C_ACCENT);
  uint16_t fillCol = (y / 35) % 2 == 0 ? C_TILE_BG : C_PANEL;
  tft.fillRoundRect(x, y, w, h, 8, fillCol);
  tft.drawRoundRect(x, y, w, h, 8, accentCol == C_ERROR ? C_ERROR : C_BORDER);
  tft.fillRoundRect(x, y + 5, 4, h - 10, 3, accentCol);

  int iconBox = h - 10;
  int iconX = x + 11;
  int iconY = y + 5;
  tft.fillRoundRect(iconX, iconY, iconBox, iconBox, 7, C_BG);
  tft.drawRoundRect(iconX, iconY, iconBox, iconBox, 7, accentCol);
  int glyphX = iconX + 6;
  int glyphY = iconY + 5;
  if (icon == 0) drawIconMsg(glyphX, glyphY, accentCol);
  else if (icon == 1) drawIconNodes(glyphX, glyphY, accentCol);
  else if (icon == 2) drawIconSignal(glyphX, glyphY, accentCol);
  else if (icon == 3) drawIconSd(glyphX, glyphY, accentCol);
  else drawIconInfo(glyphX, glyphY, accentCol);

  int textX = x + 48;
  int maxChars = (w - 60) / 6;
  tft.setTextSize(1);
  tft.setTextColor(C_MUTED);
  tft.setCursor(textX, y + 6);
  tft.print(label);

  tft.setTextColor(valueCol);
  int valueY = y + h - 13;
  tft.setCursor(textX, valueY);
  tft.print(fitDashboardText(value, maxChars));
}

void drawHomeScreen() {
  screenMode = SCREEN_HOME;
  sidebarTab = 0;
  drawHomeStatus();
}

int homeAppTab(int appIndex) {
  if (appIndex < 0) return 0;
  if (appIndex >= HOME_APP_COUNT) return SIDEBAR_TABS - 1;
  return appIndex;
}

int homeAppIconTab(int appIndex) {
  if (appIndex == 8) return 8;
  return homeAppTab(appIndex);
}

String homeAppTitle(int appIndex) {
  if (appIndex == 0) return "Home";
  if (appIndex == 1) return "Nodes";
  if (appIndex == 2) return "Messages";
  if (appIndex == 3) return "Maps";
  if (appIndex == 4) return "Location";
  if (appIndex == 5) return "SD Card";
  if (appIndex == 6) return "Config";
  if (appIndex == 7) return "Settings";
  if (appIndex == 8) return "About";
  return "Restart";
}

String homeAppSubtitle(int appIndex) {
  if (appIndex == 0) return "Menu";
  if (appIndex == 1) return onlineNodesText();
  if (appIndex == 2) return "Messages";
  if (appIndex == 3) return "Nearby";
  if (appIndex == 4) return gpsFixText();
  if (appIndex == 5) return sdReady ? "SD ready" : "No SD";
  if (appIndex == 6) return String(configModes[configModeIndex]);
  if (appIndex == 7) return String(brightnessPercent) + "%";
  if (appIndex == 8) return String("Lomhor Chat ") + appVersion;
  return "Reboot";
}

uint16_t homeAppAccentColor(int appIndex) {
  if (appIndex == 0) return C_ACCENT;
  if (appIndex == 1) return C_GREEN;
  if (appIndex == 2) return C_CYAN;
  if (appIndex == 3) return C_RED_SOFT;
  if (appIndex == 4) return C_ACCENT;
  if (appIndex == 5) return C_YELLOW;
  if (appIndex == 6) return 0x7C3F;
  if (appIndex == 7) return 0xFD20;
  if (appIndex == 8) return C_ACCENT;
  return C_ERROR;
}

void drawLauncherGlyph(int appIndex, int x, int y, uint16_t col) {
  if (appIndex == 0) {
    tft.fillTriangle(x + 5, y + 18, x + 20, y + 5, x + 35, y + 18, col);
    tft.fillRoundRect(x + 10, y + 17, 20, 18, 3, col);
    tft.fillRoundRect(x + 17, y + 25, 6, 10, 3, C_ACCENT);
    tft.fillRect(x + 8, y + 18, 24, 3, col);
  } else if (appIndex == 1) {
    tft.fillCircle(x + 20, y + 11, 8, col);
    tft.fillCircle(x + 13, y + 28, 7, col);
    tft.fillCircle(x + 27, y + 28, 7, col);
    tft.fillRoundRect(x + 9, y + 22, 22, 12, 6, col);
  } else if (appIndex == 2) {
    tft.fillRoundRect(x + 4, y + 7, 32, 22, 11, col);
    tft.fillTriangle(x + 11, y + 26, x + 7, y + 36, x + 20, y + 28, col);
    tft.fillCircle(x + 14, y + 18, 3, C_ACCENT);
    tft.fillCircle(x + 21, y + 18, 3, C_ACCENT);
    tft.fillCircle(x + 28, y + 18, 3, C_ACCENT);
  } else if (appIndex == 4) {
    tft.fillCircle(x + 20, y + 16, 15, col);
    tft.fillTriangle(x + 20, y + 40, x + 7, y + 22, x + 33, y + 22, col);
    tft.fillCircle(x + 20, y + 16, 6, C_ACCENT);
  } else if (appIndex == 5) {
    tft.fillRoundRect(x + 11, y + 5, 20, 30, 2, col);
    tft.fillTriangle(x + 24, y + 5, x + 31, y + 12, x + 24, y + 12, C_ACCENT);
    tft.fillRect(x + 15, y + 27, 12, 3, C_ACCENT);
    tft.fillRect(x + 15, y + 10, 3, 7, C_ACCENT);
    tft.fillRect(x + 20, y + 10, 3, 7, C_ACCENT);
    tft.fillRect(x + 25, y + 10, 3, 7, C_ACCENT);
  } else if (appIndex == 6) {
    tft.fillCircle(x + 18, y + 18, 16, 0xD6BA);
    tft.fillTriangle(x + 5, y + 9, x + 34, y + 23, x + 10, y + 31, 0xC618);
    tft.fillRect(x + 17, y + 25, 5, 10, C_MUTED);
    tft.drawLine(x + 23, y + 13, x + 34, y + 3, C_MUTED);
    tft.fillCircle(x + 34, y + 3, 4, C_ERROR);
    tft.fillRoundRect(x + 7, y + 35, 26, 4, 2, C_NAV_SLOT);
  } else if (appIndex == 7) {
    tft.fillCircle(x + 20, y + 20, 15, col);
    for (int i = 0; i < 8; i++) {
      float a = i * PI / 4.0f;
      int sx = x + 20 + cos(a) * 14;
      int sy = y + 20 + sin(a) * 14;
      tft.fillRoundRect(sx - 3, sy - 3, 6, 6, 2, col);
    }
    tft.fillCircle(x + 20, y + 20, 7, C_ACCENT);
    tft.fillCircle(x + 20, y + 20, 4, col);
  } else if (appIndex == 8) {
    tft.drawRoundRect(x + 5, y + 8, 30, 25, 9, col);
    tft.drawRoundRect(x + 6, y + 9, 28, 23, 8, col);
    tft.drawRoundRect(x + 7, y + 10, 26, 21, 7, col);
    tft.drawLine(x + 17, y + 32, x + 22, y + 39, col);
    tft.drawLine(x + 18, y + 32, x + 23, y + 39, col);
    tft.drawLine(x + 28, y + 32, x + 23, y + 39, col);
    tft.drawLine(x + 29, y + 32, x + 24, y + 39, col);
    tft.fillCircle(x + 20, y + 17, 7, col);
    tft.fillCircle(x + 20, y + 17, 4, C_ACCENT);
    tft.fillRoundRect(x + 17, y + 20, 6, 9, 3, col);
    tft.fillRoundRect(x + 17, y + 32, 7, 5, 2, col);
  } else {
    tft.fillCircle(x + 20, y + 20, 17, col);
    tft.fillCircle(x + 20, y + 20, 11, C_RESTART_BG);
    tft.fillRect(x + 2, y + 14, 18, 13, C_RESTART_BG);
    tft.fillTriangle(x + 5, y + 17, x + 16, y + 8, x + 20, y + 21, col);
    tft.fillRect(x + 5, y + 14, 13, 9, col);
    tft.fillRect(x + 15, y + 11, 7, 8, C_RESTART_BG);
  }
}

void ensureHomeAppVisible() {
  if (homeAppIndex < 0) homeAppIndex = HOME_APP_COUNT - 1;
  if (homeAppIndex >= HOME_APP_COUNT) homeAppIndex = 0;

  if (HOME_APP_COUNT <= HOME_APP_VISIBLE) {
    homeAppTopIndex = 0;
    return;
  }

  if (homeAppIndex < homeAppTopIndex) {
    homeAppTopIndex = (homeAppIndex / HOME_APP_COLS) * HOME_APP_COLS;
  }
  if (homeAppIndex >= homeAppTopIndex + HOME_APP_VISIBLE) {
    int row = homeAppIndex / HOME_APP_COLS;
    homeAppTopIndex = (row - HOME_APP_ROWS + 1) * HOME_APP_COLS;
  }

  int maxTop = (((HOME_APP_COUNT - 1) / HOME_APP_COLS) - HOME_APP_ROWS + 1) * HOME_APP_COLS;
  if (maxTop < 0) maxTop = 0;
  if (homeAppTopIndex < 0) homeAppTopIndex = 0;
  if (homeAppTopIndex > maxTop) homeAppTopIndex = maxTop;
}

void drawHomeAppTile(int appIndex, int slotIndex) {
  int areaX = PANEL_X + PANEL_INSET;
  int areaY = BODY_Y + 74;
  int areaW = PANEL_W - PANEL_INSET * 2;
  int gap = 4;
  int tileW = (areaW - gap * (HOME_APP_COLS - 1)) / HOME_APP_COLS;
  int tileH = 58;
  int col = slotIndex % HOME_APP_COLS;
  int row = slotIndex / HOME_APP_COLS;
  int x = areaX + col * (tileW + gap);
  int y = areaY + row * (tileH + 10);
  bool selected = appIndex == homeAppIndex;
  uint16_t textCol = selected ? C_ACCENT : C_TEXT;
  String title = homeAppTitle(appIndex);
  int titleMax = (tileW - 8) / 6;
  if (title.length() > titleMax) title = title.substring(0, titleMax - 2) + "..";

  tft.fillRect(x - 2, y - 2, tileW + 4, tileH + 8, C_BG);
  int iconX = x + (tileW - LAUNCHER_ICON_SIZE) / 2;
  int bitmapIndex = appIndex;
  if (appIndex >= 5 && appIndex <= 8) bitmapIndex = appIndex - 1;
  if (appIndex == 9) bitmapIndex = 8;

  if (appIndex == 4) {
    tft.fillRoundRect(iconX, y, LAUNCHER_ICON_SIZE, LAUNCHER_ICON_SIZE, 9, C_ACCENT);
    drawLauncherGlyph(appIndex, iconX + 2, y + 2, C_TEXT);
  } else if (bitmapIndex >= 0 && bitmapIndex < 9) {
    tft.drawRGBBitmap(iconX, y, LAUNCHER_ICONS[bitmapIndex], LAUNCHER_ICON_SIZE, LAUNCHER_ICON_SIZE);
  } else {
    tft.fillRoundRect(iconX, y, LAUNCHER_ICON_SIZE, LAUNCHER_ICON_SIZE, 9, C_ACCENT);
    drawLauncherGlyph(appIndex, iconX + 2, y + 2, C_TEXT);
  }

  tft.setTextSize(1);
  tft.setTextColor(textCol);
  int titleX = x + (tileW - title.length() * 6) / 2;
  if (titleX < x + 4) titleX = x + 4;
  tft.setCursor(titleX, y + 47);
  tft.print(title);
  tft.setTextColor(C_MUTED);
}

void drawHomeAppScrollbar() {
  if (HOME_APP_COUNT <= HOME_APP_VISIBLE) return;

  int trackX = PANEL_X + PANEL_W - PANEL_INSET - 3;
  int trackY = BODY_Y + 74;
  int trackH = HOME_APP_ROWS * 58 + (HOME_APP_ROWS - 1) * 10;
  int totalRows = (HOME_APP_COUNT + HOME_APP_COLS - 1) / HOME_APP_COLS;
  int topRow = homeAppTopIndex / HOME_APP_COLS;
  int maxTopRow = totalRows - HOME_APP_ROWS;
  if (maxTopRow < 1) maxTopRow = 1;
  int thumbH = totalRows <= HOME_APP_ROWS ? trackH : trackH * HOME_APP_ROWS / totalRows;
  if (thumbH < 18) thumbH = 18;
  int thumbY = trackY + (trackH - thumbH) * topRow / maxTopRow;

  tft.fillRoundRect(trackX, trackY, 3, trackH, 2, C_NAV_SLOT);
  tft.fillRoundRect(trackX, thumbY, 3, thumbH, 2, C_ACCENT);
}

void drawHomeAppList() {
  ensureHomeAppVisible();
  tft.fillRect(PANEL_X + PANEL_INSET, BODY_Y + 70, PANEL_W - PANEL_INSET * 2, 150, C_BG);

  for (int slot = 0; slot < HOME_APP_VISIBLE; slot++) {
    int appIndex = homeAppTopIndex + slot;
    if (appIndex >= HOME_APP_COUNT) break;
    drawHomeAppTile(appIndex, slot);
  }
  drawHomeAppScrollbar();
}

int homeAppVisibleSlot(int appIndex) {
  int slot = appIndex - homeAppTopIndex;
  if (slot < 0 || slot >= HOME_APP_VISIBLE) return -1;
  return slot;
}

void drawHomeAppBlinkBorder(int slotIndex, uint16_t col) {
  if (slotIndex < 0) return;

  int areaX = PANEL_X + PANEL_INSET;
  int areaY = BODY_Y + 74;
  int areaW = PANEL_W - PANEL_INSET * 2;
  int gap = 4;
  int tileW = (areaW - gap * (HOME_APP_COLS - 1)) / HOME_APP_COLS;
  int tileH = 58;
  int colIndex = slotIndex % HOME_APP_COLS;
  int row = slotIndex / HOME_APP_COLS;
  int x = areaX + colIndex * (tileW + gap);
  int y = areaY + row * (tileH + 10);

  tft.drawRoundRect(x - 3, y - 4, tileW + 6, tileH + 10, 10, col);
  tft.drawRoundRect(x - 2, y - 3, tileW + 4, tileH + 8, 9, col);
}

void moveHomeAppSelection(int delta) {
  int oldIndex = homeAppIndex;
  int oldTop = homeAppTopIndex;
  homeAppIndex += delta;
  ensureHomeAppVisible();

  if (oldTop != homeAppTopIndex) {
    drawHomeAppList();
  } else {
    int oldSlot = homeAppVisibleSlot(oldIndex);
    if (oldSlot >= 0) drawHomeAppTile(oldIndex, oldSlot);
    int newSlot = homeAppVisibleSlot(homeAppIndex);
    if (newSlot >= 0) drawHomeAppTile(homeAppIndex, newSlot);
    drawHomeAppScrollbar();
  }
}

void drawHomeInfoPanel(int x, int y, int w, String title, String line1, String line2, String line3, uint16_t accentCol) {
  tft.fillRoundRect(x, y, w, 66, 8, C_CARD_BG);
  tft.drawRoundRect(x, y, w, 66, 8, C_CARD_BG);
  tft.fillRoundRect(x + 5, y + 5, 18, 18, 5, C_HEADER_BG);
  drawIconInfo(x + 7, y + 7, accentCol);

  tft.setTextSize(1);
  tft.setTextColor(accentCol);
  tft.setCursor(x + 29, y + 7);
  tft.print(title);

  tft.setTextColor(C_TEXT);
  tft.setCursor(x + 8, y + 28);
  tft.print(fitDashboardText(line1, (w - 16) / 6));
  tft.setTextColor(C_MUTED);
  tft.setCursor(x + 8, y + 40);
  tft.print(fitDashboardText(line2, (w - 16) / 6));
  tft.setCursor(x + 8, y + 52);
  tft.print(fitDashboardText(line3, (w - 16) / 6));
}

void drawHomeTopStatusIcons() {
  lastHomeTopStatusRefresh = millis();
  drawTopWifiIcon(226, 12, C_TEXT);
  drawTopGpsIcon(251, 12, gpsHasFix ? C_TEXT : C_ERROR);
  drawTopRadioIcon(273, 11, e22Ready ? C_TEXT : C_ERROR);
  uint16_t batteryMv = readBatteryMv();
  int battPercent = batteryPercentFromMv(batteryMv);
  drawTopBatteryIcon(302, 12, batteryColor(battPercent), battPercent);
}

void drawHomeClockValues() {
  lastHomeClockRefresh = millis();

  tft.setTextSize(2);
  tft.fillRect(14, 55, 180, 18, C_BG);
  tft.setTextColor(C_TEXT);
  tft.setCursor(14, 55);
  tft.print(currentLauncherDateText());

  tft.setTextSize(1);
  tft.fillRect(15, 78, 170, 10, C_BG);
  tft.setTextColor(C_TEXT);
  tft.setCursor(15, 78);
  tft.print(uptimeText());
}

void drawHomeHeaderBackground() {
  const uint16_t headerColor = 0x055E;
  const int headerY = 0;
  const int headerH = 45;
  const int wedgeStartX = 210;
  const int wedgeEndX = 235;
  const int wedgeOffsetX = 16;

  tft.fillScreen(C_BG);
  tft.fillRect(236, headerY, 84, headerH, headerColor);

  for (int x = wedgeStartX; x <= wedgeEndX; x++) {
    tft.drawLine(x, headerY, x + wedgeOffsetX, headerH - 2, headerColor);
    if (x > wedgeStartX) {
      tft.drawLine(x, headerY, x + wedgeOffsetX, headerH - 1, headerColor);
    }
  }
}



void drawHomeStatus() {
  drawHomeHeaderBackground();

  tft.setTextSize(3);
  tft.setTextColor(C_TEXT);
  tft.setCursor(14, 14);
  tft.print("LomhorOS");

  drawHomeTopStatusIcons();
  drawHomeClockValues();

  drawHomeAppList();
}

void drawHomeDetailsContent(bool fullRedraw) {
  int x = 8;
  int y = 6;
  int w = 304;
  String heapText = String("Heap ") + String(ESP.getFreeHeap() / 1024) + "k";
  String flashText = String("Flash ") + String(ESP.getFlashChipSize() / (1024 * 1024)) + "MB";
  String psramText = String("PSRAM ") + String(ESP.getPsramSize() / (1024 * 1024)) + "MB";
  String nodeText = String("Node ") + nodeIdText(MY_ADDH, MY_ADDL);
  String netText = String("NET ") + toHex2(NETWORK_ID) + " CH " + toHex2(CHANNEL);
  String keyText = String("KEY ") + toHex4(CRYPT);
  String radioText = e22Ready ? String(configPowers[configPowerIndex]) : "E22 offline";
  String rssiText = lastRssiSignal <= -250 ? "-- dBm" : String(lastRssiSignal) + " dBm";
  String trafficText = String(sentCount) + " TX / " + String(receivedCount) + " RX";
  String wifiText = wifiApEnabled ? (wifiApReady ? wifiApSsid() : "WiFi AP starting") : "WiFi AP off";
  String bleText = bluetoothEnabled ? (bluetoothReady ? networkDeviceName() : "BLE starting") : "BLE off";
  uint16_t batteryMv = readBatteryMv();
  int battPercent = batteryPercentFromMv(batteryMv);
  String batteryText = String(batteryMv / 1000.0f, 2) + "V  " + String(battPercent) + "%";
  String values[HOME_DETAILS_ROW_COUNT] = {
    HARDWARE_TARGET,
    heapText,
    flashText + "  " + psramText,
    String("Brightness ") + String(brightnessPercent) + "%",
    batteryText,
    String("Alert ") + (alertSoundOn ? "ON" : "OFF") + "  Sleep " + sleepModes[sleepModeIndex],
    nodeText,
    netText + "  " + radioFrequencyText(),
    keyText,
    radioText,
    String(configModes[configModeIndex]),
    rssiText + "  " + trafficText,
    wifiText,
    bleText,
    sdReady ? "SD ready" : "No SD",
    lastRxMessage
  };
  String labels[HOME_DETAILS_ROW_COUNT] = {
    "ESP32",
    "Memory",
    "Storage",
    "Display",
    "Battery",
    "Settings",
    "E22 Address",
    "Network",
    "Crypto",
    "Power",
    "Mode",
    "Signal",
    "WiFi AP",
    "Bluetooth",
    "SD Card",
    "Last RX"
  };
  int icons[HOME_DETAILS_ROW_COUNT] = {4, 4, 3, 4, 4, 4, 1, 2, 4, 2, 4, 2, 2, 2, 3, 0};
  uint16_t colors[HOME_DETAILS_ROW_COUNT] = {
    C_ACCENT, C_TEXT, C_TEXT, C_TEXT, (uint16_t)(battPercent <= 15 ? C_ERROR : (battPercent <= 30 ? C_WARN : C_ACCENT)), C_TEXT,
    C_ACCENT, C_ACCENT, C_MUTED, (uint16_t)(e22Ready ? C_ACCENT : C_ERROR),
    C_TEXT, (uint16_t)(lastRssiSignal <= -250 ? C_MUTED : C_ACCENT),
    (uint16_t)(wifiApReady ? C_CYAN : C_MUTED), (uint16_t)(bluetoothReady ? C_CYAN : C_MUTED),
    (uint16_t)(sdReady ? C_ACCENT : C_ERROR), C_TEXT
  };

  int maxTop = HOME_DETAILS_ROW_COUNT - HOME_DETAILS_VISIBLE_ROWS;
  if (maxTop < 0) maxTop = 0;
  if (homeDetailsTopIndex < 0) homeDetailsTopIndex = 0;
  if (homeDetailsTopIndex > maxTop) homeDetailsTopIndex = maxTop;

  int cardY = y + 36;
  int cardW = (w - 6) / 2;
  int listY = cardY + 38;
  int rowH = 20;
  int rowGap = 2;
  int listH = HOME_DETAILS_VISIBLE_ROWS * (rowH + rowGap) - rowGap;

  if (fullRedraw) {
    tft.fillRect(0, 0, 320, 240, C_BG);

    tft.fillRoundRect(x, y, w, 28, 7, C_HEADER_BG);
    tft.drawRoundRect(x, y, w, 28, 7, C_BORDER);
    tft.fillCircle(x + 10, y + 14, 3, e22Ready ? C_GREEN : C_ERROR);
    tft.fillCircle(x + 21, y + 14, 3, sdReady ? C_CYAN : C_MUTED);
    tft.fillCircle(x + 32, y + 14, 3, C_WARN);
    tft.setTextSize(1);
    tft.setTextColor(C_TEXT);
    tft.setCursor(x + 44, y + 7);
    tft.print("Home Details");
    tft.setTextColor(C_MUTED);
    tft.setCursor(x + 44, y + 18);
    tft.print(currentLauncherDateText());
    tft.setTextColor(C_ACCENT);
    tft.setCursor(x + w - 58, y + 11);
    tft.print("LEFT Back");

    tft.fillRoundRect(x, cardY, cardW, 30, 6, C_PANEL);
    tft.drawRoundRect(x, cardY, cardW, 30, 6, e22Ready ? C_GREEN : C_ERROR);
    drawIconSignal(x + 8, cardY + 8, e22Ready ? C_GREEN : C_ERROR);
    tft.setTextColor(C_MUTED);
    tft.setCursor(x + 30, cardY + 5);
    tft.print("Radio");
    tft.setTextColor(e22Ready ? C_GREEN : C_ERROR);
    tft.setCursor(x + 30, cardY + 17);
    tft.print(e22Ready ? radioFrequencyText() : "Offline");

    tft.fillRoundRect(x + cardW + 6, cardY, cardW, 30, 6, C_PANEL);
    tft.drawRoundRect(x + cardW + 6, cardY, cardW, 30, 6, C_CYAN);
    drawIconMsg(x + cardW + 14, cardY + 8, C_CYAN);
    tft.setTextColor(C_MUTED);
    tft.setCursor(x + cardW + 36, cardY + 5);
    tft.print("Traffic");
    tft.setTextColor(C_TEXT);
    tft.setCursor(x + cardW + 36, cardY + 17);
    tft.print(trafficText);

    tft.fillRoundRect(x, listY - 4, w, listH + 8, 7, C_PANEL);
    tft.drawRoundRect(x, listY - 4, w, listH + 8, 7, C_BORDER);
  } else {
    tft.fillRect(x + 4, listY - 1, w - 22, listH + 2, C_PANEL);
  }

  for (int slot = 0; slot < HOME_DETAILS_VISIBLE_ROWS; slot++) {
    int rowIndex = homeDetailsTopIndex + slot;
    if (rowIndex >= HOME_DETAILS_ROW_COUNT) break;
    int rowY = listY + slot * (rowH + rowGap);
    uint16_t rowBg = (slot % 2 == 0) ? C_TILE_BG : C_CARD_BG;
    tft.fillRoundRect(x + 5, rowY, w - 17, rowH, 4, rowBg);
    tft.fillRoundRect(x + 5, rowY + 4, 3, rowH - 8, 2, colors[rowIndex]);

    uint16_t iconCol = colors[rowIndex] == C_ERROR ? C_ERROR : C_MUTED;
    if (icons[rowIndex] == 0) drawIconMsg(x + 13, rowY + 3, iconCol);
    else if (icons[rowIndex] == 1) drawIconNodes(x + 13, rowY + 3, iconCol);
    else if (icons[rowIndex] == 2) drawIconSignal(x + 13, rowY + 3, iconCol);
    else if (icons[rowIndex] == 3) drawIconSd(x + 13, rowY + 3, iconCol);
    else drawIconInfo(x + 13, rowY + 3, iconCol);

    String label = fitDashboardText(labels[rowIndex], 13);
    String value = fitDashboardText(values[rowIndex], 25);
    tft.setTextSize(1);
    tft.setTextColor(C_MUTED);
    tft.setCursor(x + 35, rowY + 6);
    tft.print(label);
    tft.setTextColor(colors[rowIndex]);
    tft.setCursor(x + 119, rowY + 6);
    tft.print(value);
  }

  if (HOME_DETAILS_ROW_COUNT > HOME_DETAILS_VISIBLE_ROWS) {
    int trackX = x + w - 9;
    int trackY = listY + 2;
    int trackH = listH - 4;
    int thumbH = trackH * HOME_DETAILS_VISIBLE_ROWS / HOME_DETAILS_ROW_COUNT;
    if (thumbH < 16) thumbH = 16;
    int thumbY = maxTop == 0 ? trackY : trackY + (trackH - thumbH) * homeDetailsTopIndex / maxTop;

    if (fullRedraw || prevHomeDetailsThumbY < 0) {
      tft.fillRoundRect(trackX, trackY, 4, trackH, 2, C_NAV_SLOT);
    } else if (thumbY != prevHomeDetailsThumbY || thumbH != prevHomeDetailsThumbH) {
      tft.fillRoundRect(trackX, prevHomeDetailsThumbY, 4, prevHomeDetailsThumbH, 2, C_NAV_SLOT);
    }
    tft.fillRoundRect(trackX, thumbY, 4, thumbH, 2, C_ACCENT);
    prevHomeDetailsThumbY = thumbY;
    prevHomeDetailsThumbH = thumbH;
  }
}

void drawHomeDetailsScreen() {
  screenMode = SCREEN_HOME_DETAILS;
  sidebarTab = 0;
  homeDetailsTopIndex = 0;
  prevHomeDetailsThumbY = -1;
  drawHomeDetailsContent();
}

// ===================== USER SCREEN =====================

const int USER_VISIBLE_ROWS = 5;

void ensureUserVisible() {
  int maxTop = onlinePeerCount() - USER_VISIBLE_ROWS;
  if (maxTop < 0) maxTop = 0;
  if (userTopIndex < 0) userTopIndex = 0;
  if (userTopIndex > maxTop) userTopIndex = maxTop;
}

int onlineSlotForUserIndex(int userIndex) {
  int seen = 0;
  for (int i = 0; i < MAX_ONLINE_NODES; i++) {
    if (!isOnlineNodeSlot(i)) continue;
    if (seen == userIndex) return i;
    seen++;
  }
  return -1;
}

void drawUserRow(int slot, int nodeSlot) {
  int x = PANEL_X + PANEL_INSET;
  int y = BODY_Y + 42 + slot * 31;
  int w = PANEL_W - PANEL_INSET * 2 - 8;
  OnlineNode node = onlineNodes[nodeSlot];
  String name = node.name;
  String idText = nodeIdText(node.addh, node.addl);
  int nameMax = (w - 33 - idText.length() * 6 - 18) / 6;
  if (nameMax < 6) nameMax = 6;
  if (name.length() > nameMax) name = name.substring(0, nameMax - 2) + "..";
  String detail = String(node.rssi) + " dBm  CH " + String(node.channel);
  if (node.isRepeater) detail += "  R";

  tft.fillRoundRect(x, y, w, 28, 7, C_CARD_BG);
  tft.drawRoundRect(x, y, w, 28, 7, C_CARD_BG);
  tft.fillRoundRect(x + 5, y + 5, 18, 18, 5, C_SEL_BG);
  drawIconNodes(x + 9, y + 7, C_ACCENT);

  tft.setTextSize(1);
  tft.setTextColor(C_TEXT);
  tft.setCursor(x + 33, y + 5);
  tft.print(name);
  tft.setTextColor(C_MUTED);
  tft.setCursor(x + w - idText.length() * 6 - 8, y + 5);
  tft.print(idText);
  tft.setTextColor(C_ACCENT);
  tft.setCursor(x + 33, y + 17);
  tft.print(detail);
}

void drawUserScrollbar() {
  int count = onlinePeerCount();
  if (count <= USER_VISIBLE_ROWS) return;

  int trackX = PANEL_X + PANEL_W - PANEL_INSET - 4;
  int trackY = BODY_Y + 42;
  int trackH = USER_VISIBLE_ROWS * 31 - 3;
  int thumbH = trackH * USER_VISIBLE_ROWS / count;
  if (thumbH < 18) thumbH = 18;
  int maxTop = count - USER_VISIBLE_ROWS;
  int thumbY = trackY + (trackH - thumbH) * userTopIndex / maxTop;
  tft.fillRoundRect(trackX, trackY, 3, trackH, 2, C_NAV_SLOT);
  tft.fillRoundRect(trackX, thumbY, 3, thumbH, 2, C_ACCENT);
}

void drawUserScreen() {
  screenMode = SCREEN_USER;
  sidebarTab = 1;
  ensureUserVisible();
  clearContentArea();
  drawSidebar();

  drawCardBorder(PANEL_X, PANEL_Y, PANEL_W, PANEL_H);
  drawHeader("User");

  drawUserContent();
}

void drawUserContent() {
  ensureUserVisible();
  int x = PANEL_X + PANEL_INSET;
  int w = PANEL_W - PANEL_INSET * 2;
  int count = onlinePeerCount();
  tft.fillRect(PANEL_X + PANEL_INSET, BODY_Y, PANEL_W - PANEL_INSET * 2, 190, C_BG);

  tft.fillRoundRect(x, BODY_Y + 4, w - 8, 30, 7, C_HEADER_BG);
  tft.drawRoundRect(x, BODY_Y + 4, w - 8, 30, 7, count > 0 ? C_ACCENT : C_CARD_BG);
  drawIconNodes(x + 9, BODY_Y + 11, count > 0 ? C_ACCENT : C_MUTED);
  tft.setTextSize(1);
  tft.setTextColor(C_MUTED);
  tft.setCursor(x + 33, BODY_Y + 10);
  tft.print("Online users");
  tft.setTextColor(count > 0 ? C_ACCENT : C_TEXT);
  tft.setCursor(x + 33, BODY_Y + 21);
  tft.print(onlineNodesText());

  if (count == 0) {
    tft.setTextColor(C_MUTED);
    tft.setCursor(x + 8, BODY_Y + 72);
    tft.print("Waiting for nearby nodes...");
    return;
  }

  for (int slot = 0; slot < USER_VISIBLE_ROWS; slot++) {
    int userIndex = userTopIndex + slot;
    if (userIndex >= count) break;
    int nodeSlot = onlineSlotForUserIndex(userIndex);
    if (nodeSlot >= 0) drawUserRow(slot, nodeSlot);
  }
  drawUserScrollbar();
}

// ===================== ABOUT SCREEN =====================

void drawAboutScreen() {
  screenMode = SCREEN_ABOUT;
  sidebarTab = 8;
  clearContentArea();
  drawSidebar();

  drawCardBorder(PANEL_X, PANEL_Y, PANEL_W, PANEL_H);
  drawHeader("About");

  int centerX = PANEL_X + PANEL_W / 2;
  int y = BODY_Y + 24;
  int iconSize = LAUNCHER_ICON_SIZE;
  int titleW = 11 * 6;
  int titleGroupW = iconSize + 8 + titleW;
  int x = centerX - titleGroupW / 2;

  tft.drawRGBBitmap(x, y - 8, LAUNCHER_ICONS[7], iconSize, iconSize);
  tft.setTextSize(1);
  tft.setTextColor(C_ACCENT);
  tft.setCursor(x + iconSize + 8, y + 2);
  tft.print("Lomhor Chat");
  tft.setTextColor(C_MUTED);
  tft.setCursor(x + iconSize + 8, y + 18);
  tft.print(appVersion);

  y += 52;
  tft.setTextColor(C_TEXT);
  String line = "LoRa chat of Cambodia.";
  tft.setCursor(centerX - line.length() * 3, y);
  tft.print(line);
  y += 18;
  tft.setTextColor(C_MUTED);
  line = "Offline mesh messaging";
  tft.setCursor(centerX - line.length() * 3, y);
  tft.print(line);
  y += 14;
  line = "for local communities,";
  tft.setCursor(centerX - line.length() * 3, y);
  tft.print(line);
  y += 14;
  line = "field teams, and makers.";
  tft.setCursor(centerX - line.length() * 3, y);
  tft.print(line);
  y += 24;
  tft.setTextColor(C_ACCENT);
  line = "Cambodia LoRa network";
  tft.setCursor(centerX - line.length() * 3, y);
  tft.print(line);

}

// ===================== MAIN MENU (sidebar navigation) =====================

void openTab(int tab) {
  sidebarTab = tab;
  if (tab == 0) { drawHomeScreen(); return; }
  if (tab == 1) { drawUserScreen(); return; }
  if (tab == 2) { drawChatPresetScreen(); return; }
  if (tab == 3) { drawMapScreen(); return; }
  if (tab == 4) { drawLocationScreen(); return; }
  if (tab == 5) { sdInfoScreen(); return; }
  if (tab == 6) { drawConfigMenu(); return; }
  if (tab == 7) { drawSettingsMenu(); return; }
  if (tab == 8) { drawAboutScreen(); return; }
  if (tab == 9) {
    saveSettingsToStorage();
    saveConfigToStorage();
    showRestartLoadingScreen();
    ESP.restart();
    return;
  }

  // Fallback placeholder
  clearContentArea();
  sidebarTab = tab;
  drawSidebar();
  String titles[] = {"Home","User","Chat","Map","Location","Records","Config","Settings","About","Restart"};
  int cx = PANEL_X, cy = PANEL_Y;
  drawCardBorder(cx, cy, PANEL_W, PANEL_H);
  drawHeader(titles[tab]);
  tft.setTextColor(C_MUTED); tft.setTextSize(1);
  tft.setCursor(cx + PANEL_INSET + 12, cy + 96);
  tft.print("Coming soon...");
}

void returnToHomeTab() {
  openTab(0);
}

void handleHomeScreen(String joy) {
  if (joy == "LEFT") {
    moveHomeAppSelection(-1);
  } else if (joy == "RIGHT") {
    moveHomeAppSelection(1);
  } else if (joy == "UP") {
    moveHomeAppSelection(-HOME_APP_COLS);
  } else if (joy == "DOWN") {
    moveHomeAppSelection(HOME_APP_COLS);
  } else if (joy == "SELECT") {
    if (homeAppIndex == 0) {
      drawHomeDetailsScreen();
      return;
    }
    openTab(homeAppTab(homeAppIndex));
  }
}

void handleHomeDetailsScreen(String joy) {
  if (joy == "UP") {
    if (homeDetailsTopIndex <= 0) return;
    homeDetailsTopIndex--;
    drawHomeDetailsContent(false);
  } else if (joy == "DOWN") {
    int maxTop = HOME_DETAILS_ROW_COUNT - HOME_DETAILS_VISIBLE_ROWS;
    if (maxTop < 0) maxTop = 0;
    if (homeDetailsTopIndex >= maxTop) return;
    homeDetailsTopIndex++;
    drawHomeDetailsContent(false);
  } else if (joy == "LEFT" || joy == "SELECT") {
    returnToHomeTab();
  }
}

void handleUserScreen(String joy) {
  if (joy == "UP") {
    userTopIndex--;
    drawUserContent();
  } else if (joy == "DOWN") {
    userTopIndex++;
    drawUserContent();
  } else if (joy == "LEFT") {
    returnToHomeTab();
  }
}

// ===================== MAP SCREEN =====================

void drawMapNode(int cx, int cy, String label, String detail, uint16_t col, bool selfNode) {
  if (selfNode) {
    tft.fillCircle(cx, cy, 6, col);
    tft.drawCircle(cx, cy, 10, C_BORDER);
  } else {
    tft.fillCircle(cx, cy, 4, col);
  }

  tft.setTextSize(1);
  tft.setTextColor(col);
  int labelX = cx + 8;
  if (labelX + label.length() * 6 > PANEL_X + PANEL_W - PANEL_INSET) {
    labelX = cx - 8 - label.length() * 6;
  }
  if (labelX < PANEL_X + PANEL_INSET) labelX = PANEL_X + PANEL_INSET;
  tft.setCursor(labelX, cy - 8);
  tft.print(label);

  if (detail.length() > 0) {
    tft.setTextColor(C_MUTED);
    tft.setCursor(labelX, cy + 3);
    tft.print(detail);
  }
}

void drawMapContent() {
  int x = PANEL_X + PANEL_INSET;
  int y = BODY_Y + 8;
  int w = PANEL_W - PANEL_INSET * 2;
  int h = 168;
  int centerX = x + w / 2;
  int centerY = y + h / 2;
  int peers = onlinePeerCount();

  tft.fillRect(x, BODY_Y, w, 190, C_BG);
  tft.fillRoundRect(x, y, w, h, 8, C_CARD_BG);
  tft.drawRoundRect(x, y, w, h, 8, C_BORDER);

  for (int gx = x + 24; gx < x + w; gx += 24) tft.drawFastVLine(gx, y + 4, h - 8, C_NAV_SLOT);
  for (int gy = y + 24; gy < y + h; gy += 24) tft.drawFastHLine(x + 4, gy, w - 8, C_NAV_SLOT);

  tft.drawCircle(centerX, centerY, 34, C_NAV_SLOT);
  tft.drawCircle(centerX, centerY, 62, C_NAV_SLOT);
  drawMapNode(centerX, centerY, "ME", nodeIdText(MY_ADDH, MY_ADDL), C_ACCENT, true);

  int plotted = 0;
  for (int i = 0; i < MAX_ONLINE_NODES; i++) {
    if (!isOnlineNodeSlot(i)) continue;
    OnlineNode node = onlineNodes[i];
    int16_t signal = node.rssi;
    int radius = map(constrain(signal, -120, -35), -120, -35, 68, 24);
    float angle = ((node.addh * 37 + node.addl * 19 + plotted * 53) % 360) * PI / 180.0f;
    int nx = centerX + cos(angle) * radius;
    int ny = centerY + sin(angle) * radius;
    String label = node.name.length() > 0 ? node.name : nodeIdText(node.addh, node.addl);
    if (label.length() > 8) label = label.substring(0, 6) + "..";
    drawMapNode(nx, ny, label, String(signal) + " dBm", node.isRepeater ? C_WARN : C_TEXT, false);
    plotted++;
  }

  tft.fillRoundRect(x, y + h + 7, w, 24, 7, C_HEADER_BG);
  tft.drawRoundRect(x, y + h + 7, w, 24, 7, peers > 0 ? C_ACCENT : C_CARD_BG);
  drawIconMap(x + 9, y + h + 12, peers > 0 ? C_ACCENT : C_MUTED);
  tft.setTextColor(peers > 0 ? C_ACCENT : C_MUTED);
  tft.setCursor(x + 31, y + h + 12);
  tft.print(onlineNodesText());
}

void drawMapScreen() {
  screenMode = SCREEN_MAP;
  sidebarTab = 3;
  clearContentArea();
  drawSidebar();
  drawCardBorder(PANEL_X, PANEL_Y, PANEL_W, PANEL_H);
  drawHeader("Map");
  drawMapContent();
}

void handleMapScreen(String joy) {
  if (joy == "LEFT") {
    returnToHomeTab();
  } else if (joy == "SELECT") {
    drawMapContent();
  }
}

// ===================== LOCATION SCREEN =====================

void drawLocationRow(int x, int y, String label, String value, uint16_t valueCol) {
  int w = PANEL_W - PANEL_INSET * 2;
  tft.fillRoundRect(x, y, w, 21, 4, (y / 21) % 2 == 0 ? C_TILE_BG : C_CARD_BG);
  tft.setTextSize(1);
  tft.setTextColor(C_MUTED);
  tft.setCursor(x + 8, y + 7);
  tft.print(label);
  tft.setTextColor(valueCol);
  int valueX = x + 108;
  if (value.length() > 31) value = value.substring(0, 29) + "..";
  tft.setCursor(valueX, y + 7);
  tft.print(value);
}

void ensureLocationVisible() {
  int maxTop = LOCATION_ROW_COUNT - LOCATION_VISIBLE_ROWS;
  if (maxTop < 0) maxTop = 0;
  if (locationTopIndex < 0) locationTopIndex = 0;
  if (locationTopIndex > maxTop) locationTopIndex = maxTop;
}

String gpsAgeText(unsigned long timestamp) {
  if (timestamp == 0) return "--";
  unsigned long age = (millis() - timestamp) / 1000UL;
  if (age > 999) return ">999s";
  return String(age) + "s";
}

String locationRowLabel(int rowIndex) {
  if (rowIndex == 0) return "Latitude";
  if (rowIndex == 1) return "Longitude";
  if (rowIndex == 2) return "Altitude";
  if (rowIndex == 3) return "Speed";
  if (rowIndex == 4) return "Course";
  if (rowIndex == 5) return "Sat / HDOP";
  if (rowIndex == 6) return "UTC";
  if (rowIndex == 7) return "UART";
  if (rowIndex == 8) return "I2C";
  return "Last fix";
}

String locationRowValue(int rowIndex) {
  if (rowIndex == 0) return gpsHasFix ? formatDecimalDegrees(gpsLatitude) : "--";
  if (rowIndex == 1) return gpsHasFix ? formatDecimalDegrees(gpsLongitude) : "--";
  if (rowIndex == 2) return gpsHasFix ? String(gpsAltitudeM, 1) + " m" : "--";
  if (rowIndex == 3) return gpsHasFix ? String(gpsSpeedKmh, 1) + " km/h" : "--";
  if (rowIndex == 4) return gpsHasFix ? String(gpsCourseDeg, 1) + " deg" : "--";
  if (rowIndex == 5) return String(gpsSatellites) + " / " + String(gpsHdop, 1);
  if (rowIndex == 6) return gpsUtcDate + " " + gpsUtcTime;
  if (rowIndex == 7) return millis() - lastGpsCharMs <= 4000UL ? "NMEA RX " + gpsAgeText(lastGpsCharMs) : "No NMEA";
  if (rowIndex == 8) return gpsI2cReady ? "Device ACK" : "No ACK";
  return gpsHasFix ? gpsAgeText(lastGpsFixMs) : "--";
}

uint16_t locationRowColor(int rowIndex) {
  if (rowIndex == 0 || rowIndex == 1 || rowIndex == 2 || rowIndex == 3 || rowIndex == 4) return gpsHasFix ? C_TEXT : C_MUTED;
  if (rowIndex == 5) return gpsSatellites > 0 ? C_ACCENT : C_MUTED;
  if (rowIndex == 6) return gpsDateValid ? C_TEXT : C_MUTED;
  if (rowIndex == 7) return millis() - lastGpsCharMs <= 4000UL ? C_ACCENT : C_WARN;
  if (rowIndex == 8) return gpsI2cReady ? C_CYAN : C_MUTED;
  return gpsHasFix ? C_ACCENT : C_MUTED;
}

void drawLocationScrollbar(int x, int y, int h) {
  if (LOCATION_ROW_COUNT <= LOCATION_VISIBLE_ROWS) return;

  int maxTop = LOCATION_ROW_COUNT - LOCATION_VISIBLE_ROWS;
  int thumbH = h * LOCATION_VISIBLE_ROWS / LOCATION_ROW_COUNT;
  if (thumbH < 18) thumbH = 18;
  int thumbY = y + (h - thumbH) * locationTopIndex / maxTop;

  tft.fillRoundRect(x, y, 3, h, 2, C_NAV_SLOT);
  tft.fillRoundRect(x, thumbY, 3, thumbH, 2, C_ACCENT);
}

String compassDirectionText(float degrees) {
  while (degrees < 0.0f) degrees += 360.0f;
  while (degrees >= 360.0f) degrees -= 360.0f;
  const char* dirs[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  int index = (int)((degrees + 22.5f) / 45.0f) % 8;
  return String(dirs[index]);
}

void drawLocationCompass(int x, int y, int w, int h) {
  tft.fillRoundRect(x, y, w, h, 7, C_PANEL);
  tft.drawRoundRect(x, y, w, h, 7, C_BORDER);

  int cx = x + 48;
  int cy = y + h / 2;
  int r = 25;
  uint16_t dialCol = gpsHasFix ? C_ACCENT : C_MUTED;
  float course = gpsHasFix ? gpsCourseDeg : 0.0f;
  float angle = (course - 90.0f) * PI / 180.0f;
  int tipX = cx + cos(angle) * (r - 11);
  int tipY = cy + sin(angle) * (r - 11);
  int tailX = cx - cos(angle) * 5;
  int tailY = cy - sin(angle) * 5;

  tft.drawCircle(cx, cy, r, dialCol);
  tft.drawCircle(cx, cy, r - 1, dialCol);
  tft.drawCircle(cx, cy, r - 7, C_NAV_SLOT);
  tft.setTextSize(1);
  tft.setTextColor(C_TEXT);
  tft.setCursor(cx - 3, cy - r - 9);
  tft.print("N");
  tft.setTextColor(C_MUTED);
  tft.setCursor(cx + r + 6, cy - 3);
  tft.print("E");
  tft.setCursor(cx - 3, cy + r + 3);
  tft.print("S");
  tft.setCursor(cx - r - 12, cy - 3);
  tft.print("W");

  tft.drawLine(tailX, tailY, tipX, tipY, dialCol);
  tft.fillCircle(tipX, tipY, 2, gpsHasFix ? C_WARN : C_MUTED);
  tft.fillCircle(cx, cy, 2, C_TEXT);

  int textX = x + 96;
  tft.setTextColor(gpsHasFix ? C_ACCENT : C_WARN);
  tft.setCursor(textX, y + 9);
  tft.print(gpsHasFix ? "Compass" : "Compass --");

  tft.setTextColor(C_TEXT);
  tft.setCursor(textX, y + 25);
  if (gpsHasFix) {
    tft.print(compassDirectionText(course));
    tft.print(" ");
    tft.print(course, 1);
    tft.print(" deg");
  } else {
    tft.print("Waiting for GPS course");
  }

  tft.setTextColor(C_MUTED);
  tft.setCursor(textX, y + 49);
  String status = gpsStatusText;
  if (millis() - lastGpsCharMs <= 4000UL) status += " NMEA";
  tft.print(fitDashboardText(status, 30));
}

void drawLocationHeaderValues(int x, int y, int w) {
  tft.drawRoundRect(x, y, w, 32, 7, gpsHasFix ? C_ACCENT : C_CARD_BG);
  tft.fillRect(x + 8, y + 7, 22, 18, C_HEADER_BG);
  drawIconMap(x + 10, y + 8, gpsHasFix ? C_ACCENT : C_MUTED);

  tft.setTextSize(1);
  tft.setTextColor(C_MUTED);
  tft.fillRect(x + 36, y + 7, 70, 8, C_HEADER_BG);
  tft.setCursor(x + 36, y + 7);
  tft.print("GPS");

  tft.setTextColor(gpsHasFix ? C_ACCENT : C_WARN);
  tft.fillRect(x + 36, y + 19, 150, 8, C_HEADER_BG);
  tft.setCursor(x + 36, y + 19);
  tft.print(gpsFixText());

  tft.setTextColor(gpsI2cReady ? C_CYAN : C_MUTED);
  tft.fillRect(x + w - 74, y + 13, 66, 8, C_HEADER_BG);
  tft.setCursor(x + w - 70, y + 13);
  tft.print(gpsI2cReady ? "I2C OK" : "I2C --");
}

void drawLocationContent(bool fullRedraw = true) {
  ensureLocationVisible();
  int x = PANEL_X + PANEL_INSET;
  int w = PANEL_W - PANEL_INSET * 2;
  int y = BODY_Y + 6;

  if (fullRedraw) {
    tft.fillRect(x, BODY_Y, w, 190, C_BG);
    tft.fillRoundRect(x, y, w, 32, 7, C_HEADER_BG);
  }
  drawLocationHeaderValues(x, y, w);

  y += 42;
  for (int slot = 0; slot < LOCATION_VISIBLE_ROWS; slot++) {
    int rowIndex = locationTopIndex + slot;
    if (rowIndex >= LOCATION_ROW_COUNT) break;
    drawLocationRow(x, y + slot * 23, locationRowLabel(rowIndex), locationRowValue(rowIndex), locationRowColor(rowIndex));
  }
  drawLocationScrollbar(x + w - 5, y, LOCATION_VISIBLE_ROWS * 23 - 2);

  drawLocationCompass(x, FOOTER_Y - 74, w, 74);
}

void drawLocationScreen() {
  screenMode = SCREEN_LOCATION;
  sidebarTab = 4;
  locationTopIndex = 0;
  clearContentArea();
  drawSidebar();
  drawCardBorder(PANEL_X, PANEL_Y, PANEL_W, PANEL_H);
  drawHeader("Location");
  drawLocationContent();
}

void handleLocationScreen(String joy) {
  if (joy == "UP") {
    locationTopIndex--;
    drawLocationContent();
  } else if (joy == "DOWN") {
    locationTopIndex++;
    drawLocationContent();
  } else if (joy == "LEFT") {
    returnToHomeTab();
  } else if (joy == "SELECT") {
    gpsI2cReady = scanGpsI2cBus();
    drawLocationContent();
  }
}

void handleTabNav(String joy) {
  // UP/DOWN navigate tabs, SELECT opens
  if (joy == "UP") {
    int oldTab = sidebarTab;
    int oldTop = sidebarTopTab;
    sidebarTab--;
    if (sidebarTab < 0) sidebarTab = SIDEBAR_TABS - 1;
    ensureSidebarVisible();
    animateSidebarSelection(oldTab, sidebarTab, oldTop != sidebarTopTab);
  }
  else if (joy == "DOWN") {
    int oldTab = sidebarTab;
    int oldTop = sidebarTopTab;
    sidebarTab++;
    if (sidebarTab >= SIDEBAR_TABS) sidebarTab = 0;
    ensureSidebarVisible();
    animateSidebarSelection(oldTab, sidebarTab, oldTop != sidebarTopTab);
  }
  else if (joy == "SELECT") {
    openTab(sidebarTab);
  }
}

void drawSidebarTabOnly(int i, bool active) {
  drawSidebarSlot(i, active, SIDEBAR_ICON_LARGE);
}

// ===================== CHAT PRESETS =====================

void ensureChatPresetVisible() {
  if (chatPresetIndex < chatPresetTopIndex) chatPresetTopIndex = chatPresetIndex;
  if (chatPresetIndex >= chatPresetTopIndex + CHAT_VISIBLE_PRESETS) {
    chatPresetTopIndex = chatPresetIndex - CHAT_VISIBLE_PRESETS + 1;
  }
  if (chatPresetTopIndex < 0) chatPresetTopIndex = 0;
  int maxTop = chatPresetCount - CHAT_VISIBLE_PRESETS;
  if (maxTop < 0) maxTop = 0;
  if (chatPresetTopIndex > maxTop) chatPresetTopIndex = maxTop;
}

void drawChatPresetRow(int presetIndex, int slot, bool selected) {
  int x = PANEL_X + PANEL_INSET;
  int listX = x + 6;
  int y = BODY_Y + 50 + slot * 27;
  int w = PANEL_W - PANEL_INSET * 2 - 18;
  uint16_t bubbleCol = C_CARD_BG;
  uint16_t borderCol = selected ? C_ACCENT : C_CARD_BG;
  uint16_t textCol = selected ? C_ACCENT : C_TEXT;

  tft.fillRect(x, y - 2, PANEL_W - PANEL_INSET * 2, 26, C_BG);
  tft.fillRoundRect(listX, y, w, 23, 6, bubbleCol);
  tft.drawRoundRect(listX, y, w, 23, 6, borderCol);
  tft.fillCircle(listX + 12, y + 11, selected ? 4 : 3, selected ? C_ACCENT : C_MUTED);

  tft.setTextSize(1);
  tft.setTextColor(textCol);
  tft.setCursor(listX + 26, y + 8);
  tft.print(chatPresets[presetIndex]);
}

void drawChatPresetList() {
  ensureChatPresetVisible();
  tft.fillRect(PANEL_X + PANEL_INSET, BODY_Y + 46, PANEL_W - PANEL_INSET * 2, 140, C_BG);

  for (int slot = 0; slot < CHAT_VISIBLE_PRESETS; slot++) {
    int presetIndex = chatPresetTopIndex + slot;
    if (presetIndex >= chatPresetCount) break;
    drawChatPresetRow(presetIndex, slot, presetIndex == chatPresetIndex);
  }

  if (chatPresetCount > CHAT_VISIBLE_PRESETS) {
    int trackX = PANEL_X + PANEL_W - PANEL_INSET - 7;
    int trackY = BODY_Y + 50;
    int trackH = CHAT_VISIBLE_PRESETS * 27 - 4;
    int thumbH = trackH * CHAT_VISIBLE_PRESETS / chatPresetCount;
    if (thumbH < 14) thumbH = 14;
    int denom = chatPresetCount - CHAT_VISIBLE_PRESETS;
    int thumbY = trackY + (trackH - thumbH) * chatPresetTopIndex / denom;
    tft.fillRoundRect(trackX, trackY, 3, trackH, 2, C_NAV_SLOT);
    tft.fillRoundRect(trackX, thumbY, 3, thumbH, 2, C_ACCENT);
  }
}

void drawChatStatus(String text, uint16_t col) {
  int x = PANEL_X + PANEL_INSET;
  int y = FOOTER_Y - 0;
  int w = PANEL_W - PANEL_INSET * 2;
  tft.fillRect(x, y - 1, w, 12, C_BG);
  tft.setTextSize(1);
  tft.setTextColor(col);
  tft.setCursor(x, y);
  tft.print(text);
}

void drawChatPresetScreen() {
  screenMode = SCREEN_CHAT_PRESETS;
  sidebarTab = 2;
  clearContentArea();
  drawSidebar();
  drawCardBorder(PANEL_X, PANEL_Y, PANEL_W, PANEL_H);
  drawHeader("Chat");

  int x = PANEL_X + PANEL_INSET;
  tft.fillRoundRect(x, BODY_Y + 6, PANEL_W - PANEL_INSET * 2 - 28, 28, 7, C_CARD_BG);
  tft.drawRoundRect(x, BODY_Y + 6, PANEL_W - PANEL_INSET * 2 - 28, 28, 7, C_BORDER);
  drawIconMsg(x + 9, BODY_Y + 13, C_ACCENT);
  tft.setTextSize(1);
  tft.setTextColor(C_MUTED);
  tft.setCursor(x + 33, BODY_Y + 12);
  tft.print("Quick message");
  tft.setTextColor(C_TEXT);
  tft.setCursor(x + 33, BODY_Y + 24);
  tft.print("Choose a preset to send");

  drawChatPresetList();

  // drawChatStatus("UP/DN choose  SEL send  LEFT back", C_MUTED);
}

void handleChatPresets(String joy) {
  if (joy == "UP") {
    chatPresetIndex--;
    if (chatPresetIndex < 0) chatPresetIndex = chatPresetCount - 1;
    drawChatPresetList();
    // drawChatStatus("UP/DN choose  SEL send  LEFT back", C_MUTED);
  }
  else if (joy == "DOWN") {
    chatPresetIndex++;
    if (chatPresetIndex >= chatPresetCount) chatPresetIndex = 0;
    drawChatPresetList();
    // drawChatStatus("UP/DN choose  SEL send  LEFT back", C_MUTED);
  }
  else if (joy == "SELECT") {
    if (chatPresetIndex == 0) {
      drawFreeTextKeyboard();
      return;
    }
    pendingChatMessage = chatPresets[chatPresetIndex];
    pendingChatFromFreeText = false;
    drawChatSendTargetScreen();
  }
  else if (joy == "LEFT") {
    returnToHomeTab();
  }
}

// ===================== FREE TEXT KEYBOARD =====================

void drawFreeTextValue() {
  int x = PANEL_X + 10;
  int y = 38;
  int w = PANEL_W - 20;
  String text = keyboardEditingWifiPassword ? wifiPasswordEdit : freeTextMessage;
  int maxLen = keyboardEditingWifiPassword ? 63 : 64;
  tft.fillRoundRect(x, y, w, 42, 8, C_PANEL);
  tft.drawRoundRect(x, y, w, 42, 8, wifiPasswordTooShort ? C_WARN : C_ACCENT);

  String shown = text;
  if (shown.length() == 0) shown = keyboardEditingWifiPassword ? "Type 8-63 chars..." : "Type message...";
  if (shown.length() > 34) shown = shown.substring(shown.length() - 34);

  tft.setTextSize(1);
  tft.setTextColor(C_MUTED);
  tft.setCursor(x + 8, y + 6);
  tft.print(keyboardEditingWifiPassword ? "WiFi Password" : "Message");
  tft.setTextColor(text.length() == 0 ? C_MUTED : C_TEXT);
  tft.setCursor(x + 8, y + 23);
  tft.print(shown);

  String countText = String(text.length()) + "/" + String(maxLen);
  tft.setTextColor((keyboardEditingWifiPassword && text.length() < 8) || text.length() >= maxLen - 4 ? C_WARN : C_MUTED);
  tft.setCursor(x + w - countText.length() * 6 - 8, y + 6);
  tft.print(countText);
}

uint16_t keyboardKeyColor(String label, bool selected) {
  if (label == "SEND") return selected ? C_GREEN : 0x1D66;
  if (label == "BACK") return selected ? C_WARN : 0x6320;
  if (label == "DEL") return selected ? C_ERROR : 0x4000;
  if (label == "SP") return selected ? C_CYAN : 0x0451;
  return selected ? C_ACCENT : C_TILE_BG;
}

void drawKeyboardKey(int i, bool selected) {
  int row = i / KEYBOARD_COLS;
  int col = i % KEYBOARD_COLS;
  int keyW = 35;
  int keyH = 20;
  int gap = 3;
  int x0 = PANEL_X + 11;
  int y0 = 88;
  int x = x0 + col * (keyW + gap);
  int y = y0 + row * (keyH + gap);
  String label = keyboardKeys[i];
  String shownLabel = (keyboardEditingWifiPassword && label == "SEND") ? "SAVE" : label;
  uint16_t fillCol = keyboardKeyColor(label, selected);
  uint16_t borderCol = selected ? C_TEXT : C_BORDER;
  uint16_t textCol = C_TEXT;

  tft.fillRect(x - 2, y - 2, keyW + 4, keyH + 4, C_BG);
  tft.fillRoundRect(x, y, keyW, keyH, 5, fillCol);
  tft.drawRoundRect(x, y, keyW, keyH, 5, borderCol);
  if (selected) tft.drawRoundRect(x - 2, y - 2, keyW + 4, keyH + 4, 7, C_ACCENT);
  tft.setTextSize(1);
  tft.setTextColor(textCol);
  int tx = x + (keyW - shownLabel.length() * 6) / 2;
  if (tx < x + 2) tx = x + 2;
  tft.setCursor(tx, y + 7);
  tft.print(shownLabel);
}

void drawKeyboardGrid() {
  tft.fillRect(PANEL_X + 8, 84, PANEL_W - 16, 145, C_BG);
  for (int i = 0; i < keyboardKeyCount; i++) {
    drawKeyboardKey(i, i == keyboardIndex);
  }

  tft.setTextSize(1);
  tft.setTextColor(C_MUTED);
  tft.setCursor(PANEL_X + 12, 228);
  tft.print("LEFT: Back   SELECT: Press");
}

void drawFreeTextKeyboard() {
  keyboardEditingWifiPassword = false;
  wifiPasswordTooShort = false;
  screenMode = SCREEN_CHAT_KEYBOARD;
  sidebarTab = 2;
  tft.fillScreen(C_BG);
  tft.fillRoundRect(8, 6, 304, 25, 8, C_ACCENT);
  tft.setTextSize(2);
  tft.setTextColor(C_TEXT);
  tft.setCursor(16, 11);
  tft.print("Free Text");
  tft.setTextSize(1);
  tft.setCursor(246, 15);
  tft.print("LoRa");
  drawFreeTextValue();
  drawKeyboardGrid();
}

void drawWifiPasswordKeyboard() {
  keyboardEditingWifiPassword = true;
  wifiPasswordTooShort = false;
  wifiPasswordEdit = wifiApPassword;
  screenMode = SCREEN_CHAT_KEYBOARD;
  sidebarTab = 7;
  tft.fillScreen(C_BG);
  tft.fillRoundRect(8, 6, 304, 25, 8, C_ACCENT);
  tft.setTextSize(2);
  tft.setTextColor(C_TEXT);
  tft.setCursor(16, 11);
  tft.print("WiFi Pass");
  tft.setTextSize(1);
  tft.setCursor(262, 15);
  tft.print("AP");
  drawFreeTextValue();
  drawKeyboardGrid();
}

void pressKeyboardKey() {
  String key = keyboardKeys[keyboardIndex];
  String *text = keyboardEditingWifiPassword ? &wifiPasswordEdit : &freeTextMessage;
  int maxLen = keyboardEditingWifiPassword ? 63 : 64;
  if (key == "SP") {
    if (text->length() < maxLen) *text += " ";
  } else if (key == "DEL") {
    if (text->length() > 0) text->remove(text->length() - 1);
  } else if (key == "BACK") {
    if (keyboardEditingWifiPassword) drawSettingsMenu();
    else drawChatPresetScreen();
    return;
  } else if (key == "SEND") {
    if (keyboardEditingWifiPassword) {
      if (wifiPasswordEdit.length() < 8) {
        wifiPasswordTooShort = true;
        drawFreeTextValue();
        return;
      }
      wifiApPassword = wifiPasswordEdit;
      saveSettingsToStorage();
      initWifiAccessPoint();
      drawSettingsMenu();
      return;
    }
    if (freeTextMessage.length() == 0) {
      return;
    }
    pendingChatMessage = freeTextMessage;
    pendingChatFromFreeText = true;
    drawChatSendTargetScreen();
    return;
  } else {
    if (text->length() < maxLen) *text += key;
  }
  wifiPasswordTooShort = false;
  drawFreeTextValue();
}

void moveKeyboardSelection(int nextIndex) {
  if (nextIndex < 0) nextIndex = keyboardKeyCount - 1;
  if (nextIndex >= keyboardKeyCount) nextIndex = 0;
  int old = keyboardIndex;
  keyboardIndex = nextIndex;
  drawKeyboardKey(old, false);
  drawKeyboardKey(keyboardIndex, true);
}

void handleFreeTextKeyboard(String joy) {
  if (joy == "LEFT") {
    moveKeyboardSelection(keyboardIndex - 1);
  } else if (joy == "RIGHT") {
    moveKeyboardSelection(keyboardIndex + 1);
  } else if (joy == "UP") {
    moveKeyboardSelection(keyboardIndex - KEYBOARD_COLS);
  } else if (joy == "DOWN") {
    moveKeyboardSelection(keyboardIndex + KEYBOARD_COLS);
  } else if (joy == "SELECT") {
    pressKeyboardKey();
  }
}

// ===================== CHAT SEND TARGET =====================

void drawChatSendBanner(String text, uint16_t col) {
  int x = PANEL_X + PANEL_INSET;
  int y = BODY_Y + 8;
  int w = PANEL_W - PANEL_INSET * 2;
  tft.fillRoundRect(x, y, w, 24, 5, C_CARD_BG);
  tft.drawRoundRect(x, y, w, 24, 5, col);
  tft.setTextSize(1);
  tft.setTextColor(col);
  tft.setCursor(x + 8, y + 8);
  tft.print(text);
}

void drawSendingDialog() {
  const int padding = 3;
  int w = 150;
  int h = 42;
  int x = PANEL_X + (PANEL_W - w) / 2;
  int y = PANEL_Y + (PANEL_H - h) / 2;
  int contentX = x + padding;
  int contentY = y + padding;

  tft.fillRoundRect(x, y, w, h, 6, C_CARD_BG);
  tft.drawRoundRect(x, y, w, h, 6, C_ACCENT);
  drawIconMsg(contentX + 15, contentY + 10, C_ACCENT);
  tft.setTextSize(1);
  tft.setTextColor(C_TEXT);
  tft.setCursor(contentX + 43, contentY + 13);
  tft.print("Sending...");
  delay(30);
}

void drawSendTargetRow(int targetIndex, int slot, bool selected) {
  int x = PANEL_X + PANEL_INSET + 8;
  int y = BODY_Y + 54 + slot * 34;
  int w = PANEL_W - PANEL_INSET * 2 - 16;
  uint16_t fillCol = C_CARD_BG;
  uint16_t borderCol = selected ? C_ACCENT : C_CARD_BG;
  uint16_t textCol = selected ? C_ACCENT : C_TEXT;

  tft.fillRect(PANEL_X + PANEL_INSET, y - 2, PANEL_W - PANEL_INSET * 2, 32, C_BG);
  tft.fillRoundRect(x, y, w, 28, 6, fillCol);
  tft.drawRoundRect(x, y, w, 28, 6, borderCol);
  drawIconNodes(x + 10, y + 7, selected ? C_ACCENT : C_MUTED);
  tft.setTextSize(1);
  tft.setTextColor(textCol);
  tft.setCursor(x + 34, y + 10);
  tft.print(sendTargetLabel(targetIndex));
}

void drawSendTargetList() {
  ensureSendTargetVisible();
  tft.fillRect(PANEL_X + PANEL_INSET, BODY_Y + 50, PANEL_W - PANEL_INSET * 2, 142, C_BG);

  int count = sendTargetCount();
  for (int slot = 0; slot < SEND_TARGET_VISIBLE; slot++) {
    int targetIndex = sendTargetTopIndex + slot;
    if (targetIndex >= count) break;
    drawSendTargetRow(targetIndex, slot, targetIndex == sendTargetIndex);
  }
}

void drawChatSendTargetScreen() {
  screenMode = SCREEN_CHAT_SEND_TO;
  sidebarTab = 2;
  clearContentArea();
  drawSidebar();
  drawCardBorder(PANEL_X, PANEL_Y, PANEL_W, PANEL_H);
  drawHeader("Send To");

  String preview = pendingChatMessage;
  if (preview.length() > 28) preview = preview.substring(0, 28) + "..";
  drawChatSendBanner(preview, C_MUTED);

  drawSendTargetList();
}

void handleChatSendTarget(String joy) {
  if (joy == "UP") {
    sendTargetIndex--;
    if (sendTargetIndex < 0) sendTargetIndex = sendTargetCount() - 1;
    drawSendTargetList();
  } else if (joy == "DOWN") {
    sendTargetIndex++;
    if (sendTargetIndex >= sendTargetCount()) sendTargetIndex = 0;
    drawSendTargetList();
  } else if (joy == "SELECT") {
    selectSendTargetAddress();
    drawSendingDialog();
    bool sentOk = sendLocalMessage(pendingChatMessage);
    delay(SEND_DIALOG_VISIBLE_MS);
    String sent = sentOk ? e22StatusText : "Send failed";
    drawChatSendTargetScreen();
    drawChatSendBanner(sent, sentOk ? C_ACCENT : C_ERROR);
    if (sentOk && pendingChatFromFreeText) freeTextMessage = "";
  } else if (joy == "LEFT") {
    if (pendingChatFromFreeText) drawFreeTextKeyboard();
    else drawChatPresetScreen();
  }
}

// ===================== SD HELPERS =====================

void prepareSdBus() {
  digitalWrite(TFT_CS, HIGH);
  digitalWrite(SD_CS, HIGH);
  delayMicroseconds(5);
}

void beginSharedSpiBus() {
  SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TFT_CS);
}

void beginSdSpiBus() {
  sdSPI.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);
}

File openSdPath(String path, const char* mode = FILE_READ) {
  prepareSdBus();
  Serial.println("openSdPath");
  Serial.println(path.c_str());
  return SD.open(path.c_str(), mode);
}

// ===================== FILE MANAGER =====================

void loadFiles(String path) {
  fileCount = 0; selectedIndex = 0; topIndex = 0;
  prevSelectedIndex = -1; prevTopIndex = -1;
  if (!sdReady) return;
  File root = openSdPath(path);
  if (!root || !root.isDirectory()) { root.close(); return; }
  File file = root.openNextFile();
  while (file && fileCount < MAX_FILES) {
    String name = String(file.name());
    int slash = name.lastIndexOf('/');
    if (slash >= 0) name = name.substring(slash + 1);
    fileNames[fileCount] = name;
    fileIsDir[fileCount] = file.isDirectory();
    fileCount++;
    file.close();
    file = root.openNextFile();
  }
  root.close();
}

void drawFileRow(int index, bool selected) {
  int vp = index - topIndex;
  if (vp < 0 || vp >= 7) return;
  int y = BODY_Y + 18 + vp * 24;
  int x = PANEL_X + PANEL_INSET;
  int w = PANEL_W - PANEL_INSET * 2;

  if (selected) {
    tft.fillRoundRect(x, y, w, 22, 3, C_CARD_BG);
    tft.drawRoundRect(x, y, w, 22, 3, C_ACCENT);
    tft.setTextColor(C_ACCENT);
  } else {
    tft.fillRoundRect(x, y, w, 22, 3, C_CARD_BG);
    tft.setTextColor(C_TEXT);
  }
  tft.setTextSize(1);
  if (fileIsDir[index]) {
    drawIconFolder(x + 6, y + 4, selected ? C_ACCENT : C_MUTED);
  } else {
    drawIconFile(x + 7, y + 3, selected ? C_ACCENT : C_MUTED);
  }
  if (selected) tft.setTextColor(C_ACCENT); else tft.setTextColor(C_TEXT);
  tft.setCursor(x + 28, y + 7);
  String name = fileNames[index];
  if (name.length() > 25) name = name.substring(0, 25) + "..";
  tft.print(name);
}

void drawFileListContent() {
  tft.fillRect(PANEL_X + PANEL_INSET, BODY_Y + 16, PANEL_W - PANEL_INSET * 2, 174, C_BG);

  if (fileCount == 0) {
    tft.setTextColor(C_MUTED);
    tft.setTextSize(1);
    tft.setCursor(PANEL_X + PANEL_INSET + 30, BODY_Y + 86);
    tft.print("(empty)");
    return;
  }

  for (int i = 0; i < 7; i++) {
    int idx = topIndex + i;
    if (idx >= fileCount) break;
    drawFileRow(idx, idx == selectedIndex);
  }

  if (fileCount > 7) {
    int barH = 168 * 7 / fileCount;
    int barY = BODY_Y + 18 + 168 * topIndex / fileCount;
    tft.fillRect(PANEL_X + PANEL_W - 4, BODY_Y + 18, 2, 168, C_NAV_SLOT);
    tft.fillRect(PANEL_X + PANEL_W - 4, barY, 2, barH, C_BORDER);
  }
}

void drawFileManager() {
  screenMode = SCREEN_FILE_MANAGER;
  sidebarTab = 2;
  if (!sdReady) sdReady = initSDCard();
  clearContentArea();
  drawSidebar();
  drawCardBorder(PANEL_X, PANEL_Y, PANEL_W, PANEL_H);
  drawHeader("Files");

  if (!sdReady) {
    tft.setTextColor(C_ERROR); tft.setTextSize(1);
    tft.setCursor(PANEL_X + PANEL_INSET + 12, BODY_Y + 70); tft.print("SD not ready");
    return;
  }

  // Path bar
  tft.fillRect(PANEL_X + PANEL_INSET, BODY_Y, PANEL_W - PANEL_INSET * 2, 12, C_CARD_BG);
  tft.setTextSize(1); tft.setTextColor(C_MUTED);
  tft.setCursor(PANEL_X + PANEL_INSET, BODY_Y + 1);
  String p = currentPath; if (p.length() > 30) p = ".." + p.substring(p.length()-28);
  tft.print(p);

  if (fileCount == 0) {
    drawFileListContent();
    return;
  }

  drawFileListContent();

  prevSelectedIndex = selectedIndex; prevTopIndex = topIndex;
}

void goBackFolder() {
  if (currentPath == "/") { returnToHomeTab(); return; }
  int ls = currentPath.lastIndexOf('/');
  if (ls <= 0) currentPath = "/";
  else currentPath = currentPath.substring(0, ls);
  loadFiles(currentPath); drawFileManager();
}

void openSelectedFile() {
  if (fileCount == 0) return;
  String name = fileNames[selectedIndex];
  String full = (currentPath == "/") ? "/" + name : currentPath + "/" + name;
  if (fileIsDir[selectedIndex]) { currentPath = full; loadFiles(currentPath); drawFileManager(); }
  else {
    textViewerReturnScreen = SCREEN_FILE_MANAGER;
    showTextFile(full);
  }
}

int textViewerVisibleLines() {
  int lineH = 8 * displayTextSize + 4;
  int areaH = FOOTER_Y - 8 - (BODY_Y + 20);
  int visible = areaH / lineH;
  return visible < 1 ? 1 : visible;
}

int textViewerMaxChars() {
  int maxChars = (PANEL_W - PANEL_INSET * 2 - 8) / (6 * displayTextSize);
  return maxChars < 8 ? 8 : maxChars;
}

int countTextFileLines(String path) {
  if (!sdReady) sdReady = initSDCard();
  if (!sdReady) return 0;

  File file = openSdPath(path, FILE_READ);
  if (!file) return 0;

  int count = 0;
  while (file.available()) {
    file.readStringUntil('\n');
    count++;
  }
  file.close();
  return count;
}

void drawTextViewerScrollbar(int visibleLines) {
  if (textViewerLineCount <= visibleLines) return;

  int trackX = PANEL_X + PANEL_W - 4;
  int trackY = BODY_Y + 20;
  int trackH = FOOTER_Y - 8 - trackY;
  int thumbH = trackH * visibleLines / textViewerLineCount;
  if (thumbH < 16) thumbH = 16;
  int maxTop = textViewerLineCount - visibleLines;
  int thumbY = trackY + (trackH - thumbH) * textViewerTopLine / maxTop;

  tft.fillRect(trackX, trackY, 2, trackH, C_NAV_SLOT);
  tft.fillRect(trackX, thumbY, 2, thumbH, C_BORDER);
}

void drawTextViewerContent() {
  int x = PANEL_X + PANEL_INSET;
  int y = BODY_Y + 20;
  int lineH = 8 * displayTextSize + 4;
  int visibleLines = textViewerVisibleLines();
  int maxChars = textViewerMaxChars();
  const int maxWrapLines = 3;

  textViewerLineCount = countTextFileLines(textViewerPath);
  int maxTop = textViewerLineCount - 1;
  if (maxTop < 0) maxTop = 0;
  if (textViewerTopLine < 0) textViewerTopLine = 0;
  if (textViewerTopLine > maxTop) textViewerTopLine = maxTop;

  tft.fillRect(PANEL_X + PANEL_INSET, BODY_Y + 16, PANEL_W - PANEL_INSET * 2, FOOTER_Y - BODY_Y - 18, C_BG);

  File file = openSdPath(textViewerPath, FILE_READ);
  if (!file) {
    tft.setTextColor(C_ERROR);
    tft.setTextSize(1);
    tft.setCursor(PANEL_X + PANEL_INSET + 12, BODY_Y + 60);
    tft.print("Cannot open file");
    return;
  }

  for (int i = 0; i < textViewerTopLine && file.available(); i++) {
    file.readStringUntil('\n');
  }

  tft.setTextColor(C_TEXT);
  tft.setTextSize(displayTextSize);
  int drawnLines = 0;
  while (drawnLines < visibleLines && file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) line = " ";

    int start = 0;
    for (int wrapLine = 0; wrapLine < maxWrapLines && drawnLines < visibleLines; wrapLine++) {
      String part;
      if (start >= line.length()) {
        if (wrapLine == 0) part = " ";
        else break;
      } else {
        int remaining = line.length() - start;
        int take = remaining > maxChars ? maxChars : remaining;
        int end = start + take;

        if (end < line.length()) {
          int lastSpace = line.lastIndexOf(' ', end);
          if (lastSpace > start + 8) end = lastSpace;
        }

        part = line.substring(start, end);
        part.trim();
        start = end;
        while (start < line.length() && line.charAt(start) == ' ') start++;

        if (wrapLine == maxWrapLines - 1 && start < line.length() && part.length() > 2) {
          part = part.substring(0, part.length() - 2) + "..";
          start = line.length();
        }
      }

      tft.setCursor(x, y);
      tft.print(part);
      y += lineH;
      drawnLines++;
    }

    if (drawnLines < visibleLines) {
      y += 2;
    }
  }

  file.close();
  drawTextViewerScrollbar(visibleLines);
}

void showTextFile(String path) {
  screenMode = SCREEN_TEXT_VIEWER;
  textViewerPath = path;
  textViewerTopLine = 0;
  clearContentArea();
  drawSidebar();
  drawCardBorder(PANEL_X, PANEL_Y, PANEL_W, PANEL_H);
  drawHeader("Viewer");

  if (!sdReady) sdReady = initSDCard();
  File file = openSdPath(path, FILE_READ);
  if (!file) {
    tft.setTextColor(C_ERROR); tft.setCursor(PANEL_X + PANEL_INSET + 12, BODY_Y + 60); tft.print("Cannot open file");
    return;
  }
  // filename
  tft.setTextColor(C_MUTED); tft.setTextSize(1);
  tft.setCursor(PANEL_X + PANEL_INSET, BODY_Y);
  String p = path; if (p.length() > 32) p = ".." + p.substring(p.length()-30);
  tft.print(p);
  tft.drawLine(PANEL_X + PANEL_INSET, BODY_Y + 14, PANEL_X + PANEL_W - PANEL_INSET, BODY_Y + 14, C_BORDER);

  file.close();
  drawTextViewerContent();
}

void handleFileManager(String joy) {
  if (joy == "UP") {
    int old = selectedIndex, oldTop = topIndex;
    selectedIndex--; if (selectedIndex < 0) selectedIndex = fileCount - 1;
    if (selectedIndex < topIndex) topIndex = selectedIndex;
    if (selectedIndex >= topIndex + 7) topIndex = selectedIndex - 6;
    if (topIndex != oldTop) drawFileListContent();
    else { drawFileRow(old, false); drawFileRow(selectedIndex, true); prevSelectedIndex = selectedIndex; }
  }
  else if (joy == "DOWN") {
    int old = selectedIndex, oldTop = topIndex;
    selectedIndex++; if (selectedIndex >= fileCount) selectedIndex = 0;
    if (selectedIndex < topIndex) topIndex = selectedIndex;
    if (selectedIndex >= topIndex + 7) topIndex = selectedIndex - 6;
    if (topIndex != oldTop) drawFileListContent();
    else { drawFileRow(old, false); drawFileRow(selectedIndex, true); prevSelectedIndex = selectedIndex; }
  }
  else if (joy == "SELECT") openSelectedFile();
  else if (joy == "LEFT")   goBackFolder();
}

void handleTextViewer(String joy) {
  if (joy == "UP") {
    textViewerTopLine--;
    drawTextViewerContent();
  } else if (joy == "DOWN") {
    textViewerTopLine++;
    drawTextViewerContent();
  } else if (joy == "LEFT") {
    if (textViewerReturnScreen == SCREEN_SD_INFO) sdInfoScreen();
    else drawFileManager();
  }
}

// ===================== CONFIG =====================

void drawConfigRow(int i, bool selected) {
  if (i == 0) drawMenuRow(i, configIndex, "Mode", getConfigValue(i), 0);
  else if (i == 1) drawMenuRow(i, configIndex, "T33S Power", getConfigValue(i), 1);
}

void drawConfigMenu() {
  screenMode = SCREEN_CONFIG;
  sidebarTab = 6;
  clearContentArea();
  drawSidebar();
  drawCardBorder(PANEL_X, PANEL_Y, PANEL_W, PANEL_H);
  drawHeader("Config");

  tft.setTextColor(C_MUTED); tft.setTextSize(1);
  tft.setCursor(PANEL_X + PANEL_INSET, BODY_Y); tft.print("Radio");

  for (int i = 0; i < configCount; i++) drawConfigRow(i, i == configIndex);

}

void handleConfig(String joy) {
  if (joy == "UP") {
    int old = configIndex;
    configIndex--; if (configIndex < 0) configIndex = configCount - 1;
    drawConfigRow(old, false); drawConfigRow(configIndex, true);
  }
  else if (joy == "DOWN") {
    int old = configIndex;
    configIndex++; if (configIndex >= configCount) configIndex = 0;
    drawConfigRow(old, false); drawConfigRow(configIndex, true);
  }
  else if (joy == "SELECT") {
    optionGroup = 1;
    optionItem = configIndex;
    drawOptionEditor();
  }
  else if (joy == "LEFT") returnToHomeTab();
}

// ===================== SETTINGS =====================

void showRestartLoadingScreen() {
  tft.fillScreen(C_BG);
  tft.setTextSize(1);
  tft.setTextColor(C_ACCENT);
  tft.setCursor(132, 144);
  tft.print("Restarting...");

  int cx = 160;
  int cy = 108;
  int r = 18;
  const int points = 12;

  for (int frame = 0; frame < 24; frame++) {
    tft.fillCircle(cx, cy, r + 5, C_BG);
    for (int i = 0; i < points; i++) {
      int idx = (frame + i) % points;
      float angle = idx * 2.0f * PI / points;
      int dotX = cx + cos(angle) * r;
      int dotY = cy + sin(angle) * r;
      uint16_t col = (i > 8) ? C_ACCENT : (i > 4 ? C_BORDER : C_MUTED);
      int dotR = (i > 8) ? 3 : 2;
      tft.fillCircle(dotX, dotY, dotR, col);
    }
    delay(70);
  }
}

void drawSettingsRow(int i, bool selected) {
  if (i == 0) drawMenuRow(i, settingsIndex, "Brightness", getSettingsValue(i), 2);
  else if (i == 1) drawMenuRow(i, settingsIndex, "Alert sound", getSettingsValue(i), 4);
  else if (i == 2) drawMenuRow(i, settingsIndex, "Sleep mode", getSettingsValue(i), 4);
  else if (i == 3) drawMenuRow(i, settingsIndex, "WiFi AP", getSettingsValue(i), 1);
  else if (i == 4) drawMenuRow(i, settingsIndex, "WiFi Pass", getSettingsValue(i), 4);
  else if (i == 5) drawMenuRow(i, settingsIndex, "Bluetooth BLE", getSettingsValue(i), 1);
}

void drawSettingsMenu() {
  screenMode = SCREEN_SETTINGS;
  sidebarTab = 7;
  clearContentArea();
  drawSidebar();
  drawCardBorder(PANEL_X, PANEL_Y, PANEL_W, PANEL_H);
  drawHeader("Settings");

  for (int i = 0; i < settingsCount; i++) drawSettingsRow(i, i == settingsIndex);

}

void handleSettings(String joy) {
  if (joy == "UP") {
    int old = settingsIndex;
    settingsIndex--; if (settingsIndex < 0) settingsIndex = settingsCount - 1;
    drawSettingsRow(old, false); drawSettingsRow(settingsIndex, true);
  }
  else if (joy == "DOWN") {
    int old = settingsIndex;
    settingsIndex++; if (settingsIndex >= settingsCount) settingsIndex = 0;
    drawSettingsRow(old, false); drawSettingsRow(settingsIndex, true);
  }
  else if (joy == "SELECT") {
    if (settingsIndex == 4) {
      drawWifiPasswordKeyboard();
      return;
    }
    optionGroup = 0;
    optionItem = settingsIndex;
    drawOptionEditor();
  }
  else if (joy == "LEFT") returnToHomeTab();
}

// ===================== OPTION EDITOR =====================

String getOptionTitle() {
  if (optionGroup == 1) {
    if (optionItem == 0) return "Config > Mode";
    return "Config > T33S Power";
  }
  if (optionItem == 0) return "Settings > Brightness";
  if (optionItem == 1) return "Settings > Alert";
  if (optionItem == 2) return "Settings > Sleep";
  if (optionItem == 3) return "Settings > WiFi AP";
  if (optionItem == 4) return "Settings > WiFi Pass";
  return "Settings > Bluetooth";
}

String getOptionValue() {
  if (optionGroup == 1) return getConfigValue(optionItem);
  return getSettingsValue(optionItem);
}

void drawOptionEditorValue() {
  int x = PANEL_X + PANEL_INSET;
  int w = PANEL_W - PANEL_INSET * 2;
  tft.fillRoundRect(x, BODY_Y + 58, w, 54, 5, C_CARD_BG);
  tft.drawRoundRect(x, BODY_Y + 58, w, 54, 5, C_ACCENT);
  tft.setTextSize(2);
  tft.setTextColor(C_ACCENT);
  String value = getOptionValue();
  int vx = x + (w - value.length() * 12) / 2;
  if (vx < x + 8) vx = x + 8;
  tft.setCursor(vx, BODY_Y + 76);
  tft.print(value);
}

void drawOptionEditor() {
  screenMode = SCREEN_OPTION_EDITOR;
  clearContentArea();
  drawSidebar();
  drawCardBorder(PANEL_X, PANEL_Y, PANEL_W, PANEL_H);
  drawHeader(getOptionTitle());

  tft.setTextSize(1);
  tft.setTextColor(C_MUTED);
  tft.setCursor(PANEL_X + PANEL_INSET, BODY_Y + 18);
  tft.print("LEFT / RIGHT to change");

  drawOptionEditorValue();
}

void adjustOption(int dir) {
  bool shouldApplyRadioConfig = false;
  bool shouldSaveSettings = false;

  if (optionGroup == 1) {
    if (optionItem == 0) {
      configModeIndex += dir;
      if (configModeIndex < 0) configModeIndex = 1;
      if (configModeIndex > 1) configModeIndex = 0;
      shouldApplyRadioConfig = true;
    } else {
      configPowerIndex += dir;
      if (configPowerIndex < 0) configPowerIndex = 3;
      if (configPowerIndex > 3) configPowerIndex = 0;
      shouldApplyRadioConfig = true;
    }
  } else {
    if (optionItem == 0) {
      brightnessPercent += dir * 5;
      if (brightnessPercent < MIN_BRIGHTNESS_PERCENT) brightnessPercent = 100;
      if (brightnessPercent > 100) brightnessPercent = MIN_BRIGHTNESS_PERCENT;
      applyBrightness();
      shouldSaveSettings = true;
    } else if (optionItem == 1) {
      alertSoundOn = !alertSoundOn;
      shouldSaveSettings = true;
    } else if (optionItem == 2) {
      sleepModeIndex += dir;
      if (sleepModeIndex < 0) sleepModeIndex = 4;
      if (sleepModeIndex > 4) sleepModeIndex = 0;
      shouldSaveSettings = true;
    } else if (optionItem == 3) {
      wifiApEnabled = !wifiApEnabled;
      initWifiAccessPoint();
      shouldSaveSettings = true;
    } else if (optionItem == 4) {
      return;
    } else {
#if ENABLE_BLE_NETWORK
      bluetoothEnabled = !bluetoothEnabled;
      initBluetoothBle();
      shouldSaveSettings = true;
#else
      bluetoothEnabled = false;
      bluetoothReady = false;
      shouldSaveSettings = true;
#endif
    }
  }

  if (shouldApplyRadioConfig) {
    saveConfigToStorage();
    applyE22Config();
  }
  if (shouldSaveSettings) saveSettingsToStorage();
  drawOptionEditorValue();
}

void handleOptionEditor(String joy) {
  if (joy == "LEFT") adjustOption(-1);
  else if (joy == "RIGHT") adjustOption(1);
  else if (joy == "SELECT" || joy == "UP") {
    if (optionGroup == 1) {
      saveConfigToStorage();
      applyE22Config();
      drawConfigMenu();
    } else {
      saveSettingsToStorage();
      drawSettingsMenu();
    }
  }
}

// ===================== DISPLAY TEST =====================

void drawTftTest() {
  screenMode = SCREEN_SETTINGS; // reuse; back goes to settings
  clearContentArea();
  drawSidebar();
  drawCardBorder(PANEL_X, PANEL_Y, PANEL_W, PANEL_H);
  drawHeader("Display Test");

  tft.setTextSize(1);
  drawIconInfo(PANEL_X + PANEL_INSET, BODY_Y + 8, C_ACCENT);
  tft.setTextColor(C_ACCENT);   tft.setCursor(PANEL_X + PANEL_INSET + 22, BODY_Y + 10);  tft.print("TFT OK - ILI9341");
  tft.setTextColor(C_TEXT);     tft.setCursor(PANEL_X + PANEL_INSET + 22, BODY_Y + 28);  tft.print("320 x 240  Landscape");
  tft.setTextColor(C_MUTED);    tft.setCursor(PANEL_X + PANEL_INSET + 22, BODY_Y + 46);  tft.print("SPI @ 40MHz");
  tft.setTextColor(C_TEXT);     tft.setCursor(PANEL_X + PANEL_INSET + 22, BODY_Y + 64);  tft.print("BL pin: 7  PWM 8-bit");

  // Color swatches
  int sx = PANEL_X + PANEL_INSET, sy = BODY_Y + 96;
  uint16_t swatches[] = {C_ACCENT, C_ERROR, C_WARN, C_TEXT, C_MUTED, C_BORDER};
  for (int i = 0; i < 6; i++) tft.fillRoundRect(sx + i * 30, sy, 24, 24, 3, swatches[i]);

  tft.setTextColor(C_MUTED); tft.setCursor(PANEL_X + PANEL_INSET, BODY_Y + 130);
  tft.print("Brightness: "); tft.print(brightnessPercent); tft.print("%");

}

// ===================== JOYSTICK TEST =====================

void joystickTestScreen() {
  clearContentArea();
  drawSidebar();
  drawCardBorder(PANEL_X, PANEL_Y, PANEL_W, PANEL_H);
  drawHeader("Joystick Debug");
  tft.setTextSize(1); tft.setTextColor(C_MUTED);
  tft.setCursor(PANEL_X + PANEL_INSET, BODY_Y + 16); tft.print("ADC Raw");
  tft.setCursor(PANEL_X + PANEL_INSET, BODY_Y + 76); tft.print("Direction");
}

void updateJoystickTest() {
  static int lastV = -1; static String lastD = "";
  int v = readJoystickSmooth(); String d = getDirection(v);
  if (abs(v - lastV) > 20 || d != lastD) {
    tft.fillRect(PANEL_X + PANEL_INSET, BODY_Y + 31, 180, 30, C_CARD_BG);
    tft.fillRect(PANEL_X + PANEL_INSET, BODY_Y + 91, 180, 20, C_CARD_BG);
    tft.setTextSize(2); tft.setTextColor(C_TEXT);
    tft.setCursor(PANEL_X + PANEL_INSET, BODY_Y + 32); tft.print(v);
    tft.setTextSize(1); tft.setTextColor(C_ACCENT);
    tft.setCursor(PANEL_X + PANEL_INSET, BODY_Y + 92);
    if (d == "") tft.print("--"); else tft.print(d);
    lastV = v; lastD = d;
  }
}

// ===================== SD INFO =====================

const int CHAT_RECORD_VISIBLE = 5;

void ensureChatRecordVisible() {
  if (fileCount <= 0) {
    selectedIndex = 0;
    topIndex = 0;
    return;
  }
  if (selectedIndex < 0) selectedIndex = fileCount - 1;
  if (selectedIndex >= fileCount) selectedIndex = 0;
  if (selectedIndex < topIndex) topIndex = selectedIndex;
  if (selectedIndex >= topIndex + CHAT_RECORD_VISIBLE) topIndex = selectedIndex - CHAT_RECORD_VISIBLE + 1;
  int maxTop = fileCount - CHAT_RECORD_VISIBLE;
  if (maxTop < 0) maxTop = 0;
  if (topIndex > maxTop) topIndex = maxTop;
  if (topIndex < 0) topIndex = 0;
}

void loadChatRecords() {
  prepareSdBus();
  if (sdReady && !SD.exists(CHAT_LOG_DIR)) SD.mkdir(CHAT_LOG_DIR);
  currentPath = CHAT_LOG_DIR;
  loadFiles(currentPath);
}

void drawChatRecordRow(int index, int slot, bool selected) {
  int x = PANEL_X + PANEL_INSET;
  int y = BODY_Y + 92 + slot * 24;
  int w = PANEL_W - PANEL_INSET * 2;
  uint16_t fillCol = C_CARD_BG;
  uint16_t textCol = selected ? C_ACCENT : C_TEXT;

  tft.fillRoundRect(x, y, w, 22, 3, fillCol);
  if (selected) tft.drawRoundRect(x, y, w, 22, 3, C_ACCENT);
  drawIconFile(x + 7, y + 3, selected ? C_ACCENT : C_MUTED);
  tft.setTextSize(1);
  tft.setTextColor(textCol);
  tft.setCursor(x + 28, y + 7);
  String name = fileNames[index];
  if (name.length() > 27) name = name.substring(0, 27) + "..";
  tft.print(name);
}

void drawChatRecordList() {
  ensureChatRecordVisible();
  tft.fillRect(PANEL_X + PANEL_INSET, BODY_Y + 86, PANEL_W - PANEL_INSET * 2, 128, C_BG);

  tft.setTextSize(1);
  tft.setTextColor(C_MUTED);
  tft.setCursor(PANEL_X + PANEL_INSET, BODY_Y + 78);
  tft.print("Chat records");

  if (fileCount == 0) {
    tft.setTextColor(C_MUTED);
    tft.setCursor(PANEL_X + PANEL_INSET + 28, BODY_Y + 132);
    tft.print("No records yet");
    return;
  }

  for (int slot = 0; slot < CHAT_RECORD_VISIBLE; slot++) {
    int idx = topIndex + slot;
    if (idx >= fileCount) break;
    drawChatRecordRow(idx, slot, idx == selectedIndex);
  }

  if (fileCount > CHAT_RECORD_VISIBLE) {
    int trackX = PANEL_X + PANEL_W - 4;
    int trackY = BODY_Y + 92;
    int trackH = CHAT_RECORD_VISIBLE * 24 - 2;
    int barH = trackH * CHAT_RECORD_VISIBLE / fileCount;
    if (barH < 16) barH = 16;
    int barY = trackY + (trackH - barH) * topIndex / (fileCount - CHAT_RECORD_VISIBLE);
    tft.fillRect(trackX, trackY, 2, trackH, C_NAV_SLOT);
    tft.fillRect(trackX, barY, 2, barH, C_BORDER);
  }
}

void openSelectedChatRecord() {
  if (fileCount == 0 || selectedIndex < 0 || selectedIndex >= fileCount) return;
  if (fileIsDir[selectedIndex]) return;

  String path = String(CHAT_LOG_DIR) + "/" + fileNames[selectedIndex];
  textViewerReturnScreen = SCREEN_SD_INFO;
  showTextFile(path);
}

void handleSdInfo(String joy) {
  if (joy == "UP") {
    int old = selectedIndex;
    selectedIndex--;
    ensureChatRecordVisible();
    if (old != selectedIndex) drawChatRecordList();
  } else if (joy == "DOWN") {
    int old = selectedIndex;
    selectedIndex++;
    ensureChatRecordVisible();
    if (old != selectedIndex) drawChatRecordList();
  } else if (joy == "SELECT") {
    openSelectedChatRecord();
  } else if (joy == "LEFT") {
    returnToHomeTab();
  }
}

void sdInfoScreen() {
  screenMode = SCREEN_SD_INFO;
  sidebarTab = 5;
  clearContentArea();
  drawSidebar();
  drawCardBorder(PANEL_X, PANEL_Y, PANEL_W, PANEL_H);
  drawHeader("SD Records");

  int rx = PANEL_X + PANEL_INSET;
  if (!sdReady) {
    sdReady = initSDCard();
  }
  if (!sdReady) {
    drawIconSd(rx, BODY_Y + 24, C_ERROR);
    tft.setTextColor(C_ERROR); tft.setTextSize(1);
    tft.setCursor(rx + 22, BODY_Y + 30); tft.print("SD not found");
    tft.setTextColor(C_MUTED);
    tft.setCursor(rx + 22, BODY_Y + 50); tft.print("CS="); tft.print(SD_CS); tft.print(" MOSI="); tft.print(SD_MOSI);
    tft.setCursor(rx + 22, BODY_Y + 65); tft.print("SCK="); tft.print(SD_SCLK); tft.print(" MISO="); tft.print(SD_MISO);
    tft.setCursor(rx + 22, BODY_Y + 80); tft.print("Format: FAT32");
  } else {
    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    drawIconSd(rx, BODY_Y + 14, C_ACCENT);
    tft.setTextColor(C_ACCENT); tft.setTextSize(1);
    tft.setCursor(rx + 22, BODY_Y + 20); tft.print("SD Mounted  OK");
    tft.setTextColor(C_TEXT);
    tft.setCursor(rx + 22, BODY_Y + 40); tft.print("Size: "); tft.print((unsigned long)cardSize); tft.print(" MB");
    uint8_t t = SD.cardType();
    tft.setCursor(rx + 22, BODY_Y + 58); tft.print("Type: ");
    if (t == CARD_MMC) tft.print("MMC");
    else if (t == CARD_SD) tft.print("SDSC");
    else if (t == CARD_SDHC) tft.print("SDHC");
    else tft.print("Unknown");
    loadChatRecords();
    drawChatRecordList();
  }
}

// ===================== SD INIT =====================

bool initSDCard() {
  prepareSdBus();
  delay(100);

  SD.end();
  sdSPI.end();
  beginSdSpiBus();

  if (!SD.begin(SD_CS, sdSPI, SD_SPI_FREQ)) {
    Serial.println("SD mount failed");
    return false;
  }
  if (SD.cardType() == CARD_NONE) {
    Serial.println("No SD card detected");
    SD.end();
    return false;
  }

  Serial.print("SD OK, card type ");
  Serial.println(SD.cardType());
  return true;
}

void printHardwareReport() {
  Serial.print("Target: ");
  Serial.println(HARDWARE_TARGET);
  Serial.print("Flash: ");
  Serial.print(ESP.getFlashChipSize() / (1024 * 1024));
  Serial.println(" MB");
  Serial.print("PSRAM: ");
  Serial.print(ESP.getPsramSize() / (1024 * 1024));
  Serial.println(" MB");
}

void printSpiPinReport() {
  Serial.println("SPI wiring:");
  Serial.printf("  TFT: MOSI GPIO%d, SCLK GPIO%d, MISO GPIO%d, CS GPIO%d\n", TFT_MOSI, TFT_SCLK, TFT_MISO, TFT_CS);
  Serial.printf("  SD:  MOSI GPIO%d, SCLK GPIO%d, MISO GPIO%d, CS GPIO%d\n", SD_MOSI, SD_SCLK, SD_MISO, SD_CS);
  Serial.println("  TFT and SD use separate SPI pins.");
}

// ===================== SETUP =====================

void drawStartupLogo() {
  tft.drawRGBBitmap(0, 0, STARTUP_LOGO, STARTUP_LOGO_W, STARTUP_LOGO_H);

  const int barW = 180;
  const int barH = 8;
  const int barX = (320 - barW) / 2;
  const int barY = 202;
  const int innerW = barW - 4;
  const unsigned long duration = 4000UL;
  const unsigned long started = millis();
  int lastFill = -1;

  tft.drawRoundRect(barX, barY, barW, barH, 4, C_MUTED);
  tft.fillRoundRect(barX + 2, barY + 2, innerW, barH - 4, 2, C_BG);

  while (millis() - started < duration) {
    unsigned long elapsed = millis() - started;
    int fillW = (int)(elapsed * innerW / duration);
    if (fillW != lastFill) {
      tft.fillRoundRect(barX + 2, barY + 2, fillW, barH - 4, 2, C_TEXT);
      lastFill = fillW;
    }
    delay(20);
  }

  tft.fillRoundRect(barX + 2, barY + 2, innerW, barH - 4, 2, C_TEXT);
}

void setup() {
  Serial.begin(115200); delay(500);
  initBuzzer();
  loadStoredOptions();

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  // ESP32 Arduino core 3.x backlight PWM: ledcAttach(pin, freq, resolution)
  ledcAttach(TFT_BL, BL_FREQ, BL_RES);
  applyBrightness();

  printHardwareReport();
  initNetworkDevices();
  initGps();
  initE22Radio();

  pinMode(JOY_PIN, INPUT_PULLUP);
  pinMode(BATTERY_PIN, INPUT);
  pinMode(BACK_BTN_PIN, INPUT_PULLUP);
  pinMode(HOME_BTN_PIN, INPUT_PULLUP);
  analogReadResolution(12);
  analogSetPinAttenuation(JOY_PIN, ADC_11db);
  analogSetPinAttenuation(BATTERY_PIN, ADC_11db);

  pinMode(TFT_CS, OUTPUT); pinMode(SD_CS, OUTPUT);
  digitalWrite(TFT_CS, HIGH); digitalWrite(SD_CS, HIGH);
  beginSharedSpiBus();
  beginSdSpiBus();
  printSpiPinReport();

  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(C_BG);
  drawStartupLogo();
  delay(1000);
  
  sdReady = initSDCard();
  drawHomeScreen();
  lastActivityTime = millis();
}

// ===================== LOOP =====================

void loop() {
  pollGps();
  pollE22Radio();
  sendNodeBeacon();

  if (!displaySleeping && millis() - lastBrightnessRefresh > 1000UL) {
    lastBrightnessRefresh = millis();
    applyBrightness();
  }

  if (!displaySleeping && screenMode == SCREEN_USER && millis() - lastNodeUiRefresh > 5000UL) {
    lastNodeUiRefresh = millis();
    drawUserContent();
  }

  if (!displaySleeping && screenMode == SCREEN_HOME && millis() - lastHomeClockRefresh > 60000UL) {
    drawHomeClockValues();
  }

  if (!displaySleeping && screenMode == SCREEN_HOME && millis() - lastHomeTopStatusRefresh > 1000UL) {
    drawHomeTopStatusIcons();
  }

  if (!displaySleeping && screenMode == SCREEN_LOCATION && millis() - lastGpsUiRefresh > 1000UL) {
    lastGpsUiRefresh = millis();
    drawLocationContent(false);
  }

  String joy = getJoyPressed();

  if (joy != "") {
    if (displaySleeping) {
      wakeDisplay();
      delay(20);
      return;
    }
    lastActivityTime = millis();
  }

  if (joy == "HOME") {
    returnToHomeTab();
    delay(20);
    return;
  }

  unsigned long sleepTimeout = getSleepTimeoutMs();
  if (!displaySleeping && sleepTimeout > 0 && millis() - lastActivityTime >= sleepTimeout) {
    sleepDisplay();
  }

  if (screenMode == SCREEN_HOME) {
    handleHomeScreen(joy);
  }
  else if (screenMode == SCREEN_HOME_DETAILS) {
    handleHomeDetailsScreen(joy);
  }
  else if (screenMode == SCREEN_USER) {
    handleUserScreen(joy);
  }
  else if (screenMode == SCREEN_CHAT_PRESETS) {
    handleChatPresets(joy);
  }
  else if (screenMode == SCREEN_CHAT_KEYBOARD) {
    handleFreeTextKeyboard(joy);
  }
  else if (screenMode == SCREEN_CHAT_SEND_TO) {
    handleChatSendTarget(joy);
  }
  else if (screenMode == SCREEN_MAP) {
    handleMapScreen(joy);
  }
  else if (screenMode == SCREEN_LOCATION) {
    handleLocationScreen(joy);
  }
  else if (screenMode == SCREEN_FILE_MANAGER) {
    handleFileManager(joy);
  }
  else if (screenMode == SCREEN_TEXT_VIEWER) {
    handleTextViewer(joy);
  }
  else if (screenMode == SCREEN_SETTINGS) {
    handleSettings(joy);
  }
  else if (screenMode == SCREEN_CONFIG) {
    handleConfig(joy);
  }
  else if (screenMode == SCREEN_OPTION_EDITOR) {
    handleOptionEditor(joy);
  }
  else if (screenMode == SCREEN_SD_INFO) {
    handleSdInfo(joy);
  }
  else if (screenMode == SCREEN_ABOUT) {
    if (joy == "LEFT") returnToHomeTab();
  }

  delay(20);
}
