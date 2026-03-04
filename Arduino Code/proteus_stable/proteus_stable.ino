#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x20, 16, 2);  //Change address if needed, hardware ususally works on x27

#define SENSOR_PIN A0
#define SAMPLE_COUNT 20

float filteredNTU;
float alpha = 0.15;
bool firstRun = true;

int previousNTU = -1;
int previousState = -1;

void setup() {
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

  long sum = 0;
  for(int i = 0; i < SAMPLE_COUNT; i++) {
    sum += analogRead(SENSOR_PIN);
  }

  int avgValue = sum / SAMPLE_COUNT;

  // Lightweight mapping instead of quadratic formula
  int rawNTU = map(avgValue, 0, 1023, 3000, 0);

  if(firstRun) {
    filteredNTU = rawNTU;
    firstRun = false;
  } else {
    filteredNTU = alpha * rawNTU + (1 - alpha) * filteredNTU;
  }

  int displayNTU = (int)filteredNTU;

  int state;
  if(displayNTU < 1000)
    state = 0;
  else if(displayNTU < 2000)
    state = 1;
  else
    state = 2;

  if(abs(displayNTU - previousNTU) > 5 || state != previousState) {

    lcd.setCursor(0,0);
    lcd.print("NTU:      ");
    lcd.setCursor(5,0);
    lcd.print(displayNTU);

    lcd.setCursor(0,1);
    lcd.print("Status:   ");
    lcd.setCursor(8,1);

    if(state == 0) lcd.print("CLEAN ");
    else if(state == 1) lcd.print("CLOUDY");
    else lcd.print("DIRTY ");

    previousNTU = displayNTU;
    previousState = state;
  }

  delay(150);
}