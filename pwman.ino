#define PWMAN_VERSION 4

#include <math.h>
#include <Arduboy2.h>
#include <Keyboard.h>
#include "cape.h"

// copy/paste from Arduboy (why aren't these exposed?)
#define CHAR_WIDTH 6
#define SMALL_SPACE 3
#define CHAR_HEIGHT 8

#define KC_ENTER 10
#define FRAME_RATE 60

#define UNLOCK_U 1
#define UNLOCK_D 2
#define UNLOCK_L 3
#define UNLOCK_R 4

Arduboy2 arduboy;

struct PasswordEntry {
  char *name;
  char *pw;
  uint8_t pwLen;
};

int serial = 0;

#define STATE_NONE 0
#define STATE_VERIFY 1
#define STATE_MAIN 2
#define STATE_WRITE_NAME 3
#define STATE_WRITE_PW 4
#define STATE_OUTPUT 5
uint8_t appState = STATE_NONE;
uint8_t nextAppState = STATE_VERIFY;

#define MAX_ENTRIES 8
#define MAX_NAME_LEN 20
#define MAX_GRID_LEN 14
#define MAX_PASSWORD_LEN 100
#define MAX_UNLOCK_LEN 30
#define PW_EXTRA 1

cape_t cape;
uint8_t cape_salt = '[';
uint8_t selectedEntry = 0;

char password[MAX_UNLOCK_LEN + 1];
char password_length = 0;

char grid_entries[MAX_GRID_LEN];
char grid_index = 0;

char buffer[MAX_PASSWORD_LEN + 1];
PasswordEntry entries[] = {
  { .name = NULL, .pw = NULL, .pwLen = 0 },
  { .name = NULL, .pw = NULL, .pwLen = 0 },
  { .name = NULL, .pw = NULL, .pwLen = 0 },
  { .name = NULL, .pw = NULL, .pwLen = 0 },
  { .name = NULL, .pw = NULL, .pwLen = 0 },
  { .name = NULL, .pw = NULL, .pwLen = 0 },
  { .name = NULL, .pw = NULL, .pwLen = 0 },
  { .name = NULL, .pw = NULL, .pwLen = 0 }
};

void setup() {
  Serial.begin(9600);

  arduboy.boot();
  arduboy.blank();
  arduboy.setFrameRate(FRAME_RATE);

  // UP button -> light up screen
  arduboy.flashlight();

  // system control:
  // B_BUTTON -> blue LED
  // B_BUTTON + UP -> green LED, turn sound ON
  // B_BUTTON + DOWN -> red LED, turn sound OFF
  arduboy.systemButtons();
  arduboy.initRandomSeed();

  pwmanInit();
}

void loop() {
  // <arduboy loop throttle>
  if (!arduboy.nextFrame()) {
    return;
  }
  arduboy.pollButtons();
  // </arduboy>

  if (Serial.available() > 0) {
    serial = Serial.read();
  }
  else {
    serial = 0;
  }

  arduboy.clear();
  if (nextAppState != appState) {
    switch (nextAppState) {
      case STATE_VERIFY:
        switchToVerify();
        break;
      case STATE_MAIN:
        switchToMain();
        break;
      case STATE_WRITE_NAME:
        switchToWrite();
        break;
      case STATE_WRITE_PW:
        // no-op
        break;
      case STATE_OUTPUT:
        switchToOutput();
        break;
    }

    appState = nextAppState;
  }

  switch (appState) {
    case STATE_VERIFY:
      verifyLoop();
      break;
    case STATE_MAIN:
      mainLoop();
      break;
    case STATE_WRITE_NAME:
    case STATE_WRITE_PW:
      writeLoop();
      break;
    case STATE_OUTPUT:
      outputLoop();
      break;
  }

  arduboy.display();
}

void switchToVerify() {
  resetGridEntries();
  resetPassword();
}
void switchToMain() {}
void switchToWrite() {
  PasswordEntry *entry = &entries[selectedEntry];
  resetEntry(entry);
  resetBuffer();
  Serial.print("Entry name: ");
}
void switchToOutput() {}

