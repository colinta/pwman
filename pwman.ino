#include <math.h>
#include <Arduboy2.h>
#include <Keyboard.h>

#define READ 0
#define WRITE_NAME 1
#define WRITE_PW 2
#define FRAME_RATE 60
#define MAX_ENTRIES 8
#define MAX_LEN 255

Arduboy2 arduboy;
uint8_t selectedEntry = 0;

struct PasswordEntry {
  char *name;
  char *pw;
  uint8_t pwLen;
};

int serial = 0;
uint8_t appState = READ;
char write_bfr[MAX_LEN + 1];
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
  if (appState == READ) {
    readLoop();
  }
  else if (appState == WRITE_NAME || appState == WRITE_PW) {
    writeLoop();
  }
  arduboy.display();
}

void writeLoop() {
  if (!serial) { return; }

  if (serial >= 32 && serial < 127 && strlen(write_bfr) < MAX_LEN) {
    write_bfr[strlen(write_bfr)] = (char)serial;
    if (appState == WRITE_NAME) {
      Serial.print((char)serial);
    }
  }
  else if (serial == 13) {
    PasswordEntry *entry = &entries[selectedEntry];
    if (appState == WRITE_NAME) {
      Serial.print("\r\nCopied \"");
      Serial.print(write_bfr);
      Serial.print("\"");
      entry->name = (char*)malloc(sizeof(char) * (strlen(write_bfr) + 1));
      strcpy(entry->name, write_bfr);

      appState = WRITE_PW;
      Serial.print("\r\nEnter the password:");
      resetWriteBfr();
    }
    else {
      Serial.print("\r\nCopied \"");
      Serial.print(write_bfr);
      Serial.print("\"");
      entry->pw = (char*)malloc(sizeof(char) * (strlen(write_bfr) + 1));
      strcpy(entry->pw, write_bfr);
      entry->pwLen = strlen(write_bfr);

      appState = READ;
      Serial.print("\r\ndone.\r\n");
      pwmanSave();
    }
  }
}

void readLoop() {
  if (arduboy.justPressed(UP_BUTTON) && selectedEntry > 0) {
    selectedEntry -= 1;
  }

  if (arduboy.justPressed(DOWN_BUTTON) && selectedEntry < MAX_ENTRIES - 1) {
    selectedEntry += 1;
  }

  if (arduboy.justPressed(B_BUTTON)) {
    PasswordEntry *entry = &entries[selectedEntry];
    if (entry->pw) {
      Keyboard.begin();
      for (uint8_t entry_i = 0 ; entry_i < entry->pwLen ; ++entry_i) {
        char key = entry->pw[entry_i];
        Keyboard.press(key);
        delay(10);
        Keyboard.release(key);
      }
      Keyboard.press(10);
      delay(10);
      Keyboard.release(10);
      Keyboard.end();
    }
  }

  if (arduboy.justPressed(A_BUTTON)) {
    PasswordEntry *entry = &entries[selectedEntry];
    resetEntry(entry);
    resetWriteBfr();
    appState = WRITE_NAME;
    Serial.print("Entry name: ");
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

// this check is a way to have some confidence that the EEPROM was under this
// game's control last time it was accessed.
bool checkEEPROM() {
  if ( EEPROM.read(0) != 'P' ) return false;
  if ( EEPROM.read(1) != 'W' ) return false;
  if ( EEPROM.read(2) != 'M' ) return false;
  if ( EEPROM.read(3) != 'A' ) return false;
  if ( EEPROM.read(4) != 'N' ) return false;
  return true;
}

void resetWriteBfr() {
  for (uint16_t i = 0 ; i < MAX_LEN + 1 ; ++i) {
    write_bfr[i] = 0;
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

  if (!checkEEPROM()) { return; }

  uint16_t eeprom_ptr = 5;
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
  EEPROM.update(0, 'P');
  EEPROM.update(1, 'W');
  EEPROM.update(2, 'M');
  EEPROM.update(3, 'A');
  EEPROM.update(4, 'N');

  uint16_t eeprom_ptr = 5;
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
