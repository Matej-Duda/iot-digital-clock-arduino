/*
 * Test 8× MAX7219 = 2 moduly × 4 matice
 * Každý modul na vlastních pinech (bez daisy-chain)
 *
 * ZAPOJENÍ:
 *   Modul 1 (horní):  DIN→pin 2,  CLK→pin 3,  CS→pin 4,   VCC→5V, GND→GND
 *   Modul 2 (dolní):  DIN→pin 22, CLK→pin 23, CS→pin 24,  VCC→5V, GND→GND
 */

#include "LedControl.h"

// Modul 1 (horní řada, 4 matice)
LedControl lc1 = LedControl(2, 3, 4, 4);

// Modul 2 (dolní řada, 4 matice) – jiné piny!
LedControl lc2 = LedControl(22, 23, 24, 4);

unsigned long delaytime = 300;

void setup() {
  Serial.begin(9600);
  Serial.println(F("=== TEST 2x4 MODULY (kazdy na jinych pinech) ==="));

  // Inicializace modulu 1
  for (int i = 0; i < 4; i++) {
    lc1.shutdown(i, false);
    lc1.setIntensity(i, 1);
    lc1.clearDisplay(i);
  }

  // Inicializace modulu 2
  for (int i = 0; i < 4; i++) {
    lc2.shutdown(i, false);
    lc2.setIntensity(i, 1);
    lc2.clearDisplay(i);
  }

  Serial.println(F("Oba moduly inicializovany."));
}

void loop() {
  // Rozsvěcuj LED po jedné na VŠECH 8 maticích
  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      // Modul 1 (zařízení 0-3)
      for (int dev = 0; dev < 4; dev++) {
        lc1.setLed(dev, row, col, true);
      }
      // Modul 2 (zařízení 0-3, ale fyzicky matice 4-7)
      for (int dev = 0; dev < 4; dev++) {
        lc2.setLed(dev, row, col, true);
      }
      delay(delaytime);
    }
  }

  // Vymaž vše
  delay(500);
  for (int dev = 0; dev < 4; dev++) {
    lc1.clearDisplay(dev);
    lc2.clearDisplay(dev);
  }
  delay(500);
}
