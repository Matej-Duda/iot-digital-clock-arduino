/*
 * ═══════════════════════════════════════════════════════════════════
 *  Multifunkční digitální hodiny – Arduino Mega 2560
 *  Maturitní projekt · 2026
 * ═══════════════════════════════════════════════════════════════════
 *
 *  Hardware:
 *    • Arduino Mega 2560
 *    • Ethernet Shield W5100 R3
 *    • 2× LED matice MAX7219 4-in-1 (celkem 8 modulů 8x8)
 *    • Zesilovač LM386 (GS26347) + reproduktor
 *
 *  Knihovny (nainstaluj přes Library Manager):
 *    • Ethernet        (součást Arduino)
 *    • LedControl      (od Eberhard Fahle)
 *    • SPI             (součást Arduino)
 *
 *  Zapojení displejů (bez daisy-chain, každý modul na svých pinech):
 *    Modul 1 (horní):  DIN→25,  CLK→26,  CS→27,   VCC→5V, GND→GND
 *    Modul 2 (dolní):  DIN→22, CLK→23, CS→24,  VCC→5V, GND→GND
 *
 *  Rozložení (32×16 pixelů):
 *    ┌────────────────────────────────┐
 *    │  Modul 1: ČAS       (15:30)   │
 *    ├────────────────────────────────┤
 *    │  Modul 2: TEPLOTA   (22.5*C)  │
 *    └────────────────────────────────┘
 *
 *  Funkce:
 *    1. DHCP – automatické přidělení IP
 *    2. Každých 10 s stahuje data z Flask serveru (/api/arduino)
 *    3. Zobrazuje čas (nahoře) a teplotu (dole) současně
 *    4. Scrolluje příchozí zprávy (notifikace)
 *    5. Přerušované pípání budíku přes tone()
 * ═══════════════════════════════════════════════════════════════════
 */

#include <SPI.h>
#include <Ethernet.h>
#include "LedControl.h"

// ══════════════════════════════════════════════════════════════════
//  KONFIGURACE
// ══════════════════════════════════════════════════════════════════

// --- Displeje (softwarové SPI přes LedControl) ---
LedControl lc1 = LedControl(22, 23, 24, 4);   // Modul 1 (horní řada – čas)
LedControl lc2 = LedControl(25, 26, 27, 4);   // Modul 2 (dolní řada – teplota)

// --- Síť ---
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
const IPAddress SERVER_IP(152, 70, 30, 253);
const uint16_t  SERVER_PORT = 5000;

// --- Reproduktor / LM386 ---
#define BUZZER_PIN  8

// --- Časování ---
#define FETCH_INTERVAL_MS   10000UL
#define SCROLL_SPEED_MS        50
#define ALARM_BEEP_ON_MS      200
#define ALARM_BEEP_OFF_MS     300
#define ALARM_FREQ_HZ        2000
#define SLEEP_TIMEOUT_MS    600000UL   // 10 minut bez odpovědi → vypnout displej
#define BOTTOM_TOGGLE_MS     5000      // Přepínání teplota/datum každých 5 s

// ══════════════════════════════════════════════════════════════════
//  GLOBÁLNÍ PROMĚNNÉ
// ══════════════════════════════════════════════════════════════════

EthernetClient client;

char serverTime[6]  = "--:--";
char serverDate[8]  = "--.--.";
char serverTemp[8]  = "-.--";
bool alarmActive    = false;
char serverMsg[65]  = "";

unsigned long lastFetchTime       = 0;
unsigned long lastSuccessfulFetch = 0;  // Pro 10min sleep
bool newMessageReady              = false;

// Scrollování
bool messageScrolling        = false;
int  scrollX                 = 0;
unsigned long lastScrollStep = 0;

// Budík
unsigned long lastBeepToggle = 0;
bool beepOn                  = false;

// Spánek displeje (10 min bez odpovědi)
bool displaySleeping         = false;

// Přepínání teplota ↔ datum na dolním modulu
bool showingDate             = false;
unsigned long lastBottomToggle = 0;

