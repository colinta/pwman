#define PWMAN_VERSION 2

#include <math.h>
#include <Arduboy2.h>
#include <Keyboard.h>
#include "cape.h"

#define KC_ENTER 10
#define FRAME_RATE 60

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
#define MAX_PASSWORD_LEN 100
#define MAX_UNLOCK_LEN 10
cape_t cape;
uint8_t cape_salt = 0;
uint8_t selectedEntry = 0;
char verifyPassword[] = {0,0,0,0,0,0,0,0,0,0,0};
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

void switchToVerify() {}
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
    for (uint8_t i = 0; i < MAX_UNLOCK_LEN; ++i) {
      verifyPassword[i] = 0;
    }
  }

  if (arduboy.justPressed(B_BUTTON)) {
    cape_init(&cape, verifyPassword, strlen(verifyPassword), cape_salt);
    nextAppState = STATE_MAIN;
  }

  char pressed = 0;
  if (arduboy.justPressed(UP_BUTTON)) {
    pressed = 'a';
  }
  else if (arduboy.justPressed(DOWN_BUTTON)) {
    pressed = 'b';
  }
  else if (arduboy.justPressed(LEFT_BUTTON)) {
    pressed = 'c';
  }
  else if (arduboy.justPressed(RIGHT_BUTTON)) {
    pressed = 'd';
  }

  if (pressed && strlen(verifyPassword) < MAX_UNLOCK_LEN) {
    verifyPassword[strlen(verifyPassword)] = pressed;
  }

  arduboy.setCursor(0, 8*selectedEntry);
  if (strlen(verifyPassword)) {
    for (uint8_t i = 0; i < strlen(verifyPassword); ++i) {
      arduboy.print("*");
    }
  }
  else {
    arduboy.print("_");
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

  if (serial >= 32 && serial < 127 && (nextAppState == STATE_WRITE_NAME ? strlen(buffer) < MAX_NAME_LEN : strlen(buffer) < MAX_PASSWORD_LEN)) {
    buffer[strlen(buffer)] = (char)serial;
    if (nextAppState == STATE_WRITE_NAME) {
      Serial.print((char)serial);
    }
  }
  else if (serial == 13) {
    PasswordEntry *entry = &entries[selectedEntry];
    if (nextAppState == STATE_WRITE_NAME) {
      entry->name = (char*)malloc(sizeof(char) * (strlen(buffer) + 1));
      strcpy(entry->name, buffer);

      nextAppState = STATE_WRITE_PW;
      Serial.print("\r\nEnter the password:");
      resetBuffer();
    }
    else {
      entry->pwLen = strlen(buffer);
      entry->pw = (char*)malloc(sizeof(char) * (strlen(buffer) + 1));
      cape_crypt(&cape, buffer, entry->pw, entry->pwLen);

      nextAppState = STATE_MAIN;
      Serial.println("\r\n");
      pwmanSave();
    }
  }
}

void outputLoop() {
  PasswordEntry *entry = &entries[selectedEntry];
  if (entry->pw) {
    cape_crypt(&cape, entry->pw, buffer, entry->pwLen);

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
  for (uint16_t i = 0 ; i < MAX_PASSWORD_LEN + 1 ; ++i) {
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
      str = (char*)malloc(sizeof(char) * (name_len + 1));
      for (uint8_t entry_i = 0 ; entry_i < name_len ; ++entry_i) {
        char c = EEPROM.read(eeprom_ptr++);
        str[entry_i] = c;
      }
      entry->pw = str;
      entry->pwLen = name_len;
    }
  }
}

void pwmanSave() {
  uint16_t eeprom_ptr = EEPROM_STORAGE_SPACE_START;
  EEPROM.update(eeprom_ptr++, 'P');
  EEPROM.update(eeprom_ptr++, 'W');
  EEPROM.update(eeprom_ptr++, 'm');
  EEPROM.update(eeprom_ptr++, 'a');
  EEPROM.update(eeprom_ptr++, 'n');
  EEPROM.update(eeprom_ptr++, PWMAN_VERSION);

  for (uint8_t i = 0 ; i < MAX_ENTRIES ; ++i ) {
    PasswordEntry *entry = &entries[i];
    if (!entry->name) {
      EEPROM.update(eeprom_ptr++, 0);
      EEPROM.update(eeprom_ptr++, 0);
      continue;
    }

    EEPROM.update(eeprom_ptr++, strlen(entry->name));
    for ( uint8_t i = 0 ; i < strlen(entry->name) ; ++i) {
      EEPROM.update(eeprom_ptr++, entry->name[i]);
    }

    EEPROM.update(eeprom_ptr++, entry->pwLen);
    for ( uint8_t i = 0 ; i < entry->pwLen ; ++i) {
      EEPROM.update(eeprom_ptr++, entry->pw[i]);
    }
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