void verifyLoop() {
  if (arduboy.justPressed(A_BUTTON)) {
    cape_init(&cape, password, password_length, cape_salt);
    nextAppState = STATE_MAIN;
    return;
  }

  char pressed = 0;
  if (arduboy.justPressed(UP_BUTTON)) {
    pressed = UNLOCK_U;
  }
  else if (arduboy.justPressed(DOWN_BUTTON)) {
    pressed = UNLOCK_D;
  }
  else if (arduboy.justPressed(LEFT_BUTTON)) {
    pressed = UNLOCK_L;
  }
  else if (arduboy.justPressed(RIGHT_BUTTON)) {
    pressed = UNLOCK_R;
  }

  if (pressed && grid_index < MAX_GRID_LEN) {
    grid_entries[grid_index++] = pressed;
  }

  byte x = WIDTH / 2, y = HEIGHT / 2;
  byte dx = WIDTH / 4, dy = HEIGHT / 4;
  for (uint8_t i = 0; i < grid_index; ++i) {
    char p = grid_entries[i];
    switch (p) {
      case UNLOCK_U:
        if (y == 1 && dy == 0) { y = 0; }
        else { y = y - dy; }
        dy /= 2;
        break;
      case UNLOCK_D:
        y = y + dy;
        dy /= 2;
        break;
      case UNLOCK_L:
        if (x == 1 && dx == 0) { x = 0; }
        else { x = x - dx; }
        dx /= 2;
        break;
      case UNLOCK_R:
        x = x + dx;
        dx /= 2;
        break;
    }
  }

  if (arduboy.justPressed(B_BUTTON)) {
    password[password_length++] = x;
    password[password_length++] = y;
    resetGridEntries();
  }

  arduboy.drawFastVLine(x, 0, HEIGHT);
  arduboy.drawFastHLine(0, y, WIDTH);

  uint8_t cx = (x > WIDTH / 4 ? 0 : WIDTH / 2);
  uint8_t cy = (y > HEIGHT / 4 ? 0 : HEIGHT / 2);
  arduboy.setCursor(cx, cy);
  arduboy.print(x);
  arduboy.print(",");
  arduboy.print(y);

  arduboy.setCursor(0, HEIGHT - CHAR_HEIGHT);
  for (uint8_t i = 0; i < password_length; ++i) {
    arduboy.print((uint8_t)password[i]);
    arduboy.setCursor(arduboy.getCursorX() + SMALL_SPACE, arduboy.getCursorY());
  }
}

void mainLoop() {
  if (arduboy.justPressed(UP_BUTTON) && selectedEntry > 0) {
    selectedEntry -= 1;
  }

  if (arduboy.justPressed(DOWN_BUTTON) && selectedEntry < MAX_ENTRIES - 1) {
    selectedEntry += 1;
  }

  if (arduboy.justPressed(B_BUTTON)) {
    nextAppState = STATE_OUTPUT;
  }

  if (arduboy.justPressed(A_BUTTON)) {
    nextAppState = STATE_WRITE_NAME;
  }

  arduboy.setTextSize(1);
  arduboy.setCursor(0, 8*selectedEntry);
  arduboy.print(">");
  for (uint8_t i = 0; i < MAX_ENTRIES ; ++i ) {
    PasswordEntry *entry = &entries[i];
    arduboy.setCursor(5*2, 8*i);
    if (entry->name) {
      arduboy.print(entry->name);
    }
  }
}

void writeLoop() {
  if (!serial) { return; }

  if (serial >= 32 && serial < 127 && (appState == STATE_WRITE_NAME ? strlen(buffer) < MAX_NAME_LEN : strlen(buffer) < MAX_PASSWORD_LEN)) {
    buffer[strlen(buffer)] = (char)serial;
    if (appState == STATE_WRITE_NAME) {
      Serial.print((char)serial);
    }
  }
  else if (serial == 127 && strlen(buffer) > 0) {
    buffer[strlen(buffer) - 1] = 0;
    if (appState == STATE_WRITE_NAME) {
      Serial.print("\r\nEntry name: ");
      Serial.print(buffer);
    }
  }
  else if (serial == 13) {
    PasswordEntry *entry = &entries[selectedEntry];
    if (appState == STATE_WRITE_NAME) {
      entry->name = (char*)malloc(sizeof(char) * (strlen(buffer) + 1));
      strcpy(entry->name, buffer);

      nextAppState = STATE_WRITE_PW;
      Serial.print("\r\nEnter the password:");
      resetBuffer();
    }
    else {
      entry->pwLen = strlen(buffer);
      entry->pw = (char*)malloc(sizeof(char) * (entry->pwLen + PW_EXTRA));
      uint8_t salt = random(0, 255);
      Serial.println("");
      Serial.print("Salt: ");
      Serial.print(salt);
      cape_encrypt(&cape, buffer, entry->pw, entry->pwLen, salt);

      nextAppState = STATE_MAIN;
      Serial.println("");
      pwmanSave();
    }
  }
}

void outputLoop() {
  PasswordEntry *entry = &entries[selectedEntry];
  if (entry->pw) {
    cape_decrypt(&cape, entry->pw, buffer, entry->pwLen);

    Keyboard.begin();
    for (uint8_t entry_i = 0 ; entry_i < entry->pwLen ; ++entry_i) {
      char key = buffer[entry_i];

      if (key >= ' ' && key <= '~') {
        Keyboard.press(key);
        delay(10);
        Keyboard.release(key);
      }
    }
    Keyboard.press(KC_ENTER);
    delay(10);
    Keyboard.release(KC_ENTER);
    Keyboard.end();
    resetBuffer();
  }
  nextAppState = STATE_MAIN;
}

