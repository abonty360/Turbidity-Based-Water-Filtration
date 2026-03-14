#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x20, 16, 2);  // Change to 0x27 if needed

#define SENSOR_PIN A0
#define SAMPLE_COUNT 20
#define PUMP_PIN 7

bool pumpState = false;

unsigned long stateChangeTimer = 0;
unsigned long pumpStartTime = 0;

const unsigned long confirmTime = 3000;  // 3 seconds
const unsigned long pumpRunTime = 10000;
bool pumpTriggered = false;


float filteredNTU;
float alpha = 0.15;
bool firstRun = true;

int previousNTU = -1;
int previousState = -1;


void setup() {
  Serial.begin(9600);

  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, LOW);

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
    delay(5);  // improves ADC stability
  }

  int avgValue = sum / SAMPLE_COUNT;


  // -------- Convert ADC to NTU --------

  int rawNTU = map(avgValue, 0, 1023, 3000, 0);


  // -------- Exponential Smoothing --------

  if (firstRun) {
    filteredNTU = rawNTU;
    firstRun = false;
  } else {
    filteredNTU = alpha * rawNTU + (1 - alpha) * filteredNTU;
  }

  int displayNTU = (int)filteredNTU;

  // -------- Pump Decision Logic (Hysteresis) --------

  bool shouldPumpRun;
  if (displayNTU >= 1000)
    shouldPumpRun = true;
  else if (displayNTU <= 900)
    shouldPumpRun = false;
  else
    shouldPumpRun = pumpState;

  // -------- Reset trigger when water becomes clean --------

  if (displayNTU <= 900) {
    pumpTriggered = false;
  }

  // -------- Start pump only once per cloudy event --------

  if (!pumpState && !pumpTriggered && shouldPumpRun) {
    if (stateChangeTimer == 0)
      stateChangeTimer = millis();

    if (millis() - stateChangeTimer > confirmTime) {
      pumpState = true;
      pumpTriggered = true;

      digitalWrite(PUMP_PIN, HIGH);
      pumpStartTime = millis();

      Serial.println("Pump ON");

      stateChangeTimer = 0;
    }
  } else {
    stateChangeTimer = 0;
  }

  // -------- Fixed Pump Runtime (10s) --------

  if (pumpState && millis() - pumpStartTime >= pumpRunTime) {
    digitalWrite(PUMP_PIN, LOW);
    pumpState = false;
    Serial.println(millis() - pumpStartTime);
    Serial.println("Pump OFF (10s completed)");
  }

  // -------- Water Quality Classification --------

  int state;

  if (displayNTU < 1000)
    state = 0;
  else if (displayNTU < 2000)
    state = 1;
  else
    state = 2;


  // -------- LCD Update (only when needed) --------

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