// Framebuffery (32 sloupců na modul, každý byte = 8 vertikálních pixelů)
uint8_t fb1[32];  // Modul 1 (horní)
uint8_t fb2[32];  // Modul 2 (dolní)

// ══════════════════════════════════════════════════════════════════
//  FONT 5×7 (v PROGMEM)
// ══════════════════════════════════════════════════════════════════
// Sloupcový formát: 5 bytů na znak, každý byte = 1 sloupec
// Bit 0 = horní pixel, bit 6 = spodní pixel
// ASCII 32 (mezera) až 122 ('z')

const uint8_t PROGMEM font5x7[] = {
  // 32: (space)
  0x00,0x00,0x00,0x00,0x00,
  // 33: !
  0x00,0x00,0x5F,0x00,0x00,
  // 34: "
  0x00,0x07,0x00,0x07,0x00,
  // 35: #
  0x14,0x7F,0x14,0x7F,0x14,
  // 36: $
  0x24,0x2A,0x7F,0x2A,0x12,
  // 37: %
  0x23,0x13,0x08,0x64,0x62,
  // 38: &
  0x36,0x49,0x55,0x22,0x50,
  // 39: '
  0x00,0x05,0x03,0x00,0x00,
  // 40: (
  0x00,0x1C,0x22,0x41,0x00,
  // 41: )
  0x00,0x41,0x22,0x1C,0x00,
  // 42: *
  0x08,0x2A,0x1C,0x2A,0x08,
  // 43: +
  0x08,0x08,0x3E,0x08,0x08,
  // 44: ,
  0x00,0x50,0x30,0x00,0x00,
  // 45: -
  0x08,0x08,0x08,0x08,0x08,
  // 46: .
  0x00,0x60,0x60,0x00,0x00,
  // 47: /
  0x20,0x10,0x08,0x04,0x02,
  // 48: 0
  0x3E,0x51,0x49,0x45,0x3E,
  // 49: 1
  0x00,0x42,0x7F,0x40,0x00,
  // 50: 2
  0x42,0x61,0x51,0x49,0x46,
  // 51: 3
  0x21,0x41,0x45,0x4B,0x31,
  // 52: 4
  0x18,0x14,0x12,0x7F,0x10,
  // 53: 5
  0x27,0x45,0x45,0x45,0x39,
  // 54: 6
  0x3C,0x4A,0x49,0x49,0x30,
  // 55: 7
  0x01,0x71,0x09,0x05,0x03,
  // 56: 8
  0x36,0x49,0x49,0x49,0x36,
  // 57: 9
  0x06,0x49,0x49,0x29,0x1E,
  // 58: :
  0x00,0x36,0x36,0x00,0x00,
  // 59: ;
  0x00,0x56,0x36,0x00,0x00,
  // 60: <
  0x00,0x08,0x14,0x22,0x41,
  // 61: =
  0x14,0x14,0x14,0x14,0x14,
  // 62: >
  0x41,0x22,0x14,0x08,0x00,
  // 63: ?
  0x02,0x01,0x51,0x09,0x06,
  // 64: @
  0x32,0x49,0x79,0x41,0x3E,
  // 65: A
  0x7E,0x11,0x11,0x11,0x7E,
  // 66: B
  0x7F,0x49,0x49,0x49,0x36,
  // 67: C
  0x3E,0x41,0x41,0x41,0x22,
  // 68: D
  0x7F,0x41,0x41,0x22,0x1C,
  // 69: E
  0x7F,0x49,0x49,0x49,0x41,
  // 70: F
  0x7F,0x09,0x09,0x01,0x01,
  // 71: G
  0x3E,0x41,0x41,0x51,0x32,
  // 72: H
  0x7F,0x08,0x08,0x08,0x7F,
  // 73: I
  0x00,0x41,0x7F,0x41,0x00,
  // 74: J
  0x20,0x40,0x41,0x3F,0x01,
  // 75: K
  0x7F,0x08,0x14,0x22,0x41,
  // 76: L
  0x7F,0x40,0x40,0x40,0x40,
  // 77: M
  0x7F,0x02,0x04,0x02,0x7F,
  // 78: N
  0x7F,0x04,0x08,0x10,0x7F,
  // 79: O
  0x3E,0x41,0x41,0x41,0x3E,
  // 80: P
  0x7F,0x09,0x09,0x09,0x06,
  // 81: Q
  0x3E,0x41,0x51,0x21,0x5E,
  // 82: R
  0x7F,0x09,0x19,0x29,0x46,
  // 83: S
  0x46,0x49,0x49,0x49,0x31,
  // 84: T
  0x01,0x01,0x7F,0x01,0x01,
  // 85: U
  0x3F,0x40,0x40,0x40,0x3F,
  // 86: V
  0x1F,0x20,0x40,0x20,0x1F,
  // 87: W
  0x7F,0x20,0x18,0x20,0x7F,
  // 88: X
  0x63,0x14,0x08,0x14,0x63,
  // 89: Y
  0x03,0x04,0x78,0x04,0x03,
  // 90: Z
  0x61,0x51,0x49,0x45,0x43,
  // 91: [
  0x00,0x00,0x7F,0x41,0x41,
  // 92: backslash
  0x02,0x04,0x08,0x10,0x20,
  // 93: ]
  0x41,0x41,0x7F,0x00,0x00,
  // 94: ^
  0x04,0x02,0x01,0x02,0x04,
  // 95: _
  0x40,0x40,0x40,0x40,0x40,
  // 96: `
  0x00,0x01,0x02,0x04,0x00,
  // 97: a
  0x20,0x54,0x54,0x54,0x78,
  // 98: b
  0x7F,0x48,0x44,0x44,0x38,
  // 99: c
  0x38,0x44,0x44,0x44,0x20,
  // 100: d
  0x38,0x44,0x44,0x48,0x7F,
  // 101: e
  0x38,0x54,0x54,0x54,0x18,
  // 102: f
  0x08,0x7E,0x09,0x01,0x02,
  // 103: g
  0x08,0x14,0x54,0x54,0x3C,
  // 104: h
  0x7F,0x08,0x04,0x04,0x78,
  // 105: i
  0x00,0x44,0x7D,0x40,0x00,
  // 106: j
  0x20,0x40,0x44,0x3D,0x00,
  // 107: k
  0x00,0x7F,0x10,0x28,0x44,
  // 108: l
  0x00,0x41,0x7F,0x40,0x00,
  // 109: m
  0x7C,0x04,0x18,0x04,0x78,
  // 110: n
  0x7C,0x08,0x04,0x04,0x78,
  // 111: o
  0x38,0x44,0x44,0x44,0x38,
  // 112: p
  0x7C,0x14,0x14,0x14,0x08,
  // 113: q
  0x08,0x14,0x14,0x18,0x7C,
  // 114: r
  0x7C,0x08,0x04,0x04,0x08,
  // 115: s
  0x48,0x54,0x54,0x54,0x20,
  // 116: t
  0x04,0x3F,0x44,0x40,0x20,
  // 117: u
  0x3C,0x40,0x40,0x20,0x7C,
  // 118: v
  0x1C,0x20,0x40,0x20,0x1C,
  // 119: w
  0x3C,0x40,0x30,0x40,0x3C,
  // 120: x
  0x44,0x28,0x10,0x28,0x44,
  // 121: y
  0x0C,0x50,0x50,0x50,0x3C,
  // 122: z
  0x44,0x64,0x54,0x4C,0x44
};