void resetBuffer() {
  for (uint16_t i = 0 ; i <= MAX_PASSWORD_LEN + 1 ; ++i) {
    buffer[i] = 0;
  }
}

void resetEntry(PasswordEntry *entry) {
  if (entry->name) {
    free(entry->name);
  }

  if (entry->pw) {
    free(entry->pw);
  }

  entry->name = NULL;
  entry->pw = NULL;
  entry->pwLen = 0;
}

void pwmanInit() {
  for (uint8_t i = 0; i < MAX_ENTRIES ; ++i ) {
    PasswordEntry *entry = &entries[i];
    entry->name = NULL;
    entry->pw = NULL;
    entry->pwLen = 0;
  }

  uint16_t eeprom_ptr = 0;
  if (!checkEEPROM(&eeprom_ptr)) { return; }

  uint8_t name_len = 0;
  char *str;

  for (uint8_t i = 0 ; i < MAX_ENTRIES ; ++i ) {
    PasswordEntry *entry = &entries[i];

    name_len = EEPROM.read(eeprom_ptr++);
    if (name_len) {
      str = (char*)malloc(sizeof(char) * (name_len + 1));
      for (uint8_t entry_i = 0 ; entry_i < name_len ; ++entry_i) {
        char c = EEPROM.read(eeprom_ptr++);
        str[entry_i] = c;
      }
      str[name_len] = 0;
      entry->name = str;
    }

    name_len = EEPROM.read(eeprom_ptr++);
    if (name_len) {
      str = (char*)malloc(sizeof(char) * (name_len + PW_EXTRA));
      for (uint8_t entry_i = 0 ; entry_i < name_len + PW_EXTRA ; ++entry_i) {
        char c = EEPROM.read(eeprom_ptr++);
        str[entry_i] = c;
      }
      entry->pw = str;
      entry->pwLen = name_len;
    }
  }
}

void eepUpdate(uint16_t ptr, char c) {
  Serial.print((uint8_t)c);
  Serial.print(" ");
  EEPROM.update(ptr, c);
}

void pwmanSave() {
  Serial.println("Header");
  uint16_t eeprom_ptr = EEPROM_STORAGE_SPACE_START;
  eepUpdate(eeprom_ptr++, 'P');
  eepUpdate(eeprom_ptr++, 'W');
  eepUpdate(eeprom_ptr++, 'm');
  eepUpdate(eeprom_ptr++, 'a');
  eepUpdate(eeprom_ptr++, 'n');
  eepUpdate(eeprom_ptr++, PWMAN_VERSION);

  Serial.println("");
  Serial.println("Entries");
  for (uint8_t i = 0 ; i < MAX_ENTRIES ; ++i ) {
    PasswordEntry *entry = &entries[i];
    if (!entry->name) {
      Serial.print("Empty: ");
      eepUpdate(eeprom_ptr++, 0);
      eepUpdate(eeprom_ptr++, 0);
      Serial.println("");
      continue;
    }

    Serial.print(entry->name);
    Serial.print(": ");
    eepUpdate(eeprom_ptr++, strlen(entry->name));
    for ( uint8_t i = 0 ; i < strlen(entry->name) ; ++i) {
      eepUpdate(eeprom_ptr++, entry->name[i]);
    }

    Serial.print(";");
    eepUpdate(eeprom_ptr++, entry->pwLen);
    for ( uint8_t i = 0 ; i < entry->pwLen + PW_EXTRA ; ++i) {
      eepUpdate(eeprom_ptr++, entry->pw[i]);
    }
    Serial.println("");
  }
}

// this check is a way to have some confidence that the EEPROM was under this
// game's control last time it was accessed.
bool checkEEPROM(uint16_t *eeprom_ptr) {
  uint8_t ptr = EEPROM_STORAGE_SPACE_START;
  if ( EEPROM.read(ptr++) != 'P' ) return false;
  if ( EEPROM.read(ptr++) != 'W' ) return false;
  if ( EEPROM.read(ptr++) != 'm' ) return false;
  if ( EEPROM.read(ptr++) != 'a' ) return false;
  if ( EEPROM.read(ptr++) != 'n' ) return false;
  if ( EEPROM.read(ptr++) != PWMAN_VERSION ) return false;
  if (eeprom_ptr) {
    *eeprom_ptr = ptr;
  }
  return true;
}

void resetGridEntries() {
  grid_index = 0;
}

void resetPassword() {
  password_length = 0;
  for (uint8_t i = 0; i <= MAX_UNLOCK_LEN; ++i) {
    password[i] = 0;
  }
}
