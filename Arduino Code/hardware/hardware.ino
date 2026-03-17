#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

#define SENSOR_PIN A0
#define SAMPLE_COUNT 15

#define PUMP_PIN 7   //drain pump
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
unsigned long airDetectTimer = 0;

bool pumpTriggered = false;

// -------- Existing variables --------
float filteredNTU = 0;
float alpha = 0.12;
bool firstRun = true;

int previousDisplayNTU = -1;
int previousState = -1;


// -------- Median Filter --------
int medianFilter(int arr[], int size) {
  for (int i = 0; i < size - 1; i++) {
    for (int j = i + 1; j < size; j++) {
      if (arr[j] < arr[i]) {
        int temp = arr[i];
        arr[i] = arr[j];
        arr[j] = temp;
      }
    }
  }
  return arr[size / 2];
}


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

  // -------- 1️⃣ Collect Samples --------
  int samples[SAMPLE_COUNT];

  for (int i = 0; i < SAMPLE_COUNT; i++) {
    samples[i] = analogRead(SENSOR_PIN);
    delay(10);
  }

  // -------- 2️⃣ Median Filtering --------
  int medianValue = medianFilter(samples, SAMPLE_COUNT);

  // -------- 3️⃣ Convert to Voltage --------
  float voltage = medianValue * (5.0 / 1023.0);

  // -------- 4️⃣ Calibrated NTU Equation --------
  float rawNTU = (3.45 - voltage) * 280 + 20;
  if (rawNTU < 0) rawNTU = 0;

  // -------- 5️⃣ Exponential Smoothing --------
  if (firstRun) {
    filteredNTU = rawNTU;
    firstRun = false;
  } else {
    filteredNTU = alpha * rawNTU + (1 - alpha) * filteredNTU;
  }

  int displayNTU = (int)filteredNTU;
  Serial.print("Voltage: ");
  Serial.print(voltage, 3);
  Serial.print("  NTU: ");
  Serial.println(displayNTU);

  // --------  Backup Reset (air detection with validation) --------
  if (voltage > airVoltageThreshold && displayNTU < 80 && systemState == IDLE) {
    if (airDetectTimer == 0)
      airDetectTimer = millis();

    if (millis() - airDetectTimer > 2000) {  // 2 sec confirmation
      if (pumpTriggered == true) {
        Serial.println("Backup Reset: Air detected");
      }
      pumpTriggered = false;
    }

  } else {
    airDetectTimer = 0;
  }

  // -------- Detect cloudy condition --------
  bool cloudy;

  if (displayNTU >= 50)
    cloudy = true;
  else if (displayNTU <= 40)
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
        pumpTriggered = false;
        Serial.println("Refill Pump OFF");
      }
      break;
  }

  // -------- 6️⃣ Water Quality Classification --------
  int state;

  if (displayNTU < 50)
    state = 0;
  else if (displayNTU < 300)
    state = 1;
  else
    state = 2;

  // -------- 7️⃣ LCD Update --------
  if (abs(displayNTU - previousDisplayNTU) > 5 || state != previousState) {

    lcd.setCursor(0, 0);
    lcd.print("NTU:       ");
    lcd.setCursor(5, 0);
    lcd.print(displayNTU);

    lcd.setCursor(0, 1);
    lcd.print("Status:    ");
    lcd.setCursor(8, 1);

    if (state == 0)
      lcd.print("CLEAN ");
    else if (state == 1)
      lcd.print("DIRTY ");
    else
      lcd.print("VERY DIRTY ");

    previousDisplayNTU = displayNTU;
    previousState = state;
  }

  delay(250);
}