// ══════════════════════════════════════════════════════════════════
//  FRAMEBUFFER & VYKRESLOVÁNÍ
// ══════════════════════════════════════════════════════════════════

void clearFB(uint8_t* fb) {
  memset(fb, 0, 32);
}

// Vykreslí jeden znak na pozici x do framebufferu
void drawChar(uint8_t* fb, int x, char c) {
  if (c < 32 || c > 122) c = ' ';
  int idx = (c - 32) * 5;
  for (int col = 0; col < 5; col++) {
    int px = x + col;
    if (px >= 0 && px < 32) {
      fb[px] = pgm_read_byte(&font5x7[idx + col]);
    }
  }
}

// Vykreslí řetězec od pozice x
void drawString(uint8_t* fb, int x, const char* str) {
  while (*str) {
    drawChar(fb, x, *str);
    x += 6;  // 5px znak + 1px mezera
    str++;
  }
}

// Šířka řetězce v pixelech
int stringWidth(const char* str) {
  int len = strlen(str);
  return len > 0 ? len * 6 - 1 : 0;
}

// Odešle framebuffer do LedControl modulu
// Transponuje sloupcová data na řádky (tyto moduly mají řady/sloupce prohozené)
void sendFB(LedControl& lc, uint8_t* fb) {
  for (int dev = 0; dev < 4; dev++) {
    for (int row = 0; row < 8; row++) {
      uint8_t rowData = 0;
      for (int col = 0; col < 8; col++) {
        if (fb[dev * 8 + col] & (1 << (7 - row))) {
          rowData |= (1 << col);
        }
      }
      lc.setRow(dev, row, rowData);
    }
  }
}

