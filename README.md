<h1 align="center">Bluetooth Xbox 360 Controller</h1>

<p align="center">
  <img src="img.png" height="500">
</p>

## :dart: About ##

Making the Xbox 360 controller (both wired and wireless versions) Bluetooth compatible. The Pico is used to read USB data from either the wired Xbox 360 controller (not tested) or the Xbox 360 Wireless Receiver for Windows. The ESP32 is used to make the Bluetooth controller.

## :white_check_mark: Requirements ##

- Xbox 360 Wireless Receiver or wired controller
- OTG Y splitter cable (not required but makes it easier to connect USB A and power).
- ESP32-WROOM-32 for bluetooth support
- Raspberry Pi Pico for USB Full Speed host support

A Pi Pico W is a better alternative, it has both bluetooth and USB host support but I don't have one, I just used what I had.

## :checkered_flag: Installation ##

- Compile and upload Gamepad.ino to the ESP32 using ArduinoIDE
- Load gamepad.uf2 in the Pico
- Connect the boards: SPI0 on the Pico to VSPI on the ESP32 (as in the picture)

I glued the Pico under the ESP32, it fits perfectly.