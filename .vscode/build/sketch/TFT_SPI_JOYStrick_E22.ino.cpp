#include <Arduino.h>
#line 1 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
#include <SPI.h>
#include <SD.h>
#include <FS.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include "sidebar_icons.h"

// ===================== PIN SETUP =====================

#define TFT_CS    10
#define TFT_DC    8
#define TFT_RST   9
#define TFT_MOSI  11
#define TFT_SCLK  12
#define TFT_MISO  13
#define TFT_BL    7
#define SD_CS     5
#define JOY_PIN   4

#define BL_FREQ     5000
#define BL_RES      8
#define SD_SPI_FREQ 4000000

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

// ===================== MESHTASTIC PALETTE =====================
// Screen is landscape 320x240

#define C_BG         0x0000      // pure black right display background
#define C_SIDEBAR    0x0861      // deep green left navigation
#define C_NAV_SLOT   0x10A2      // idle navigation slot
#define C_CARD_BG    0x0841      // near-black panel surface
#define C_BORDER     0x05E6      // softer teal/green border
#define C_ACCENT     0x07E0      // bright green accent / selected
#define C_TEXT       0xFFFF      // white
#define C_MUTED      0x9CF3      // readable muted text
#define C_ICON_ACT   0x0000      // active icon on green
#define C_ICON_IDLE  0x6B6D      // idle icon grey
#define C_HEADER_BG  0x0022      // black-green header
#define C_WARN       0xFD20
#define C_ERROR      0xF800
#define C_SEL_BG     0x0200      // dark green row highlight

// Sidebar width
#define SIDEBAR_W    44
// Content area starts at
#define CONTENT_X    (SIDEBAR_W + 2)
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

