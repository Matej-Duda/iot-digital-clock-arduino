/*
 * MINIMÁLNÍ TEST – 1 modul (4 matice 8x8)
 * BEZ Ethernet Shieldu!
 * 
 * ZAPOJENÍ (připoj na stranu DIN modulu, NE na DOUT!):
 *   DIN → pin 7
 *   CLK → pin 5
 *   CS  → pin 6
 *   VCC → 5V
 *   GND → GND
 */

#define DIN_PIN   7
#define CLK_PIN   5
#define CS_PIN    6
#define NUM_DEVICES 4  // Jen 1 modul = 4 matice

void sendByte(uint8_t data) {
  for (int i = 7; i >= 0; i--) {
    digitalWrite(CLK_PIN, LOW);
    delayMicroseconds(5);           // ← zpoždění
    digitalWrite(DIN_PIN, (data >> i) & 1);
    delayMicroseconds(5);           // ← data setup time
    digitalWrite(CLK_PIN, HIGH);
    delayMicroseconds(5);           // ← data hold time
  }
}

void sendAll(uint8_t reg, uint8_t data) {
  digitalWrite(CS_PIN, LOW);
  delayMicroseconds(10);            // ← CS setup
  for (int i = 0; i < NUM_DEVICES; i++) {
    sendByte(reg);
    sendByte(data);
  }
  delayMicroseconds(10);            // ← CS hold
  digitalWrite(CS_PIN, HIGH);
  delayMicroseconds(10);            // ← mezi příkazy
}

void setup() {
  Serial.begin(9600);
  Serial.println(F("=== TEST 1 MODUL (4 matice) ==="));
  
  pinMode(DIN_PIN, OUTPUT);
  pinMode(CLK_PIN, OUTPUT);
  pinMode(CS_PIN, OUTPUT);
  
  digitalWrite(CS_PIN, HIGH);
  digitalWrite(CLK_PIN, LOW);

  // 1) Display Test Mode – hardwarový test v čipu MAX7219
  //    Pokud tohle nerozsvítí VŠECHNY LED = špatné zapojení!
  Serial.println(F("TEST 1: Display Test ON (vsechny LED musi svitit)"));
  sendAll(0x0C, 0x01);  // Normal operation
  sendAll(0x0F, 0x01);  // Display test ON
  delay(5000);
  
  // 2) Display test OFF, init
  sendAll(0x0F, 0x00);
  sendAll(0x09, 0x00);  // No decode
  sendAll(0x0B, 0x07);  // Scan all rows
  sendAll(0x0A, 0x00);  // Intensity MIN
  sendAll(0x0C, 0x01);  // Normal operation
  
  // Vymaž
  for (int i = 1; i <= 8; i++) sendAll(i, 0x00);
  Serial.println(F("TEST 2: Vse vymazano (vsechny LED musi ZHASNOUT)"));
  delay(3000);
  
  // 3) Rozsvít vše
  Serial.println(F("TEST 3: Vsechny ON (nizky jas)"));
  for (int i = 1; i <= 8; i++) sendAll(i, 0xFF);
  delay(3000);
  
  Serial.println(F("\nHOTOVO. Sleduj co displej ukazuje."));
  Serial.println(F("Pokud nic nefunguje:"));
  Serial.println(F("  1. Zkontroluj ze pripojujes na DIN stranu modulu (NE DOUT!)"));
  Serial.println(F("  2. Zkus prohodit DIN a CLK draty"));
  Serial.println(F("  3. Zkontroluj 5V a GND"));
}

void loop() {
  // Blikání ON/OFF
  Serial.println(F("ON"));
  for (int i = 1; i <= 8; i++) sendAll(i, 0xFF);
  delay(1500);
  
  Serial.println(F("OFF"));
  for (int i = 1; i <= 8; i++) sendAll(i, 0x00);
  delay(1500);
}
