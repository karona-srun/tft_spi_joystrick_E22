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
  if (value >= 2700 && value < 2900) return "UP";
  if (value >= 1600 && value < 1700) return "DOWN";
  if (value >= 2300 && value < 2400) return "RIGHT";
  if (value >= 200  && value < 400)  return "LEFT";
  if (value >= 3100 && value < 3600) return "SELECT";
  return "";
}

String getJoyPressed() {
  if (digitalRead(HOME_BTN_PIN) == LOW && millis() - lastMoveTime > moveDelay) {
    lastMoveTime = millis();
    return "HOME";
  }
  if (digitalRead(BACK_BTN_PIN) == LOW && millis() - lastMoveTime > moveDelay) {
    lastMoveTime = millis();
    return "LEFT";
  }

  int value = readJoystickSmooth();
  String dir = getDirection(value);
  if (dir != "" && millis() - lastMoveTime > moveDelay) {
    lastMoveTime = millis();
    return dir;
  }
  return "";
}