#line 63 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void applyBrightness();
#line 178 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void drawIconHome(int x, int y, uint16_t col);
#line 187 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void drawIconNodes(int x, int y, uint16_t col);
#line 196 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void drawIconMsg(int x, int y, uint16_t col);
#line 203 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void drawIconMap(int x, int y, uint16_t col);
#line 210 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void drawIconSettings(int x, int y, uint16_t col);
#line 221 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void drawIconConfig(int x, int y, uint16_t col);
#line 230 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void drawIconFile(int x, int y, uint16_t col);
#line 237 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void drawIconFolder(int x, int y, uint16_t col);
#line 244 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void drawIconSun(int x, int y, uint16_t col);
#line 252 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void drawIconSignal(int x, int y, uint16_t col);
#line 259 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void drawIconSd(int x, int y, uint16_t col);
#line 266 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void drawIconInfo(int x, int y, uint16_t col);
#line 272 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void drawSidebarIconSized(int tabIndex, int centerX, int centerY, bool active, int size);
#line 277 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void drawSidebarIcon(int tabIndex, int x, int y, uint16_t col);
#line 283 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void drawHeader(String title);
#line 295 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void ensureSidebarVisible();
#line 304 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void drawSidebarScrollbar();
#line 317 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void drawSidebarSlot(int i, bool active, int iconSize);
#line 335 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void animateSidebarSelection(int oldTab, int newTab, bool movedWindow);
#line 369 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void drawSidebar();
#line 382 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void drawBottomHint(String text);
#line 392 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void drawCardBorder(int x, int y, int w, int h);
#line 398 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void drawStatusRow(int x, int y, int icon, String label, String value, uint16_t valueCol);
#line 415 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void drawMenuRow(int i, int selectedIndexValue, String label, String value, int icon);
#line 446 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
String getConfigValue(int i);
#line 452 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
String getSettingsValue(int i);
#line 461 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void drawHomeScreen();
#line 510 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void drawInformationScreen();
#line 553 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void drawAboutScreen();
#line 598 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void openTab(int tab);
#line 625 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void handleTabNav(String joy);
#line 648 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void drawSidebarTabOnly(int i, bool active);
#line 654 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void ensureChatPresetVisible();
#line 665 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void drawChatPresetRow(int presetIndex, int slot, bool selected);
#line 685 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void drawChatPresetList();
#line 708 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void drawChatStatus(String text, uint16_t col);
#line 719 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void drawChatPresetScreen();
#line 744 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void handleChatPresets(String joy);
#line 773 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void drawFreeTextValue();
#line 790 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void drawKeyboardKey(int i, bool selected);
#line 815 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void drawKeyboardGrid();
#line 822 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void drawFreeTextKeyboard();
#line 834 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void pressKeyboardKey();
#line 857 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void moveKeyboardSelection(int nextIndex);
#line 866 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void handleFreeTextKeyboard(String joy);
#line 882 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void drawChatSendBanner(String text, uint16_t col);
#line 894 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void drawSendTargetRow(int i, bool selected);
#line 912 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void drawChatSendTargetScreen();
#line 930 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void handleChatSendTarget(String joy);
#line 955 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void prepareSdBus();
#line 970 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void loadFiles(String path);
#line 990 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void drawFileRow(int index, bool selected);
#line 1018 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void drawFileManager();
#line 1066 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void goBackFolder();
#line 1074 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void openSelectedFile();
#line 1082 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void showTextFile(String path);
#line 1117 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void handleFileManager(String joy);
#line 1140 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void drawConfigRow(int i, bool selected);
#line 1145 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void drawConfigMenu();
#line 1161 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void handleConfig(String joy);
#line 1182 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void drawSettingsRow(int i, bool selected);
#line 1188 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void drawSettingsMenu();
#line 1201 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void handleSettings(String joy);
#line 1222 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
String getOptionTitle();
#line 1232 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
String getOptionValue();
#line 1237 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void drawOptionEditorValue();
#line 1251 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void drawOptionEditor();
#line 1267 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void adjustOption(int dir);
#line 1295 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void handleOptionEditor(String joy);
#line 1306 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void drawTftTest();
#line 1333 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void joystickTestScreen();
#line 1344 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void updateJoystickTest();
#line 1361 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void sdInfoScreen();
#line 1401 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
bool initSDCard();
#line 1415 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void setup();
#line 1454 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void loop();
#line 3 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/SystemInput.ino"
unsigned long getSleepTimeoutMs();
#line 11 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/SystemInput.ino"
void sleepDisplay();
#line 17 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/SystemInput.ino"
void wakeDisplay();
#line 26 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/SystemInput.ino"
int readJoystickSmooth();
#line 32 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/SystemInput.ino"
String getDirection(int value);
#line 41 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/SystemInput.ino"
String getJoyPressed();
#line 63 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/TFT_SPI_JOYStrick_E22.ino"
void applyBrightness() {
  // ESP32 Arduino core 3.x: ledcWrite(pin, duty)
  ledcWrite(TFT_BL, brightnessPercent * 255 / 100);
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

enum ScreenMode {
  SCREEN_HOME,
  SCREEN_CHAT_PRESETS,
  SCREEN_CHAT_KEYBOARD,
  SCREEN_CHAT_SEND_TO,
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

void returnToHomeTab();

// Sidebar tab index (0=Home,1=Information,2=Files,3=Map,4=Config,5=Settings,6=About)
int sidebarTab = 0;
const int SIDEBAR_TABS = 7;
int sidebarTopTab = 0;
const int SIDEBAR_VISIBLE_TABS = 6;
const int SIDEBAR_TAB_Y = 12;
const int SIDEBAR_TAB_STEP = 38;

// Config menu
int configIndex = 0;
const int configCount = 2;
int configModeIndex = 0;   // Client, Repeater
int configPowerIndex = 2;  // 10dBm, 13dBm, 17dBm, 22dBm

// Settings menu
int settingsIndex = 0;
const int settingsCount = 3;
int displayTextSize = 1;
bool alertSoundOn = true;
int sleepModeIndex = 4;    // 10s, 30s, 1m, 5m, Never
unsigned long lastActivityTime = 0;
bool displaySleeping = false;

int optionGroup = 0;       // 0=settings, 1=config
int optionItem = 0;

const char* configModes[] = {"Client", "Repeater"};
const char* configPowers[] = {"10dBm", "13dBm", "17dBm", "22dBm"};
const char* textSizes[] = {"Font size 1", "Font size 2", "Font size 3"};
const char* alertSounds[] = {"OFF", "ON"};
const char* sleepModes[] = {"10s", "30s", "1m", "5m", "Never"};
const char* appVersion = "v0.0.20";
const char* username = "Karon";

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
int keyboardIndex = 0;
int sendTargetIndex = 0;
const int sendTargetCount = 3;
const char* sendTargets[] = {"All Broadcast", "Node 1", "Node 2"};
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

void drawIconMap(int x, int y, uint16_t col) {
  tft.drawRect(x, y+2, 15, 11, col);
  tft.drawLine(x+5, y+2, x+4, y+13, col);
  tft.drawLine(x+10, y+2, x+11, y+13, col);
  tft.fillCircle(x+8, y+7, 2, col);
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

void drawSidebarIconSized(int tabIndex, int centerX, int centerY, bool active, int size) {
  const uint16_t* bitmap = getSidebarIconBitmap(tabIndex, active, size);
  tft.drawRGBBitmap(centerX - size / 2, centerY - size / 2, bitmap, size, size);
}

void drawSidebarIcon(int tabIndex, int x, int y, uint16_t col) {
  drawSidebarIconSized(tabIndex, x + 7, y + 7, col == C_ICON_ACT, SIDEBAR_ICON_LARGE);
}

// ===================== LAYOUT PRIMITIVES =====================

void drawHeader(String title) {
  tft.fillRoundRect(PANEL_X, PANEL_Y, PANEL_W + PANEL_INSET * 2, 20, 5, C_BG);
  tft.drawRoundRect(PANEL_X, PANEL_Y, PANEL_W + PANEL_INSET * 2, 20, 5, C_BG);
  tft.setTextSize(1);
  tft.setTextColor(C_TEXT);
  // center title
  int cx = PANEL_X + (PANEL_W - title.length() * 6) / 2;
  if (cx < PANEL_X + PANEL_INSET) cx = PANEL_X + PANEL_INSET;
  tft.setCursor(cx, TITLE_Y);
  tft.print(title);
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
  if (i < sidebarTopTab || i >= sidebarTopTab + SIDEBAR_VISIBLE_TABS) return;

  int ty = SIDEBAR_TAB_Y + (i - sidebarTopTab) * SIDEBAR_TAB_STEP;
  int centerX = SIDEBAR_W / 2;
  int centerY = ty + 9;

  tft.fillRect(0, ty - 9, SIDEBAR_W - 1, 41, C_BG);
  if (active) {
    drawSidebarIconSized(i, centerX, centerY, true, iconSize);
  } else {
    drawSidebarIconSized(i, centerX, centerY, false, SIDEBAR_ICON_IDLE);
  }

  tft.drawFastVLine(SIDEBAR_W, 0, 240, C_BORDER);
  tft.drawFastVLine(SIDEBAR_W + 1, 0, 240, C_BG);
}

void animateSidebarSelection(int oldTab, int newTab, bool movedWindow) {
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
  tft.fillRect(0, 0, SIDEBAR_W, 240, C_BG);
  ensureSidebarVisible();

  int visibleCount = SIDEBAR_TABS < SIDEBAR_VISIBLE_TABS ? SIDEBAR_TABS : SIDEBAR_VISIBLE_TABS;
  for (int slot = 0; slot < visibleCount; slot++) {
    int i = sidebarTopTab + slot;
    drawSidebarSlot(i, i == sidebarTab, SIDEBAR_ICON_LARGE);
  }

  drawSidebarScrollbar();
}

void drawBottomHint(String text) {
  // tft.fillRect(PANEL_X + PANEL_INSET, FOOTER_Y - 2, PANEL_W - PANEL_INSET * 2, 12, C_CARD_BG);
  // tft.drawFastHLine(PANEL_X + PANEL_INSET, FOOTER_Y - 4, PANEL_W - PANEL_INSET * 2, C_NAV_SLOT);
  tft.setTextSize(1);
  tft.setTextColor(C_MUTED);
  tft.setCursor(PANEL_X + PANEL_INSET, FOOTER_Y);
  tft.print(text);
}

// Draw card border (rounded rect with teal border)
void drawCardBorder(int x, int y, int w, int h) {
  // tft.fillRoundRect(x, y, w, h, 4, C_CARD_BG);
  // tft.drawRoundRect(x, y, w, h, 5, C_BORDER);
  // tft.drawFastHLine(x + 6, y + 1, w - 12, C_NAV_SLOT);
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

void drawMenuRow(int i, int selectedIndexValue, String label, String value, int icon) {
  int y = BODY_Y + 12 + i * 30;
  int x = PANEL_X + PANEL_INSET;
  int w = PANEL_W - PANEL_INSET * 2;
  bool selected = (i == selectedIndexValue);

  if (selected) {
    tft.fillRoundRect(x, y, w, 26, 3, C_SEL_BG);
    tft.drawRoundRect(x, y, w, 26, 3, C_BORDER);
  } else {
    tft.fillRoundRect(x, y, w, 26, 3, C_CARD_BG);
  }

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
  return "";
}

// ===================== HOME SCREEN =====================

void drawHomeScreen() {
  screenMode = SCREEN_HOME;
  sidebarTab = 0;
  tft.fillScreen(C_BG);
  drawSidebar();

  // Main info card
  int cx = PANEL_X;
  int cy = PANEL_Y;
  int cw = PANEL_W;
  int ch = PANEL_H;
  drawCardBorder(cx, cy, cw, ch);
  drawHeader("Home");

  int rx = cx + PANEL_INSET;
  tft.setTextColor(C_MUTED);
  tft.setTextSize(1);
  int vx = PANEL_X + PANEL_W - strlen(appVersion) * 6 - PANEL_INSET;
  tft.setCursor(vx, TITLE_Y);
  tft.print(appVersion);

  drawIconMsg(rx + 8, BODY_Y + 28, C_ACCENT);
  tft.setTextColor(C_ACCENT);
  tft.setTextSize(2);
  tft.setCursor(rx + 36, BODY_Y + 28);
  tft.print("Lomhor");
  tft.setCursor(rx + 36, BODY_Y + 50);
  tft.print("Chat");

  tft.setTextSize(1);
  tft.setTextColor(C_MUTED);
  tft.setCursor(rx + 8, BODY_Y + 94);
  tft.print("Node Online");
  tft.setTextColor(C_ACCENT);
  tft.setCursor(rx + 104, BODY_Y + 94);
  tft.print("2 of 3");

  tft.setTextColor(C_MUTED);
  tft.setCursor(rx + 8, BODY_Y + 116);
  tft.print("Username");
  tft.setTextColor(C_TEXT);
  tft.setCursor(rx + 104, BODY_Y + 116);
  tft.print(username);

  drawBottomHint("UP/DN tab  SEL open");
}

// ===================== INFORMATION SCREEN =====================

void drawInformationScreen() {
  screenMode = SCREEN_HOME;
  sidebarTab = 1;
  tft.fillScreen(C_BG);
  drawSidebar();

  int cx = PANEL_X;
  int cy = PANEL_Y;
  int cw = PANEL_W;
  int ch = PANEL_H;
  drawCardBorder(cx, cy, cw, ch);
  drawHeader("Information");

  int rx = cx + PANEL_INSET;
  int ry = BODY_Y;
  drawStatusRow(rx, ry, 0, "Messages", "no new messages", C_TEXT);
  ry += 23;
  drawStatusRow(rx, ry, 1, "Mesh", "2 of 3 nodes online", C_ACCENT);
  ry += 23;
  drawStatusRow(rx, ry, 4, "Time", "--:--:-- GMT  Sat 12-Apr-25", C_TEXT);
  ry += 23;
  drawStatusRow(rx, ry, 2, "Radio", "LoRa 906.875 MHz  250 kHz", C_ACCENT);
  ry += 23;
  drawStatusRow(rx, ry, 2, "Signal", "SNR 6.2  RSSI -22  99%", C_ACCENT);
  ry += 23;
  drawStatusRow(rx, ry, 4, "Alerts", "silent", C_MUTED);
  ry += 23;
  if (sdReady) {
    drawStatusRow(rx, ry, 3, "Storage", "SD OK", C_ACCENT);
  } else {
    drawStatusRow(rx, ry, 3, "Storage", "SD not found", C_ERROR);
  }
  ry += 23;
  String heapText = "Heap ";
  heapText += String(ESP.getFreeHeap() / 1024);
  heapText += "k";
  drawStatusRow(rx, ry, 4, "System", heapText, C_TEXT);

  drawBottomHint("UP/DN tab  SEL open");
}

// ===================== ABOUT SCREEN =====================

void drawAboutScreen() {
  screenMode = SCREEN_ABOUT;
  sidebarTab = 6;
  tft.fillScreen(C_BG);
  drawSidebar();

  drawCardBorder(PANEL_X, PANEL_Y, PANEL_W, PANEL_H);
  drawHeader("About");

  int x = PANEL_X + PANEL_INSET;
  int y = BODY_Y + 14;

  drawIconInfo(x, y, C_ACCENT);
  tft.setTextSize(1);
  tft.setTextColor(C_ACCENT);
  tft.setCursor(x + 24, y + 2);
  tft.print("Lomhor Chat");
  tft.setTextColor(C_MUTED);
  tft.setCursor(x + 24, y + 16);
  tft.print(appVersion);

  y += 48;
  tft.setTextColor(C_TEXT);
  tft.setCursor(x, y);
  tft.print("LoRa chat of Cambodia.");
  y += 18;
  tft.setTextColor(C_MUTED);
  tft.setCursor(x, y);
  tft.print("Offline mesh messaging");
  y += 14;
  tft.setCursor(x, y);
  tft.print("for local communities,");
  y += 14;
  tft.setCursor(x, y);
  tft.print("field teams, and makers.");
  y += 24;
  tft.setTextColor(C_ACCENT);
  tft.setCursor(x, y);
  tft.print("Cambodia LoRa network");

  drawBottomHint("LEFT back");
}

// ===================== MAIN MENU (sidebar navigation) =====================

void openTab(int tab) {
  sidebarTab = tab;
  if (tab == 0) { drawHomeScreen(); return; }
  if (tab == 1) { drawInformationScreen(); return; }
  if (tab == 2) { drawChatPresetScreen(); return; }
  if (tab == 4) { drawConfigMenu(); return; }
  if (tab == 5) { drawSettingsMenu(); return; }
  if (tab == 6) { drawAboutScreen(); return; }

  // Tab 3 (Map) - placeholder
  tft.fillScreen(C_BG);
  sidebarTab = tab;
  drawSidebar();
  String titles[] = {"Home","Information","Files","Map","Config","Settings","About"};
  int cx = PANEL_X, cy = PANEL_Y;
  drawCardBorder(cx, cy, PANEL_W, PANEL_H);
  drawHeader(titles[tab]);
  tft.setTextColor(C_MUTED); tft.setTextSize(1);
  tft.setCursor(cx + PANEL_INSET + 12, cy + 96);
  tft.print("Coming soon...");
  drawBottomHint("UP/DN tab  LEFT back");
}

void returnToHomeTab() {
  openTab(0);
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
  uint16_t bubbleCol = selected ? C_SEL_BG : C_CARD_BG;
  uint16_t borderCol = selected ? C_ACCENT : C_NAV_SLOT;
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
  int y = FOOTER_Y - 16;
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
  tft.fillScreen(C_BG);
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
    String sent = "Ready: ";
    sent += chatPresets[chatPresetIndex];
    drawChatStatus(sent, C_ACCENT);
  }
  else if (joy == "LEFT") {
    returnToHomeTab();
  }
}

// ===================== FREE TEXT KEYBOARD =====================

void drawFreeTextValue() {
  int x = PANEL_X + PANEL_INSET;
  int y = BODY_Y + 8;
  int w = PANEL_W - PANEL_INSET * 2;
  tft.fillRoundRect(x, y, w, 34, 5, C_CARD_BG);
  tft.drawRoundRect(x, y, w, 34, 5, C_BORDER);

  String shown = freeTextMessage;
  if (shown.length() == 0) shown = "...";
  if (shown.length() > 30) shown = shown.substring(shown.length() - 30);

  tft.setTextSize(1);
  tft.setTextColor(freeTextMessage.length() == 0 ? C_MUTED : C_TEXT);
  tft.setCursor(x + 8, y + 13);
  tft.print(shown);
}

void drawKeyboardKey(int i, bool selected) {
  int row = i / KEYBOARD_COLS;
  int col = i % KEYBOARD_COLS;
  int keyW = 29;
  int keyH = 19;
  int gap = 3;
  int x0 = PANEL_X + PANEL_INSET + 1;
  int y0 = BODY_Y + 52;
  int x = x0 + col * (keyW + gap);
  int y = y0 + row * (keyH + gap);
  uint16_t fillCol = selected ? C_SEL_BG : C_CARD_BG;
  uint16_t borderCol = selected ? C_ACCENT : C_NAV_SLOT;
  uint16_t textCol = selected ? C_ACCENT : C_TEXT;

  tft.fillRoundRect(x, y, keyW, keyH, 3, fillCol);
  tft.drawRoundRect(x, y, keyW, keyH, 3, borderCol);
  tft.setTextSize(1);
  tft.setTextColor(textCol);
  String label = keyboardKeys[i];
  int tx = x + (keyW - label.length() * 6) / 2;
  if (tx < x + 2) tx = x + 2;
  tft.setCursor(tx, y + 6);
  tft.print(label);
}

void drawKeyboardGrid() {
  tft.fillRect(PANEL_X + PANEL_INSET, BODY_Y + 49, PANEL_W - PANEL_INSET * 2, 136, C_BG);
  for (int i = 0; i < keyboardKeyCount; i++) {
    drawKeyboardKey(i, i == keyboardIndex);
  }
}

void drawFreeTextKeyboard() {
  screenMode = SCREEN_CHAT_KEYBOARD;
  sidebarTab = 2;
  tft.fillScreen(C_BG);
  drawSidebar();
  drawCardBorder(PANEL_X, PANEL_Y, PANEL_W, PANEL_H);
  drawHeader("Free Text");
  drawFreeTextValue();
  drawKeyboardGrid();
  // drawBottomHint("JOY move  SEL key  BACK exits");
}

void pressKeyboardKey() {
  String key = keyboardKeys[keyboardIndex];
  if (key == "SP") {
    if (freeTextMessage.length() < 64) freeTextMessage += " ";
  } else if (key == "DEL") {
    if (freeTextMessage.length() > 0) freeTextMessage.remove(freeTextMessage.length() - 1);
  } else if (key == "BACK") {
    drawChatPresetScreen();
    return;
  } else if (key == "SEND") {
    if (freeTextMessage.length() == 0) {
      drawBottomHint("Type message first");
      return;
    }
    pendingChatMessage = freeTextMessage;
    drawChatSendTargetScreen();
    return;
  } else {
    if (freeTextMessage.length() < 64) freeTextMessage += key;
  }
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

void drawSendTargetRow(int i, bool selected) {
  int x = PANEL_X + PANEL_INSET + 8;
  int y = BODY_Y + 54 + i * 34;
  int w = PANEL_W - PANEL_INSET * 2 - 16;
  uint16_t fillCol = selected ? C_SEL_BG : C_CARD_BG;
  uint16_t borderCol = selected ? C_ACCENT : C_NAV_SLOT;
  uint16_t textCol = selected ? C_ACCENT : C_TEXT;

  tft.fillRect(PANEL_X + PANEL_INSET, y - 2, PANEL_W - PANEL_INSET * 2, 32, C_BG);
  tft.fillRoundRect(x, y, w, 28, 6, fillCol);
  tft.drawRoundRect(x, y, w, 28, 6, borderCol);
  drawIconNodes(x + 10, y + 7, selected ? C_ACCENT : C_MUTED);
  tft.setTextSize(1);
  tft.setTextColor(textCol);
  tft.setCursor(x + 34, y + 10);
  tft.print(sendTargets[i]);
}

void drawChatSendTargetScreen() {
  screenMode = SCREEN_CHAT_SEND_TO;
  sidebarTab = 2;
  tft.fillScreen(C_BG);
  drawSidebar();
  drawCardBorder(PANEL_X, PANEL_Y, PANEL_W, PANEL_H);
  drawHeader("Send To");

  String preview = pendingChatMessage;
  if (preview.length() > 28) preview = preview.substring(0, 28) + "..";
  drawChatSendBanner(preview, C_MUTED);

  for (int i = 0; i < sendTargetCount; i++) {
    drawSendTargetRow(i, i == sendTargetIndex);
  }
  drawBottomHint("UP/DN target  SEL send  LEFT edit");
}

void handleChatSendTarget(String joy) {
  if (joy == "UP") {
    int old = sendTargetIndex;
    sendTargetIndex--;
    if (sendTargetIndex < 0) sendTargetIndex = sendTargetCount - 1;
    drawSendTargetRow(old, false);
    drawSendTargetRow(sendTargetIndex, true);
  } else if (joy == "DOWN") {
    int old = sendTargetIndex;
    sendTargetIndex++;
    if (sendTargetIndex >= sendTargetCount) sendTargetIndex = 0;
    drawSendTargetRow(old, false);
    drawSendTargetRow(sendTargetIndex, true);
  } else if (joy == "SELECT") {
    String sent = "Sent to ";
    sent += sendTargets[sendTargetIndex];
    drawChatSendBanner(sent, C_ACCENT);
    drawBottomHint("LEFT back");
  } else if (joy == "LEFT") {
    drawFreeTextKeyboard();
  }
}

// ===================== SD HELPERS =====================

void prepareSdBus() {
  digitalWrite(TFT_CS, HIGH);
  digitalWrite(SD_CS, HIGH);
  delayMicroseconds(5);
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
    tft.fillRoundRect(x, y, w, 22, 3, C_SEL_BG);
    tft.drawRoundRect(x, y, w, 22, 3, C_BORDER);
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

void drawFileManager() {
  screenMode = SCREEN_FILE_MANAGER;
  sidebarTab = 2;
  if (!sdReady) sdReady = initSDCard();
  tft.fillScreen(C_BG);
  drawSidebar();
  drawCardBorder(PANEL_X, PANEL_Y, PANEL_W, PANEL_H);
  drawHeader("Files");

  if (!sdReady) {
    tft.setTextColor(C_ERROR); tft.setTextSize(1);
    tft.setCursor(PANEL_X + PANEL_INSET + 12, BODY_Y + 70); tft.print("SD not ready");
    drawBottomHint("LEFT back");
    return;
  }

  // Path bar
  tft.fillRect(PANEL_X + PANEL_INSET, BODY_Y, PANEL_W - PANEL_INSET * 2, 12, C_CARD_BG);
  tft.setTextSize(1); tft.setTextColor(C_MUTED);
  tft.setCursor(PANEL_X + PANEL_INSET, BODY_Y + 1);
  String p = currentPath; if (p.length() > 30) p = ".." + p.substring(p.length()-28);
  tft.print(p);

  if (fileCount == 0) {
    tft.setTextColor(C_MUTED); tft.setCursor(PANEL_X + PANEL_INSET + 30, BODY_Y + 86);
    tft.print("(empty)");
    drawBottomHint("LEFT back");
    return;
  }

  for (int i = 0; i < 7; i++) {
    int idx = topIndex + i;
    if (idx >= fileCount) break;
    drawFileRow(idx, idx == selectedIndex);
  }

  // Scroll indicator
  if (fileCount > 7) {
    int barH = 168 * 7 / fileCount;
    int barY = BODY_Y + 18 + 168 * topIndex / fileCount;
    tft.fillRect(PANEL_X + PANEL_W - 4, BODY_Y + 18, 2, 168, C_NAV_SLOT);
    tft.fillRect(PANEL_X + PANEL_W - 4, barY, 2, barH, C_BORDER);
  }

  drawBottomHint("UP/DN move  SEL open  LEFT back");
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
  else showTextFile(full);
}

void showTextFile(String path) {
  screenMode = SCREEN_TEXT_VIEWER;
  tft.fillScreen(C_BG);
  drawSidebar();
  drawCardBorder(PANEL_X, PANEL_Y, PANEL_W, PANEL_H);
  drawHeader("Viewer");

  if (!sdReady) sdReady = initSDCard();
  File file = openSdPath(path, FILE_READ);
  if (!file) {
    tft.setTextColor(C_ERROR); tft.setCursor(PANEL_X + PANEL_INSET + 12, BODY_Y + 60); tft.print("Cannot open file");
    drawBottomHint("LEFT back"); return;
  }
  // filename
  tft.setTextColor(C_MUTED); tft.setTextSize(1);
  tft.setCursor(PANEL_X + PANEL_INSET, BODY_Y);
  String p = path; if (p.length() > 32) p = ".." + p.substring(p.length()-30);
  tft.print(p);
  tft.drawLine(PANEL_X + PANEL_INSET, BODY_Y + 14, PANEL_X + PANEL_W - PANEL_INSET, BODY_Y + 14, C_BORDER);

  tft.setTextColor(C_TEXT);
  tft.setTextSize(displayTextSize);
  int x = PANEL_X + PANEL_INSET, y = BODY_Y + 20;
  int lineH = 8 * displayTextSize + 4;
  int maxChars = 40 / displayTextSize;
  while (file.available() && y < FOOTER_Y - 8) {
    String line = file.readStringUntil('\n');
    if (line.length() > maxChars) line = line.substring(0, maxChars);
    tft.setCursor(x, y); tft.print(line);
    y += lineH;
  }
  file.close();
  drawBottomHint("LEFT back");
}

void handleFileManager(String joy) {
  if (joy == "UP") {
    int old = selectedIndex, oldTop = topIndex;
    selectedIndex--; if (selectedIndex < 0) selectedIndex = fileCount - 1;
    if (selectedIndex < topIndex) topIndex = selectedIndex;
    if (selectedIndex >= topIndex + 7) topIndex = selectedIndex - 6;
    if (topIndex != oldTop) drawFileManager();
    else { drawFileRow(old, false); drawFileRow(selectedIndex, true); prevSelectedIndex = selectedIndex; }
  }
  else if (joy == "DOWN") {
    int old = selectedIndex, oldTop = topIndex;
    selectedIndex++; if (selectedIndex >= fileCount) selectedIndex = 0;
    if (selectedIndex < topIndex) topIndex = selectedIndex;
    if (selectedIndex >= topIndex + 7) topIndex = selectedIndex - 6;
    if (topIndex != oldTop) drawFileManager();
    else { drawFileRow(old, false); drawFileRow(selectedIndex, true); prevSelectedIndex = selectedIndex; }
  }
  else if (joy == "SELECT") openSelectedFile();
  else if (joy == "LEFT")   goBackFolder();
}

// ===================== CONFIG =====================

void drawConfigRow(int i, bool selected) {
  if (i == 0) drawMenuRow(i, configIndex, "Mode", getConfigValue(i), 0);
  else if (i == 1) drawMenuRow(i, configIndex, "Power", getConfigValue(i), 1);
}

void drawConfigMenu() {
  screenMode = SCREEN_CONFIG;
  sidebarTab = 4;
  tft.fillScreen(C_BG);
  drawSidebar();
  drawCardBorder(PANEL_X, PANEL_Y, PANEL_W, PANEL_H);
  drawHeader("Config");

  tft.setTextColor(C_MUTED); tft.setTextSize(1);
  tft.setCursor(PANEL_X + PANEL_INSET, BODY_Y); tft.print("Radio");

  for (int i = 0; i < configCount; i++) drawConfigRow(i, i == configIndex);

  drawBottomHint("UP/DN move  SEL edit  LEFT back");
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

void drawSettingsRow(int i, bool selected) {
  if (i == 0) drawMenuRow(i, settingsIndex, "Brightness", getSettingsValue(i), 2);
  else if (i == 1) drawMenuRow(i, settingsIndex, "Alert sound", getSettingsValue(i), 4);
  else if (i == 2) drawMenuRow(i, settingsIndex, "Sleep mode", getSettingsValue(i), 4);
}

void drawSettingsMenu() {
  screenMode = SCREEN_SETTINGS;
  sidebarTab = 5;
  tft.fillScreen(C_BG);
  drawSidebar();
  drawCardBorder(PANEL_X, PANEL_Y, PANEL_W, PANEL_H);
  drawHeader("Settings");

  for (int i = 0; i < settingsCount; i++) drawSettingsRow(i, i == settingsIndex);

  drawBottomHint("UP/DN move  SEL open  LEFT back");
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
    return "Config > Power";
  }
  if (optionItem == 0) return "Settings > Brightness";
  if (optionItem == 1) return "Settings > Alert";
  return "Settings > Sleep";
}

String getOptionValue() {
  if (optionGroup == 1) return getConfigValue(optionItem);
  return getSettingsValue(optionItem);
}

void drawOptionEditorValue() {
  int x = PANEL_X + PANEL_INSET;
  int w = PANEL_W - PANEL_INSET * 2;
  tft.fillRoundRect(x, BODY_Y + 58, w, 54, 5, C_SEL_BG);
  tft.drawRoundRect(x, BODY_Y + 58, w, 54, 5, C_BORDER);
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
  tft.fillScreen(C_BG);
  drawSidebar();
  drawCardBorder(PANEL_X, PANEL_Y, PANEL_W, PANEL_H);
  drawHeader(getOptionTitle());

  tft.setTextSize(1);
  tft.setTextColor(C_MUTED);
  tft.setCursor(PANEL_X + PANEL_INSET, BODY_Y + 18);
  tft.print("LEFT / RIGHT to change");

  drawOptionEditorValue();
  drawBottomHint("L/R change  SEL save  UP back");
}

void adjustOption(int dir) {
  if (optionGroup == 1) {
    if (optionItem == 0) {
      configModeIndex += dir;
      if (configModeIndex < 0) configModeIndex = 1;
      if (configModeIndex > 1) configModeIndex = 0;
    } else {
      configPowerIndex += dir;
      if (configPowerIndex < 0) configPowerIndex = 3;
      if (configPowerIndex > 3) configPowerIndex = 0;
    }
  } else {
    if (optionItem == 0) {
      brightnessPercent += dir * 5;
      if (brightnessPercent < 5) brightnessPercent = 100;
      if (brightnessPercent > 100) brightnessPercent = 5;
      applyBrightness();
    } else if (optionItem == 1) {
      alertSoundOn = !alertSoundOn;
    } else {
      sleepModeIndex += dir;
      if (sleepModeIndex < 0) sleepModeIndex = 4;
      if (sleepModeIndex > 4) sleepModeIndex = 0;
    }
  }
  drawOptionEditorValue();
}

void handleOptionEditor(String joy) {
  if (joy == "LEFT") adjustOption(-1);
  else if (joy == "RIGHT") adjustOption(1);
  else if (joy == "SELECT" || joy == "UP") {
    if (optionGroup == 1) drawConfigMenu();
    else drawSettingsMenu();
  }
}

// ===================== DISPLAY TEST =====================

void drawTftTest() {
  screenMode = SCREEN_SETTINGS; // reuse; back goes to settings
  tft.fillScreen(C_BG);
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

  drawBottomHint("LEFT back");
}

// ===================== JOYSTICK TEST =====================

void joystickTestScreen() {
  tft.fillScreen(C_BG);
  drawSidebar();
  drawCardBorder(PANEL_X, PANEL_Y, PANEL_W, PANEL_H);
  drawHeader("Joystick Debug");
  tft.setTextSize(1); tft.setTextColor(C_MUTED);
  tft.setCursor(PANEL_X + PANEL_INSET, BODY_Y + 16); tft.print("ADC Raw");
  tft.setCursor(PANEL_X + PANEL_INSET, BODY_Y + 76); tft.print("Direction");
  drawBottomHint("LEFT back");
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

void sdInfoScreen() {
  screenMode = SCREEN_SD_INFO;
  tft.fillScreen(C_BG);
  drawSidebar();
  drawCardBorder(PANEL_X, PANEL_Y, PANEL_W, PANEL_H);
  drawHeader("SD Card");

  int rx = PANEL_X + PANEL_INSET;
  if (!sdReady) {
    drawIconSd(rx, BODY_Y + 24, C_ERROR);
    tft.setTextColor(C_ERROR); tft.setTextSize(1);
    tft.setCursor(rx + 22, BODY_Y + 30); tft.print("SD not found");
    tft.setTextColor(C_MUTED);
    tft.setCursor(rx + 22, BODY_Y + 50); tft.print("CS=5 MOSI=11");
    tft.setCursor(rx + 22, BODY_Y + 65); tft.print("SCK=12 MISO=13");
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

    File f = openSdPath("/test.txt", FILE_WRITE);
    tft.setCursor(rx + 22, BODY_Y + 76);
    if (f) { f.println("test"); f.close(); tft.setTextColor(C_ACCENT); tft.print("Write: OK"); }
    else   { tft.setTextColor(C_ERROR); tft.print("Write: FAIL"); }
  }
  drawBottomHint("LEFT back");
}

// ===================== SD INIT =====================

bool initSDCard() {
  prepareSdBus();
  delay(100);
  Serial.println(SD.cardType());
  if (!SD.begin(SD_CS, SPI, SD_SPI_FREQ)) {
    Serial.println("SD Failed");
    return false;
  }
  if (SD.cardType() == CARD_NONE)    { Serial.println("No card");   return false; }
  Serial.println("SD OK"); return true;
}

// ===================== SETUP =====================

void setup() {
  Serial.begin(115200); delay(500);

  // ESP32 Arduino core 3.x backlight PWM: ledcAttach(pin, freq, resolution)
  ledcAttach(TFT_BL, BL_FREQ, BL_RES);
  applyBrightness();

  pinMode(JOY_PIN, INPUT_PULLUP);
  analogReadResolution(12);
  analogSetPinAttenuation(JOY_PIN, ADC_11db);

  pinMode(TFT_CS, OUTPUT); pinMode(SD_CS, OUTPUT);
  digitalWrite(TFT_CS, HIGH); digitalWrite(SD_CS, HIGH);
  SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TFT_CS);

  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(C_BG);

  // Boot splash
  drawSidebar();
  drawCardBorder(PANEL_X, PANEL_Y, PANEL_W, PANEL_H);
  drawHeader("/\\  Lomhor Chat");
  tft.setTextColor(C_ACCENT); tft.setTextSize(2);
  drawIconMsg(PANEL_X + PANEL_INSET + 8, BODY_Y + 42, C_ACCENT);
  tft.setCursor(PANEL_X + PANEL_INSET + 32, BODY_Y + 44);  tft.print("Lomhor");
  tft.setCursor(PANEL_X + PANEL_INSET + 32, BODY_Y + 66); tft.print("Chat");
  tft.setTextSize(1); tft.setTextColor(C_MUTED);
  tft.setCursor(PANEL_X + PANEL_INSET + 8, BODY_Y + 100); tft.print("ESP32-S3 N16R8");
  tft.setCursor(PANEL_X + PANEL_INSET + 8, BODY_Y + 118); tft.print("Booting...");

  delay(1000);
  sdReady = initSDCard();
  drawHomeScreen();
  lastActivityTime = millis();
}

// ===================== LOOP =====================

void loop() {
  String joy = getJoyPressed();

  if (joy != "") {
    if (displaySleeping) {
      wakeDisplay();
      delay(20);
      return;
    }
    lastActivityTime = millis();
  }

  unsigned long sleepTimeout = getSleepTimeoutMs();
  if (!displaySleeping && sleepTimeout > 0 && millis() - lastActivityTime >= sleepTimeout) {
    sleepDisplay();
  }

  if (screenMode == SCREEN_HOME) {
    // On home-style screens, LEFT returns to the Home tab.
    if (joy == "LEFT" && sidebarTab != 0) returnToHomeTab();
    else if (joy == "UP" || joy == "DOWN" || joy == "SELECT") handleTabNav(joy);
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
  else if (screenMode == SCREEN_FILE_MANAGER) {
    handleFileManager(joy);
  }
  else if (screenMode == SCREEN_TEXT_VIEWER) {
    if (joy == "LEFT") drawFileManager();
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
    if (joy == "LEFT") returnToHomeTab();
  }
  else if (screenMode == SCREEN_ABOUT) {
    if (joy == "LEFT") returnToHomeTab();
  }

  delay(20);
}

#line 1 "/Users/karonasrun/Documents/Arduino/TFT_SPI_JOYStrick_E22/SystemInput.ino"
// ===================== SLEEP / BACKLIGHT =====================

unsigned long getSleepTimeoutMs() {
  if (sleepModeIndex == 0) return 10000UL;
  if (sleepModeIndex == 1) return 30000UL;
  if (sleepModeIndex == 2) return 60000UL;
  if (sleepModeIndex == 3) return 300000UL;
  return 0; // Never
}

void sleepDisplay() {
  if (displaySleeping) return;
  ledcWrite(TFT_BL, 0);
  displaySleeping = true;
}

void wakeDisplay() {
  if (!displaySleeping) return;
  displaySleeping = false;
  applyBrightness();
  lastActivityTime = millis();
}

// ===================== JOYSTICK =====================

int readJoystickSmooth() {
  long total = 0;
  for (int i = 0; i < 20; i++) { total += analogRead(JOY_PIN); delayMicroseconds(300); }
  return total / 20;
}

String getDirection(int value) {
  if (value >= 2700 && value < 2900) return "LEFT";
  if (value >= 1600 && value < 1700) return "RIGHT";
  if (value >= 2300 && value < 2400) return "UP";
  if (value >= 200  && value < 400)  return "DOWN";
  if (value >= 3100 && value < 3600) return "SELECT";
  return "";
}

String getJoyPressed() {
  int value = readJoystickSmooth();
  String dir = getDirection(value);
  if (dir != "" && millis() - lastMoveTime > moveDelay) {
    lastMoveTime = millis();
    return dir;
  }
  return "";
}

