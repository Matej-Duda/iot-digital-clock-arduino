/*
 * DIAGNOSTIKA POZIC A ROTACE – Max72xxPanel
 * 
 * Tento sketch zobrazí na každé 8x8 matici její číslo (0-7).
 * Otevři Serial Monitor (9600) a sleduj displej.
 * 
 * Pak mi řekni:
 *   - Které číslo vidíš na které matici (a jestli je čitelné)
 *   - Jestli je číslo otočené (a jak – o 90°, 180°, zrcadlově?)
 * 
 * Sketch zkouší postupně rotace 0, 1, 2, 3 – u každé čeká 5 sekund.
 * 
 * Zapojení: DIN→51(MOSI), CLK→52(SCK), CS→6
 */

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Max72xxPanel.h>

#define CS_PIN      6
#define H_DISPLAYS  4
#define V_DISPLAYS  2

Max72xxPanel matrix = Max72xxPanel(CS_PIN, H_DISPLAYS, V_DISPLAYS);

void showDeviceNumbers(int rotation) {
  // Nastav rotaci pro všechny moduly
  for (int i = 0; i < 8; i++) {
    matrix.setRotation(i, rotation);
  }

  matrix.fillScreen(LOW);
  matrix.setTextSize(1);
  matrix.setTextColor(HIGH);
  matrix.setTextWrap(false);

  // Zobraz číslo 0-7 na každém modulu
  // Každý modul je 8x8 pixelů, takže pozice v mřížce 4×2:
  //   (0,0) (1,0) (2,0) (3,0)   ← horní řada
  //   (0,1) (1,1) (2,1) (3,1)   ← dolní řada
  for (int dev = 0; dev < 8; dev++) {
    int gridX, gridY;
    if (dev < 4) {
      gridX = dev;
      gridY = 0;
    } else {
      gridX = dev - 4;
      gridY = 1;
    }
    // Pozice pixelu: gridX * 8 + offset, gridY * 8
    int px = gridX * 8 + 1;
    int py = gridY * 8;
    matrix.setCursor(px, py);
    matrix.print(dev);
  }

  matrix.write();
}

void setup() {
  Serial.begin(9600);
  while (!Serial) { ; }

  Serial.println(F("\n╔══════════════════════════════════════════╗"));
  Serial.println(F("║  DIAGNOSTIKA POZIC A ROTACE              ║"));
  Serial.println(F("║  Max72xxPanel  4x2                       ║"));
  Serial.println(F("║  CS=6, HW SPI (51/52)                    ║"));
  Serial.println(F("╚══════════════════════════════════════════╝\n"));

  // Deaktivuj Ethernet CS
  pinMode(10, OUTPUT);
  digitalWrite(10, HIGH);
  pinMode(53, OUTPUT);

  matrix.setIntensity(1);  // Nízký jas

  // ── TEST POZIC: výchozí mapování (device = grid pozice) ──
  // setPosition: device 0 → (0,0), 1 → (1,0), ... 4 → (0,1), ...
  Serial.println(F("=== POZICE: Výchozí (0→(0,0), 1→(1,0), ...) ===\n"));
  for (int i = 0; i < 4; i++) {
    matrix.setPosition(i, i, 0);       // horní řada
    matrix.setPosition(i + 4, i, 1);   // dolní řada
  }

  // Testuj rotace 0-3
  for (int rot = 0; rot < 4; rot++) {
    Serial.print(F("  Rotace "));
    Serial.print(rot);
    Serial.println(F(" → Vidíš čísla 0-7? Jsou čitelná? (5 sekund)"));
    showDeviceNumbers(rot);
    delay(5000);
  }

  // ── TEST POZIC: obrácené mapování ──
  Serial.println(F("\n=== POZICE: Obrácené (0→(3,0), 1→(2,0), ...) ===\n"));
  for (int i = 0; i < 4; i++) {
    matrix.setPosition(i, 3 - i, 0);       // horní řada pozpátku
    matrix.setPosition(i + 4, 3 - i, 1);   // dolní řada pozpátku
  }

  for (int rot = 0; rot < 4; rot++) {
    Serial.print(F("  Rotace "));
    Serial.print(rot);
    Serial.println(F(" → Vidíš čísla 0-7? Jsou čitelná? (5 sekund)"));
    showDeviceNumbers(rot);
    delay(5000);
  }

  // ── HOTOVO ──
  Serial.println(F("\n══════════════════════════════════════════"));
  Serial.println(F("  8 testů hotovo. Řekni mi:"));
  Serial.println(F("  1. U kterého testu byly čísla ČITELNÁ?"));
  Serial.println(F("     (Serial Monitor řekne 'Rotace X' a pozici)"));
  Serial.println(F("  2. Jaké číslo je na jaké pozici? (zleva doprava)"));
  Serial.println(F("══════════════════════════════════════════\n"));
}

void loop() {
  // Po testech blikej jednoduchou šachovnicí
  static bool toggle = false;
  toggle = !toggle;

  for (int i = 0; i < 8; i++) {
    matrix.setRotation(i, 0);
  }

  matrix.fillScreen(toggle ? HIGH : LOW);
  matrix.write();
  delay(1000);
}
