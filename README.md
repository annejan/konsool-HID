# Tanmatsu HID Host Example

This project demonstrates a USB HID host implementation for the Tanmatsu platform.  
It can detect and handle the following USB HID device types:

- 🖱️ Mouse
- ⌨️ Keyboard
- 🎮 Game Controller

The screen keeps a panel per kind of input event the BSP delivers - keyboard, navigation, action and
scancode - and marks the one the latest event came from, so you can see at a glance what a device
does and does not report. The same events go to the serial monitor. Note that the console goes
quiet once the app switches the USB port to host mode.

## Features

- Detects device connection and disconnection events
- Parses HID report descriptors to determine device capabilities
- Supports real-time input reporting (e.g. mouse movement, button presses)
- Designed to support hot-plugging

## How it works

Reading the USB HID report descriptor is done by
[badgeteam/hid-host](https://github.com/badgeteam/esp32-component-hid-host), which works out where
the axes, the hat switch and the buttons sit in a report and knows the quirks of pads that need a
nudge before they say anything. What is left in this project is `main/badge_hid_drivers.c`, which
turns that into the named buttons and byte sized axes the rest of the firmware navigates by.

## Requirements

- ESP-IDF v6.0.2, which `make prepare` fetches into the project directory
- A Tanmatsu-compatible board with USB host support

## Tests

The mapping layer is plain C, so it builds and runs on a host against report descriptors captured
from real hardware. The hid-host component is fetched on the first run:

```
make -C test_native && test_native/test
```

Point it at a checkout of your own to test against unreleased component changes:

```
make -C test_native HID_HOST=/path/to/esp32-component-hid-host
```

## License

This project is based on the Tanmatsu PAX template and is released under the [MIT License](https://opensource.org/license/mit).  
You are free to use, modify, and distribute it — with proper attribution.
