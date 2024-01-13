#include <Arduino.h>
#include <BleGamepad.h>
#include <ESP32SPISlave.h>

#define CHECK_BIT(var, pos) ((var) & (1 << (pos)))
#define BUTTON(var, pos, btn) CHECK_BIT(var, pos) ? bleGamepad.press(btn) : bleGamepad.release(btn)

BleGamepad bleGamepad("jojo", "dio");
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
  bleGamepadConfig.setButtonCount(16);
  bleGamepadConfig.setHatSwitchCount(1);
  bleGamepadConfig.setWhichAxes(true, true, true, false, false, true, false, false);
  bleGamepadConfig.setWhichSimulationControls(false, false, true, true, false);
  bleGamepadConfig.setSimulationMin(0);
  bleGamepadConfig.setSimulationMax(255);
  bleGamepadConfig.setVid(0xe502);
  bleGamepadConfig.setPid(0xbbab);
  //bleGamepadConfig.setAxesMin(0);
  bleGamepadConfig.setAxesMin(-32767);
  bleGamepadConfig.setAxesMax(32767);
  bleGamepad.begin(&bleGamepadConfig);
}

uint8_t sum = 0;
uint8_t padReport[13] = { 0 };
const long deadZoneLow = -3072;
const long deadZoneHigh = 3072;

void loop() {
  slave.wait(padReport, sizeof(padReport));
  while (slave.available()) {
    sum = 0;
    for (size_t i = 0; i < sizeof(padReport); i++)
      sum += padReport[i];
    if (sum != 0xff)
      return;

    // BLE GAMEPAD
    if (bleGamepad.isConnected()) {
      // DPAD
      if (CHECK_BIT(padReport[0], 0)) {
        bleGamepad.setHat1(DPAD_UP);
        if (CHECK_BIT(padReport[0], 2))
          bleGamepad.setHat1(DPAD_UP_LEFT);
        if (CHECK_BIT(padReport[0], 3))
          bleGamepad.setHat1(DPAD_UP_RIGHT);
      } else if (CHECK_BIT(padReport[0], 1)) {
        bleGamepad.setHat1(DPAD_DOWN);
        if (CHECK_BIT(padReport[0], 2))
          bleGamepad.setHat1(DPAD_DOWN_LEFT);
        if (CHECK_BIT(padReport[0], 3))
          bleGamepad.setHat1(DPAD_DOWN_RIGHT);
      } else if (CHECK_BIT(padReport[0], 2)) {
        bleGamepad.setHat1(DPAD_LEFT);
      } else if (CHECK_BIT(padReport[0], 3)) {
        bleGamepad.setHat1(DPAD_RIGHT);
      } else {
        bleGamepad.setHat1(DPAD_CENTERED);
      }
      // BUTTONS
      // A
      BUTTON(padReport[1], 4, 1);
      // B
      BUTTON(padReport[1], 5, 2);
      // X
      BUTTON(padReport[1], 6, 4);
      // Y
      BUTTON(padReport[1], 7, 5);
      // LB
      BUTTON(padReport[1], 0, 7);
      // RB
      BUTTON(padReport[1], 1, 8);
      // BACK
      BUTTON(padReport[0], 5, 11);
      // START
      BUTTON(padReport[0], 4, 12);
      // XBOX
      BUTTON(padReport[1], 2, 13);
      // L3
      BUTTON(padReport[0], 6, 14);
      // R3
      BUTTON(padReport[0], 7, 15);
      // TRIGGERS
      long x, y;
      x = (long)(*((uint8_t *)(padReport + 2)));
      y = (long)(*((uint8_t *)(padReport + 3)));
      bleGamepad.setBrake(x);
      bleGamepad.setAccelerator(y);
      // LEFT THUMB
      x = (long)(*((int16_t *)(padReport + 4)));
      y = (long)(*((int16_t *)(padReport + 6)));
      if (x <= deadZoneLow)
        x = map(x, deadZoneLow, -32768, 0, -32767);
      else if (x >= deadZoneHigh)
        x = map(x, deadZoneHigh, 32767, 0, 32767);
      else
        x = 0;
      if (y <= deadZoneLow)
        y = map(y, deadZoneLow, -32768, 0, -32767);
      else if (y >= deadZoneHigh)
        y = map(y, deadZoneHigh, 32767, 0, 32767);
      else
        y = 0;
      bleGamepad.setLeftThumb(x, -y);
      // RIGHT THUMB
      x = (long)(*((int16_t *)(padReport + 8)));
      y = (long)(*((int16_t *)(padReport + 10)));
      if (x <= deadZoneLow)
        x = map(x, deadZoneLow, -32768, 0, -32767);
      else if (x >= deadZoneHigh)
        x = map(x, deadZoneHigh, 32767, 0, 32767);
      else
        x = 0;
      if (y <= deadZoneLow)
        y = map(y, deadZoneLow, -32768, 0, -32767);
      else if (y >= deadZoneHigh)
        y = map(y, deadZoneHigh, 32767, 0, 32767);
      else
        y = 0;
      bleGamepad.setRightThumb(x, -y);
      // SEND REPORT
      bleGamepad.sendReport();
    }
    slave.pop();
  }
}