// ══════════════════════════════════════════════════════════════════
//  DISPLEJ – ZOBRAZENÍ
// ══════════════════════════════════════════════════════════════════

// Vypne oba displeje
void clearDisplays() {
  clearFB(fb1);
  clearFB(fb2);
  sendFB(lc1, fb1);
  sendFB(lc2, fb2);
}

// Zobrazí čas (nahoře) a teplotu NEBO datum (dole)
void updateStaticDisplay() {
  // Horní modul: ČAS (centrovaný)
  clearFB(fb1);
  int tw = stringWidth(serverTime);
  drawString(fb1, (32 - tw) / 2, serverTime);
  sendFB(lc1, fb1);

  // Dolní modul: TEPLOTA nebo DATUM (střídá se)
  clearFB(fb2);
  if (showingDate) {
    int dw = stringWidth(serverDate);
    drawString(fb2, (32 - dw) / 2, serverDate);
  } else {
    char buf[12];
    snprintf(buf, sizeof(buf), "%s*C", serverTemp);
    int tempW = stringWidth(buf);
    drawString(fb2, (32 - tempW) / 2, buf);
  }
  sendFB(lc2, fb2);
}

// Přepíná teplota/datum na dolním displeji
void updateBottomToggle() {
  unsigned long now = millis();
  if (now - lastBottomToggle >= BOTTOM_TOGGLE_MS) {
    lastBottomToggle = now;
    showingDate = !showingDate;
    if (!messageScrolling) {
      updateStaticDisplay();
    }
  }
}

// Zahájí scrollování zprávy (na horním modulu)
void startMessageScroll() {
  messageScrolling = true;
  scrollX = 32;
  lastScrollStep = millis();
}

// Aktualizuje scrollování (non-blocking)
void updateScrollDisplay() {
  unsigned long now = millis();
  if (now - lastScrollStep < SCROLL_SPEED_MS) return;
  lastScrollStep = now;

  clearFB(fb1);
  drawString(fb1, scrollX, serverMsg);
  sendFB(lc1, fb1);

  scrollX--;
  int msgW = stringWidth(serverMsg);
  if (scrollX < -msgW) {
    messageScrolling = false;
  }
}

// ══════════════════════════════════════════════════════════════════
//  PARSOVÁNÍ ODPOVĚDI SERVERU
// ══════════════════════════════════════════════════════════════════

bool extractValue(const char* response, const char* key, char* dest, size_t destSize) {
  const char* start = strstr(response, key);
  if (start == NULL) return false;
  start += strlen(key);
  const char* end = start;
  while (*end != '\0' && *end != '|' && *end != '>') end++;
  size_t len = (size_t)(end - start);
  if (len >= destSize) len = destSize - 1;
  memcpy(dest, start, len);
  dest[len] = '\0';
  return true;
}

