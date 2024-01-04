#include <Arduino.h>
#include <BleGamepad.h>

#define CHECK_BIT(var, pos) ((var) & (1 << (pos)))
#define numOfButtons 16
#define numOfHatSwitches 4

BleGamepad bleGamepad("jojo pad", "dio");
BleGamepadConfiguration bleGamepadConfig;

void setup() {
  //Serial.begin(115200);
  //Serial.println("Starting Xbox 360 Wireless to BLE gamepad!");
  Serial2.begin(115200, SERIAL_8N1, 16, 17);
  bleGamepadConfig.setAutoReport(false);
  bleGamepadConfig.setControllerType(CONTROLLER_TYPE_GAMEPAD);  // CONTROLLER_TYPE_JOYSTICK, CONTROLLER_TYPE_GAMEPAD (DEFAULT), CONTROLLER_TYPE_MULTI_AXIS
  bleGamepadConfig.setButtonCount(numOfButtons);
  bleGamepadConfig.setHatSwitchCount(numOfHatSwitches);
  bleGamepadConfig.setVid(0xe502);
  bleGamepadConfig.setPid(0xbbab);
  // Some non-Windows operating systems and web based gamepad testers don't like min axis set below 0
  bleGamepadConfig.setAxesMin(0x8000);  // -32768 --> int16_t - 16 bit signed integer - Can be in decimal or hexadecimal
  //bleGamepadConfig.setAxesMin(0x0000);  // 0 --> int16_t - 16 bit signed integer - Can be in decimal or hexadecimal
  bleGamepadConfig.setAxesMax(0x7FFF);  // 32767 --> int16_t - 16 bit signed integer - Can be in decimal or hexadecimal
  bleGamepad.begin(&bleGamepadConfig);  // Simulation controls, special buttons and hats 2/3/4 are disabled by default
}

bool sumErr = false;
uint8_t padReport[13] = { 0 };
uint8_t prev_padReport[12] = { 0 };

void loop() {
  while (Serial2.available() <= 0 || Serial2.read() != 0xaa)
    ;
  while (1) {
    if (Serial2.available() >= sizeof(padReport)) {
      if (!sumErr) memcpy(prev_padReport, padReport, sizeof(prev_padReport));
      Serial2.readBytes((char *)padReport, sizeof(padReport));
      uint8_t sum = 0;
      for (size_t i = 0; i < sizeof(padReport); i++) {
        sum += padReport[i];
        /*char buffer[5];
        sprintf(buffer, "%02X ", padReport[i]);
        Serial.print(buffer);*/
      }
      if (sum != 0xff) {
        sumErr = true;
        //Serial.print("Checksum ERROR! sum: ");
        //Serial.println(sum, HEX);
        return;
      }
      sumErr = false;
      //Serial.println(Serial2.available());
      break;
    }
  }

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
  }
}
