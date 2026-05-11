/*
 * NEJJEDNODUŠŠÍ TEST MAX7219
 * Jen zapne a vypne všechny LED přes hardware SPI.
 * Pokud tohle nefunguje = špatné zapojení!
 * 
 * ZAPOJENÍ:
 *   DIN → pin 51 (MOSI)
 *   CLK → pin 52 (SCK)
 *   CS  → pin 6
 *   VCC → 5V
 *   GND → GND
 */

#include <SPI.h>

#define CS_PIN 6
#define NUM_DEVICES 8

void sendAll(byte reg, byte data) {
  digitalWrite(CS_PIN, LOW);
  for (int i = 0; i < NUM_DEVICES; i++) {
    SPI.transfer(reg);
    SPI.transfer(data);
  }
  digitalWrite(CS_PIN, HIGH);
}

void setup() {
  Serial.begin(9600);
  Serial.println(F("=== JEDNODUCHY TEST MAX7219 ==="));
  
  pinMode(CS_PIN, OUTPUT);
  digitalWrite(CS_PIN, HIGH);
  
  // Deaktivuj Ethernet CS (pin 10) aby nerušil SPI
  pinMode(10, OUTPUT);
  digitalWrite(10, HIGH);
  
  pinMode(53, OUTPUT);  // HW SS na Mega
  
  SPI.begin();
  SPI.setClockDivider(SPI_CLOCK_DIV16);
  
  // Inicializace MAX7219
  sendAll(0x0F, 0x00);  // Display test OFF
  sendAll(0x09, 0x00);  // No decode
  sendAll(0x0B, 0x07);  // Scan all 8 rows
  sendAll(0x0A, 0x01);  // Intensity = 1 (nízký jas!)
  sendAll(0x0C, 0x01);  // Normal operation (ne shutdown)
  
  // Vymaž displej
  for (int i = 1; i <= 8; i++) sendAll(i, 0x00);
  
  Serial.println(F("Inicializace hotova."));
  Serial.println(F("Nyni budu stridat: VSECHNY ON → VSECHNY OFF"));
  Serial.println(F("Pokud NEVIDIS blikani → SPATNE ZAPOJENI!"));
}

void loop() {
  // Všechny LED ON
  Serial.println(F(">> VSECHNY LED ON"));
  for (int i = 1; i <= 8; i++) sendAll(i, 0xFF);
  delay(2000);
  
  // Všechny LED OFF
  Serial.println(F(">> VSECHNY LED OFF"));
  for (int i = 1; i <= 8; i++) sendAll(i, 0x00);
  delay(2000);
  
  // Display Test Mode (hardwarový – ignoruje registry, plný jas)
  Serial.println(F(">> DISPLAY TEST MODE (plny jas, hw test)"));
  sendAll(0x0F, 0x01);
  delay(2000);
  sendAll(0x0F, 0x00);
  delay(500);
}