void parseServerResponse(const char* response) {
  char tempBuf[16];

  if (extractValue(response, "TIME:", serverTime, sizeof(serverTime))) {
    Serial.print(F("[PARSE] Čas: "));
    Serial.println(serverTime);
  }
  if (extractValue(response, "DATE:", serverDate, sizeof(serverDate))) {
    Serial.print(F("[PARSE] Datum: "));
    Serial.println(serverDate);
  }
  if (extractValue(response, "TEMP:", serverTemp, sizeof(serverTemp))) {
    Serial.print(F("[PARSE] Teplota: "));
    Serial.println(serverTemp);
  }
  if (extractValue(response, "ALARM:", tempBuf, sizeof(tempBuf))) {
    alarmActive = (tempBuf[0] == '1');
    Serial.print(F("[PARSE] Budík: "));
    Serial.println(alarmActive ? "AKTIVNÍ" : "neaktivní");
  }
  char newMsg[65];
  if (extractValue(response, "MSG:", newMsg, sizeof(newMsg))) {
    if (strcmp(newMsg, "none") != 0 && strcmp(newMsg, serverMsg) != 0) {
      strncpy(serverMsg, newMsg, sizeof(serverMsg) - 1);
      serverMsg[sizeof(serverMsg) - 1] = '\0';
      newMessageReady = true;
      Serial.print(F("[PARSE] Nová zpráva: "));
      Serial.println(serverMsg);
    } else if (strcmp(newMsg, "none") == 0) {
      serverMsg[0] = '\0';
    }
  }
}

// ══════════════════════════════════════════════════════════════════
//  HTTP POŽADAVEK
// ══════════════════════════════════════════════════════════════════

void fetchDataFromServer() {
  Serial.println(F("\n[HTTP] Připojuji se k serveru..."));
  client.stop();

  if (client.connect(SERVER_IP, SERVER_PORT)) {
    Serial.println(F("[HTTP] Odesílám GET..."));
    client.println(F("GET /api/arduino HTTP/1.1"));
    client.print(F("Host: "));
    client.println(SERVER_IP);
    client.println(F("Connection: close"));
    client.println();

    char responseBuf[256];
    size_t idx = 0;
    bool bodyStarted = false;
    bool messageComplete = false;
    unsigned long timeout = millis();

    // Čteme, dokud je spojení aktivní a nemáme celou zprávu
    while (client.connected() && !messageComplete) {
      if (client.available()) {
        char c = client.read();
        
        if (!bodyStarted) {
          // Čekáme na začátek zprávy '<'
          if (c == '<') {
            bodyStarted = true;
            responseBuf[idx++] = c;
          }
        } else {
          // Ukládáme tělo zprávy
          responseBuf[idx++] = c;
          // Konec zprávy '>'
          if (c == '>') {
            messageComplete = true;
          }
        }
        
        // Ochrana proti přetečení bufferu
        if (idx >= sizeof(responseBuf) - 1) break;
        
        // Reset timeoutu po úspěšném přečtení znaku
        timeout = millis();
      } else {
        // Pokud zrovna nejsou data, čekáme. Pokud nic nepřijde do 3 sekund, ukončíme.
        if (millis() - timeout > 3000) {
          Serial.println(F("[HTTP] Timeout pri cteni!"));
          break;
        }
      }
    }
    
    responseBuf[idx] = '\0';
    client.stop();

    Serial.print(F("[HTTP] Odpověď: "));
    Serial.println(responseBuf);

    if (idx > 0 && responseBuf[0] == '<') {
      parseServerResponse(responseBuf);
      lastSuccessfulFetch = millis();
      // Probudit displej, pokud spal
      if (displaySleeping) {
        displaySleeping = false;
        Serial.println(F("[WAKE] Odpověď přijata – displej ON"));
        updateStaticDisplay();
      }
    } else {
      Serial.println(F("[HTTP] Neplatná odpověď!"));
    }
  } else {
    Serial.println(F("[HTTP] Nelze se připojit!"));
  }
}

// ══════════════════════════════════════════════════════════════════
//  BUDÍK – PÍPÁNÍ
// ══════════════════════════════════════════════════════════════════

void handleAlarm() {
  if (!alarmActive) {
    noTone(BUZZER_PIN);
    beepOn = false;
    return;
  }

  unsigned long now = millis();
  if (beepOn) {
    if (now - lastBeepToggle >= ALARM_BEEP_ON_MS) {
      noTone(BUZZER_PIN);
      beepOn = false;
      lastBeepToggle = now;
    }
  } else {
    if (now - lastBeepToggle >= ALARM_BEEP_OFF_MS) {
      tone(BUZZER_PIN, ALARM_FREQ_HZ);
      beepOn = true;
      lastBeepToggle = now;
    }
  }
}

