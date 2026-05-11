/*
 * ═══════════════════════════════════════════════════════════════════
 *  RAW MAX7219 TEST – bez MD_Parola, bez jakékoliv knihovny
 *  Testuje přímou komunikaci s MAX7219 přes bit-bang.
 *  
 *  Otevři Serial Monitor (9600 baud) a sleduj displej + výpisy.
 *  
 *  Zapojení:
 *    DIN  → pin 7
 *    CLK  → pin 5
 *    CS   → pin 6
 *    VCC  → 5V
 *    GND  → GND
 * ═══════════════════════════════════════════════════════════════════
 */

#define DIN_PIN   7
#define CLK_PIN   5
#define CS_PIN    6
#define NUM_DEVICES 8

// MAX7219 registry
#define REG_NOOP        0x00
#define REG_DIGIT0      0x01
#define REG_DIGIT1      0x02
#define REG_DIGIT2      0x03
#define REG_DIGIT3      0x04
#define REG_DIGIT4      0x05
#define REG_DIGIT5      0x06
#define REG_DIGIT6      0x07
#define REG_DIGIT7      0x08
#define REG_DECODE      0x09
#define REG_INTENSITY   0x0A
#define REG_SCANLIMIT   0x0B
#define REG_SHUTDOWN    0x0C
#define REG_DISPLAYTEST 0x0F

// ── Posílání dat po bitu (MSB first) ──
void sendByte(uint8_t data) {
  for (int i = 7; i >= 0; i--) {
    digitalWrite(CLK_PIN, LOW);
    digitalWrite(DIN_PIN, (data >> i) & 1);
    digitalWrite(CLK_PIN, HIGH);
  }
}

// ── Pošli příkaz do VŠECH zařízení najednou ──
void sendToAll(uint8_t reg, uint8_t data) {
  digitalWrite(CS_PIN, LOW);
  for (int d = 0; d < NUM_DEVICES; d++) {
    sendByte(reg);
    sendByte(data);
  }
  digitalWrite(CS_PIN, HIGH);
}

// ── Pošli příkaz do JEDNOHO konkrétního zařízení (ostatní dostanou NOOP) ──
void sendToOne(uint8_t device, uint8_t reg, uint8_t data) {
  digitalWrite(CS_PIN, LOW);
  // Zařízení jsou v řetězu – data jdou "skrz" od posledního k prvnímu
  for (int d = NUM_DEVICES - 1; d >= 0; d--) {
    if (d == device) {
      sendByte(reg);
      sendByte(data);
    } else {
      sendByte(REG_NOOP);
      sendByte(0x00);
    }
  }
  digitalWrite(CS_PIN, HIGH);
}

// ── Inicializace všech MAX7219 ──
void initAllMax7219() {
  sendToAll(REG_DISPLAYTEST, 0x00);  // Display test OFF
  sendToAll(REG_DECODE,      0x00);  // Žádné dekódování (raw pixely)
  sendToAll(REG_SCANLIMIT,   0x07);  // Všech 8 řádků aktivních
  sendToAll(REG_INTENSITY,   0x00);  // Minimální jas
  sendToAll(REG_SHUTDOWN,    0x01);  // Normální provoz (ne shutdown)
  
  // Vymaž všechny řádky
  for (int row = 1; row <= 8; row++) {
    sendToAll(row, 0x00);
  }
}

// ═══════════════════════════════════════════════════════════════════
//  SETUP
// ═══════════════════════════════════════════════════════════════════

