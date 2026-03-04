#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);  // Change to 0x3F if needed

#define SENSOR_PIN A0
#define SAMPLE_COUNT 15
#define PUMP_PIN 7

bool pumpState = false;

unsigned long dirtyStartTime = 0;
unsigned long cleanStartTime = 0;

const unsigned long triggerDelay = 5000;   

float filteredNTU = 0;
float alpha = 0.12;  // Smoothing factor (0.08–0.15 recommended)

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
  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("Turbidity Meter");
  lcd.setCursor(0, 1);
  lcd.print("Warming Up...");
  delay(3000);  // Sensor stabilization time

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
  // Typical quadratic curve for turbidity sensor (e.g., SEN0189)
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

  Serial.print("  RawNTU: ");
  Serial.print(rawNTU);

  Serial.print("  FilteredNTU: ");
  Serial.println(displayNTU);

  // -------- 6️⃣ Water Quality Classification --------
  int state;

  if (displayNTU < 50)
    state = 0;  // CLEAN
  else if (displayNTU < 300)
    state = 1;  // CLOUDY
  else
    state = 2;  // DIRTY

  // -------- 7️⃣ Update LCD Only If Needed --------
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
      lcd.print("CLOUDY");
    else
      lcd.print("DIRTY ");

    previousDisplayNTU = displayNTU;
    previousState = state;
  }

  delay(250);
}