/*
 * ═══════════════════════════════════════════════════════════════════
 *  DIAGNOSTICKÝ SKETCH – test HARDWARE_TYPE + intenzity
 *  Nahraj do Mega, otevři Serial Monitor (9600 baud) a sleduj displej.
 *  Každých 5 s se přepne na další HW typ a zobrazí jeho název.
 *  U správného typu uvidíš čitelný text "TEST" nebo název typu.
 * ═══════════════════════════════════════════════════════════════════
 */

#include <MD_Parola.h>
#include <MD_MAX72xx.h>

// --- Piny (softwarové SPI) ---
#define DIN_PIN   7
#define CLK_PIN   5
#define CS_PIN    6
#define MAX_DEVICES 8

// Pole HW typů k otestování
const MD_MAX72XX::moduleType_t hwTypes[] = {
  MD_MAX72XX::PAROLA_HW,
  MD_MAX72XX::GENERIC_HW,
  MD_MAX72XX::ICSTATION_HW,
  MD_MAX72XX::FC16_HW
};

const char* hwNames[] = {
  "PAROLA",
  "GENERIC",
  "ICSTATION",
  "FC16"
};

const int NUM_TYPES = 4;
int currentType = 0;

// Vytvoříme ukazatel na displej – budeme ho znovu vytvářet pro každý typ
MD_Parola* display = nullptr;

void testDisplay(int typeIndex) {
  // Uvolni předchozí instanci
  if (display != nullptr) {
    delete display;
    display = nullptr;
  }

  Serial.print(F("\n══════════════════════════════════\n"));
  Serial.print(F("  Test #"));
  Serial.print(typeIndex + 1);
  Serial.print(F("/4: "));
  Serial.println(hwNames[typeIndex]);
  Serial.println(F("══════════════════════════════════"));

  // Vytvoř novou instanci s daným HW typem
  display = new MD_Parola(hwTypes[typeIndex], DIN_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);

  display->begin();
  display->setIntensity(0);   // MINIMUM jasu
  display->displayClear();

  // Zobraz název HW typu
  display->setTextAlignment(PA_CENTER);
  display->print(hwNames[typeIndex]);

  Serial.print(F("  Intenzita: 0 (minimum)\n"));
  Serial.print(F("  Zobrazuji: "));
  Serial.println(hwNames[typeIndex]);
  Serial.println(F("  → Pokud vidíš čitelný text, TOTO je správný typ!"));
  Serial.println(F("  → Pokud je jas stále vysoký, intenzita nefunguje."));
}

void setup() {
  Serial.begin(9600);
  while (!Serial) { ; }

  Serial.println(F("\n╔══════════════════════════════════════╗"));
  Serial.println(F("║  MAX7219 DIAGNOSTIKA                 ║"));
  Serial.println(F("║  Piny: DIN=7, CLK=5, CS=6           ║"));
  Serial.println(F("║  Zařízení: 8                         ║"));
  Serial.println(F("║  Každých 5s se změní HW typ          ║"));
  Serial.println(F("╚══════════════════════════════════════╝"));

  // Začni prvním typem
  testDisplay(0);
}

void loop() {
  // Čekej 5 sekund, pak přepni na další typ
  delay(5000);

  currentType++;
  if (currentType >= NUM_TYPES) {
    currentType = 0;
    Serial.println(F("\n\n*** CYKLUS DOKONČEN – začínám znovu ***\n"));
  }

  testDisplay(currentType);

  // Po zobrazení názvu typu, po 2.5s zobraz "12:34" jako test reálného textu
  delay(2500);
  display->displayClear();
  display->print("12:34");
  Serial.println(F("  Zobrazuji: 12:34"));

  // Zbylých 2.5s čekání je na začátku dalšího loop()
  delay(2500);

  // Test intenzity – rozsvítíme na MAX a zpět na MIN
  Serial.println(F("  Test intenzity: MAX (15)..."));
  display->setIntensity(15);
  delay(2000);
  Serial.println(F("  Test intenzity: MIN (0)..."));
  display->setIntensity(0);
  delay(2000);
}