// ══════════════════════════════════════════════════════════════════
//  INICIALIZACE DISPLEJŮ
// ══════════════════════════════════════════════════════════════════

void initModule(LedControl& lc) {
  for (int i = 0; i < 4; i++) {
    lc.shutdown(i, false);
    lc.setIntensity(i, 0);   // Jas 0 = minimum
    lc.clearDisplay(i);
  }
}

void displayBootText(const char* text) {
  clearFB(fb1);
  int w = stringWidth(text);
  drawString(fb1, (32 - w) / 2, text);
  sendFB(lc1, fb1);
}

// ══════════════════════════════════════════════════════════════════
//  SETUP
// ══════════════════════════════════════════════════════════════════

void setup() {
  Serial.begin(9600);
  while (!Serial) { ; }
  Serial.println(F("\n══════════════════════════════════════"));
  Serial.println(F("  Multifunkční digitální hodiny"));
  Serial.println(F("  Arduino Mega 2560    v3.0"));
  Serial.println(F("══════════════════════════════════════\n"));

  // Reproduktor
  pinMode(BUZZER_PIN, OUTPUT);
  noTone(BUZZER_PIN);

  // Deaktivace Ethernet CS a SD CS
  pinMode(10, OUTPUT);
  digitalWrite(10, HIGH);
  pinMode(53, OUTPUT);

  // Displeje
  initModule(lc1);
  initModule(lc2);

  // Boot obrazovka
  displayBootText("BOOT");
  delay(1000);

  // Ethernet
  Serial.println(F("[NET] Spouštím DHCP..."));
  displayBootText("NET..");

  if (Ethernet.begin(mac) == 0) {
    Serial.println(F("[NET] DHCP selhalo!"));
    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
      Serial.println(F("[NET] Ethernet shield nenalezen!"));
    }
    if (Ethernet.linkStatus() == LinkOFF) {
      Serial.println(F("[NET] Kabel není zapojen!"));
    }
    displayBootText("ERR");
    while (true) { delay(1000); }
  }

  Serial.print(F("[NET] IP: "));
  Serial.println(Ethernet.localIP());

  displayBootText("OK");
  delay(1000);

  // Stáhni data ihned
  fetchDataFromServer();
  lastFetchTime = millis();
  lastSuccessfulFetch = millis();
  lastBottomToggle = millis();

  // Zobraz čas + teplotu
  updateStaticDisplay();

  Serial.println(F("\n[READY] Systém připraven.\n"));
}

// ══════════════════════════════════════════════════════════════════
//  LOOP
// ══════════════════════════════════════════════════════════════════

void loop() {
  unsigned long now = millis();

  // 1. Periodické stahování dat
  if (now - lastFetchTime >= FETCH_INTERVAL_MS) {
    fetchDataFromServer();
    lastFetchTime = now;
    Ethernet.maintain();

    if (!messageScrolling && !displaySleeping) {
      updateStaticDisplay();
    }
  }

  // 2. Sleep po 10 minutách bez úspěšné odpovědi
  //    Používáme millis() místo 'now' – 'now' je zastaralé po HTTP fetchi!
  if (!displaySleeping && (millis() - lastSuccessfulFetch > SLEEP_TIMEOUT_MS)) {
    Serial.println(F("[SLEEP] 10 min bez odpovědi – displej OFF"));
    displaySleeping = true;
    clearDisplays();
    noTone(BUZZER_PIN);
  }

  // Pokud spíme, nescrollujeme ani neblikáme
  if (displaySleeping) return;

  // 3. Přepínání teplota/datum na dolním displeji
  updateBottomToggle();

  // 4. Správa displeje – scrollování zpráv
  if (messageScrolling) {
    updateScrollDisplay();
    if (!messageScrolling) {
      updateStaticDisplay();
    }
  } else if (newMessageReady) {
    newMessageReady = false;
    startMessageScroll();
  }

  // 5. Budík
  handleAlarm();
}
