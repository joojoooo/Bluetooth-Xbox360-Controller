#include <Arduino.h>
#include <BleGamepad.h>
#include <ESP32SPISlave.h>

#define CHECK_BIT(var, pos) ((var) & (1 << (pos)))
#define numOfButtons 16
#define numOfHatSwitches 4

BleGamepad bleGamepad("jojo pad", "dio");
BleGamepadConfiguration bleGamepadConfig;

ESP32SPISlave slave;

void setup() {
  //Serial.begin(115200);
  //Serial.println("Starting Xbox 360 Wireless to BLE gamepad!");
  slave.setDataMode(SPI_MODE3);
  slave.begin(VSPI);
  // BLE GAMEPAD
  bleGamepadConfig.setAutoReport(false);
  bleGamepadConfig.setControllerType(CONTROLLER_TYPE_GAMEPAD);
  bleGamepadConfig.setButtonCount(numOfButtons);
  bleGamepadConfig.setHatSwitchCount(numOfHatSwitches);
  bleGamepadConfig.setVid(0xe502);
  bleGamepadConfig.setPid(0xbbab);
  bleGamepadConfig.setAxesMin(0x8000);
  //bleGamepadConfig.setAxesMin(0x0000);
  bleGamepadConfig.setAxesMax(0x7FFF);
  bleGamepad.begin(&bleGamepadConfig);  // Simulation controls, special buttons and hats 2/3/4 are disabled by default
}

uint8_t sum = 0;
uint8_t padReport[13] = { 0 };
uint8_t prev_padReport[12] = { 0 };

void loop() {
  slave.wait(padReport, sizeof(padReport));
  while (slave.available()) {
    for (size_t i = 0; i < sizeof(padReport); i++)
      sum += padReport[i];
    if (sum != 0xff)
      return;
    sum = 0;

    // BLE GAMEPAD
    if (bleGamepad.isConnected()) {
      if (CHECK_BIT(padReport[1], 4) && !CHECK_BIT(prev_padReport[1], 4))
        bleGamepad.press(1);  // A
      if (!CHECK_BIT(padReport[1], 4) && CHECK_BIT(prev_padReport[1], 4))
        bleGamepad.release(1);
      if (CHECK_BIT(padReport[1], 6) && !CHECK_BIT(prev_padReport[1], 6))
        bleGamepad.press(4);  // X
      if (!CHECK_BIT(padReport[1], 6) && CHECK_BIT(prev_padReport[1], 6))
        bleGamepad.release(4);
      // AXES
      int16_t lx = *((int16_t *)(padReport + 4));
      int16_t ly = -(*((int16_t *)(padReport + 6)) + 1);
      int16_t rx = *((int16_t *)(padReport + 8));
      int16_t ry = -(*((int16_t *)(padReport + 10)) + 1);
      bleGamepad.setAxes(lx, ly, 0, 0, rx, ry);
      /*long lx = (long)*((int16_t *)(padReport + 4));
      long ly = (long)-(*((int16_t *)(padReport + 6)));
      long rx = (long)*((int16_t *)(padReport + 8));
      long ry = (long)-(*((int16_t *)(padReport + 10)));
      bleGamepad.setAxes(map(lx, -32768, 32767, 0, 32767), map(ly, -32768, 32767, 0, 32767), 0, 0, map(rx, -32768, 32767, 0, 32767), map(ry, -32768, 32767, 0, 32767));*/
      bleGamepad.sendReport();
    }

    memcpy(prev_padReport, padReport, sizeof(prev_padReport));
    slave.pop();
  }
}
