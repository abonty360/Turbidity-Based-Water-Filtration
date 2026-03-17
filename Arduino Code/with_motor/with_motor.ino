#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x20, 16, 2);

#define SENSOR_PIN A0
#define SAMPLE_COUNT 20

#define PUMP_PIN 7   // drain pump
#define PUMP2_PIN 6  // refill pump
const float airVoltageThreshold = 3.2;

// ---------- Pump timing ----------
const unsigned long confirmTime = 3000;
const unsigned long drainTime = 10000;
const unsigned long waitTime = 5000;
const unsigned long refillTime = 10000;

// ---------- System states ----------
#define IDLE 0
#define DRAIN 1
#define WAIT 2
#define REFILL 3

int systemState = IDLE;

unsigned long stateTimer = 0;
unsigned long confirmTimer = 0;

bool pumpTriggered = false;

// ---------- Turbidity filtering ----------
float filteredNTU;
float alpha = 0.15;
bool firstRun = true;

int previousNTU = -1;
int previousState = -1;

void setup() {
  Serial.begin(9600);

  pinMode(PUMP_PIN, OUTPUT);
  pinMode(PUMP2_PIN, OUTPUT);

  digitalWrite(PUMP_PIN, LOW);
  digitalWrite(PUMP2_PIN, LOW);

  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("Turbidity Meter");
  lcd.setCursor(0, 1);
  lcd.print("Warming Up...");
  delay(3000);

  lcd.clear();
}

void loop() {

  // -------- Sensor Sampling --------

  long sum = 0;

  for (int i = 0; i < SAMPLE_COUNT; i++) {
    sum += analogRead(SENSOR_PIN);
    delay(5);
  }

  int avgValue = sum / SAMPLE_COUNT;


  // -------- Convert ADC to NTU --------

  int rawNTU = map(avgValue, 0, 1023, 3000, 0);


  // -------- Exponential smoothing --------

  if (firstRun) {
    filteredNTU = rawNTU;
    firstRun = false;
  } else {
    filteredNTU = alpha * rawNTU + (1 - alpha) * filteredNTU;
  }

  int displayNTU = (int)filteredNTU;


  // -------- Reset trigger (Proteus empty tank simulation) --------

  if (displayNTU >= 2100) {
    if (pumpTriggered == true) {
      Serial.println("Pump cycle reset (empty tank detected)");
    }
    pumpTriggered = false;
  }


  // -------- Detect cloudy condition --------

  bool cloudy;

  if (displayNTU >= 1000)
    cloudy = true;
  else if (displayNTU <= 900)
    cloudy = false;
  else
    cloudy = (systemState != IDLE);


  // -------- Start filtration cycle --------

  if (systemState == IDLE && cloudy && !pumpTriggered) {
    if (confirmTimer == 0)
      confirmTimer = millis();

    if (millis() - confirmTimer > confirmTime) {
      systemState = DRAIN;
      stateTimer = millis();
      pumpTriggered = true;

      digitalWrite(PUMP_PIN, HIGH);

      Serial.println("Drain Pump ON");
    }
  } else {
    confirmTimer = 0;
  }


  // -------- State Machine --------

  switch (systemState) {

    case DRAIN:

      if (millis() - stateTimer >= drainTime) {
        digitalWrite(PUMP_PIN, LOW);
        Serial.print("Drain runtime (ms): ");
    Serial.println(millis() - stateTimer);

        systemState = WAIT;
        stateTimer = millis();

        Serial.println("Drain Pump OFF");
      }

      break;


    case WAIT:

      if (millis() - stateTimer >= waitTime) {
        Serial.print("Wait time (ms): ");
        Serial.println(millis() - stateTimer);
        digitalWrite(PUMP2_PIN, HIGH);

        systemState = REFILL;
        stateTimer = millis();

        Serial.println("Refill Pump ON");
      }

      break;


    case REFILL:

      if (millis() - stateTimer >= refillTime) {
        digitalWrite(PUMP2_PIN, LOW);
        Serial.print("Refill runtime (ms): ");
    Serial.println(millis() - stateTimer);

        systemState = IDLE;
        stateTimer = 0;

        Serial.println("Refill Pump OFF");
      }

      break;
  }


  // -------- Water Quality Classification --------

  int state;

  if (displayNTU < 1000)
    state = 0;
  else if (displayNTU < 2000)
    state = 1;
  else
    state = 2;


  // -------- LCD Update --------

  if (abs(displayNTU - previousNTU) > 5 || state != previousState) {

    lcd.setCursor(0, 0);
    lcd.print("NTU:      ");
    lcd.setCursor(5, 0);
    lcd.print(displayNTU);

    lcd.setCursor(0, 1);
    lcd.print("Status:   ");
    lcd.setCursor(8, 1);

    if (state == 0)
      lcd.print("CLEAN ");
    else if (state == 1)
      lcd.print("CLOUDY");
    else
      lcd.print("DIRTY ");

    previousNTU = displayNTU;
    previousState = state;
  }

  delay(100);
}