#include <Arduino.h>
#include <BleGamepad.h>
#include <SPI.h>

#define CHECK_BIT(var, pos) ((var) & (1 << (pos)))
#define numOfButtons 16
#define numOfHatSwitches 4

BleGamepad bleGamepad("jojo Gamepad", "Dio");
BleGamepadConfiguration bleGamepadConfig;

void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, 16, 17);
  Serial.println("Starting BLE work!");
  bleGamepadConfig.setAutoReport(false);
  bleGamepadConfig.setControllerType(CONTROLLER_TYPE_GAMEPAD);  // CONTROLLER_TYPE_JOYSTICK, CONTROLLER_TYPE_GAMEPAD (DEFAULT), CONTROLLER_TYPE_MULTI_AXIS
  bleGamepadConfig.setButtonCount(numOfButtons);
  bleGamepadConfig.setHatSwitchCount(numOfHatSwitches);
  bleGamepadConfig.setVid(0xe502);
  bleGamepadConfig.setPid(0xbbab);
  // Some non-Windows operating systems and web based gamepad testers don't like min axis set below 0, so 0 is set by default
  bleGamepadConfig.setAxesMin(0x8000);  // -32768 --> int16_t - 16 bit signed integer - Can be in decimal or hexadecimal
  //bleGamepadConfig.setAxesMin(0x0000);  // 0 --> int16_t - 16 bit signed integer - Can be in decimal or hexadecimal
  bleGamepadConfig.setAxesMax(0x7FFF);  // 32767 --> int16_t - 16 bit signed integer - Can be in decimal or hexadecimal
  bleGamepad.begin(&bleGamepadConfig);  // Simulation controls, special buttons and hats 2/3/4 are disabled by default
}

uint8_t padReport[12] = { 0 };
uint8_t prev_padReport[12] = { 0 };

void loop() {
  while (Serial2.available() <= 0 || Serial2.read() != 0xff)
    ;
  int readAvailable = 0;
  do {
    readAvailable = Serial2.available();
    if (readAvailable >= sizeof(padReport)) {
      memcpy(prev_padReport, padReport, sizeof(padReport));
      Serial2.readBytes((char *)padReport, sizeof(padReport));
      char buffer[5];
      for (int i = 0; i < sizeof(padReport); i++) {
        sprintf(buffer, "%02X ", padReport[i]);
        Serial.print(buffer);
      }
      Serial.println(readAvailable);
    }
  } while (readAvailable < sizeof(padReport));

  // Bluetooth
  if (bleGamepad.isConnected()) {
    // A
    if (CHECK_BIT(padReport[1], 4) && !CHECK_BIT(prev_padReport[1], 4)) {
      bleGamepad.press(1);
      bleGamepad.sendReport();
    }
    if (!CHECK_BIT(padReport[1], 4) && CHECK_BIT(prev_padReport[1], 4)) {
      bleGamepad.release(1);
      bleGamepad.sendReport();
    }
    // X
    if (CHECK_BIT(padReport[1], 6) && !CHECK_BIT(prev_padReport[1], 6)) {
      bleGamepad.press(4);
      bleGamepad.sendReport();
    }
    if (!CHECK_BIT(padReport[1], 6) && CHECK_BIT(prev_padReport[1], 6)) {
      bleGamepad.release(4);
      bleGamepad.sendReport();
    }
    int16_t lx = *((int16_t *)(padReport + 4));
    int16_t ly = -(*((int16_t *)(padReport + 6)) + 1);
    int16_t rx = *((int16_t *)(padReport + 8));
    int16_t ry = -(*((int16_t *)(padReport + 10)) + 1);
    bleGamepad.setAxes(lx, ly, 0, 0, rx, ry);
    bleGamepad.sendReport();
    /*long lx = (long)*((int16_t *)(padReport + 4));
    long ly = (long)-(*((int16_t *)(padReport + 6)));
    long rx = (long)*((int16_t *)(padReport + 8));
    long ry = (long)-(*((int16_t *)(padReport + 10)));
    bleGamepad.setAxes(map(lx, -32768, 32767, 0, 32767), map(ly, -32768, 32767, 0, 32767), 0, 0, map(rx, -32768, 32767, 0, 32767), map(ry, -32768, 32767, 0, 32767));
    bleGamepad.sendReport();*/
    /*int i = 4;
    Serial.println(i);
    bleGamepad.press(i);
    bleGamepad.sendReport();
    delay(1000);
    bleGamepad.release(i);
    bleGamepad.sendReport();
    delay(1000);
    i = 1;
    Serial.println(i);
    bleGamepad.press(i);
    bleGamepad.sendReport();
    delay(500);
    bleGamepad.release(i);
    bleGamepad.sendReport();
    delay(1000);
    // 1A 2B 4X 5Y

    Serial.println("Move all axis simultaneously from min to max");
    int mid = (bleGamepadConfig.getAxesMax() - bleGamepadConfig.getAxesMin()) / 2;
    for (i = bleGamepadConfig.getAxesMin(); i < bleGamepadConfig.getAxesMax(); i += (bleGamepadConfig.getAxesMax() / 256) + 1) {
      bleGamepad.setAxes(i, mid, 0, 0, mid, mid);
      bleGamepad.sendReport();
      delay(10);
    }
    bleGamepad.setAxes(mid, mid, 0, 0, mid, mid);
    bleGamepad.sendReport();

    delay(1000);*/
  }
}