void setup() {
  Serial.begin(9600);
  while (!Serial) { ; }
  
  Serial.println(F("\n╔══════════════════════════════════════════╗"));
  Serial.println(F("║  RAW MAX7219 TEST (bez knihovny)          ║"));
  Serial.println(F("║  DIN=7, CLK=5, CS=6, Zarizeni=8          ║"));
  Serial.println(F("╚══════════════════════════════════════════╝\n"));
  
  // Nastav piny
  pinMode(DIN_PIN, OUTPUT);
  pinMode(CLK_PIN, OUTPUT);
  pinMode(CS_PIN,  OUTPUT);
  
  digitalWrite(CS_PIN,  HIGH);
  digitalWrite(CLK_PIN, LOW);
  digitalWrite(DIN_PIN, LOW);

  // Deaktivuj Ethernet shield CS (pin 10) aby nerušil SPI bus
  pinMode(10, OUTPUT);
  digitalWrite(10, HIGH);  // HIGH = Ethernet chip NEAKTIVNÍ
  
  // Pin 53 (HW SS na Mega) musí být OUTPUT
  pinMode(53, OUTPUT);
  digitalWrite(53, HIGH);

  // ══ TEST 1: Display Test Mode (hardware test – ALL LEDs ON) ══
  Serial.println(F("[TEST 1] Display Test Mode – VSECHNY LED by mely svitit PLNYM jasem"));
  Serial.println(F("         (Toto ignoruje intenzitu – je to hardwarovy test MAX7219)"));
  sendToAll(REG_SHUTDOWN, 0x01);     // Zapni
  sendToAll(REG_DISPLAYTEST, 0x01);  // Display test ON
  delay(3000);
  
  sendToAll(REG_DISPLAYTEST, 0x00);  // Display test OFF
  Serial.println(F("         Display test OFF\n"));
  delay(500);

  // ══ INICIALIZACE ══
  initAllMax7219();
  Serial.println(F("[INIT]  Vsechny MAX7219 inicializovany (jas=0, vse vymazano)\n"));
  delay(1000);

  // ══ TEST 2: Rozsvícení všech LED na minimum ══
  Serial.println(F("[TEST 2] Vsechny LED ON, intenzita 0 (minimum)"));
  sendToAll(REG_INTENSITY, 0x00);
  for (int row = 1; row <= 8; row++) {
    sendToAll(row, 0xFF);
  }
  delay(3000);

  // ══ TEST 3: Intenzita MAX ══
  Serial.println(F("[TEST 3] Intenzita 15 (MAXIMUM) – mel bys videt VELKY rozdil jasu!"));
  sendToAll(REG_INTENSITY, 0x0F);
  delay(3000);

  // ══ TEST 4: Intenzita MIN ══
  Serial.println(F("[TEST 4] Intenzita 0 (MINIMUM) – mel bys videt ztlumeni"));
  sendToAll(REG_INTENSITY, 0x00);
  delay(3000);

  // ══ TEST 5: Vymazání ══
  Serial.println(F("[TEST 5] Vsechny LED OFF"));
  for (int row = 1; row <= 8; row++) {
    sendToAll(row, 0x00);
  }
  delay(1000);

  // ══ TEST 6: Rozsvícení po jednom zařízení ══
  Serial.println(F("[TEST 6] Rozsvecuji matice jednu po druhe (0-7)"));
  Serial.println(F("         Sleduj, KTERA matice se rozsviti a v jakem poradi!\n"));
  
  for (int dev = 0; dev < NUM_DEVICES; dev++) {
    Serial.print(F("  Zarizeni #"));
    Serial.print(dev);
    Serial.println(F(" → sviti"));
    
    // Rozsvíť všechny řádky na tomto zařízení
    for (int row = 1; row <= 8; row++) {
      sendToOne(dev, row, 0xFF);
    }
    delay(1500);
    
    // Zhasni
    for (int row = 1; row <= 8; row++) {
      sendToOne(dev, row, 0x00);
    }
  }

  // ══ TEST 7: Jednoduchý vzor (šachovnice) ══
  Serial.println(F("\n[TEST 7] Sachovnice na vsech maticich"));
  sendToAll(REG_INTENSITY, 0x02);
  for (int row = 1; row <= 8; row++) {
    sendToAll(row, (row % 2 == 0) ? 0xAA : 0x55);
  }
  delay(3000);

  // ══ TEST 8: Diagonální pruh ══
  Serial.println(F("[TEST 8] Diagonalni pruh"));
  for (int row = 1; row <= 8; row++) {
    sendToAll(row, 1 << (row - 1));
  }
  delay(3000);

  Serial.println(F("\n══════════════════════════════════════════"));
  Serial.println(F("  TESTY DOKONCENY"));
  Serial.println(F("══════════════════════════════════════════"));
  Serial.println(F("\nPokud ZADNY test nefungoval:"));
  Serial.println(F("  → Zkontroluj zapojeni DIN/CLK/CS"));
  Serial.println(F("  → Zkus prohodit DIN a CLK kabely"));
  Serial.println(F("  → Zkontroluj napajeni (5V + GND)\n"));
  Serial.println(F("Pokud Display Test (TEST 1) fungoval ale ostatni ne:"));
  Serial.println(F("  → Problem je v komunikaci (SPI piny)\n"));
  Serial.println(F("Pokud intenzita se NEmenila (TEST 2-4):"));
  Serial.println(F("  → Mozny problem s CS pinem\n"));
  Serial.println(F("Pokud testy fungovaly:"));
  Serial.println(F("  → Zapojeni je OK, problem je v knihovne MD_Parola"));
  Serial.println(F("  → Zkus zmenit HARDWARE_TYPE v hlavnim kodu"));
}

void loop() {
  // Po testech blikej šachovnicí
  static bool toggle = false;
  toggle = !toggle;
  
  for (int row = 1; row <= 8; row++) {
    if (toggle) {
      sendToAll(row, (row % 2 == 0) ? 0xAA : 0x55);
    } else {
      sendToAll(row, (row % 2 == 0) ? 0x55 : 0xAA);
    }
  }
  delay(1000);
}
