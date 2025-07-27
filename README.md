# Tanmatsu HID Host Example

This project demonstrates a USB HID host implementation for the Tanmatsu platform.  
It can detect and handle the following USB HID device types:

- 🖱️ Mouse
- ⌨️ Keyboard
- 🎮 Game Controller

Basic state and input data is displayed via the serial monitor.

## Features

- Detects device connection and disconnection events
- Parses HID report descriptors to determine device capabilities
- Supports real-time input reporting (e.g. mouse movement, button presses)
- Designed to support hot-plugging

## Requirements

- ESP-IDF v5.x
- A Tanmatsu-compatible board with USB host support

## License

This project is based on the Tanmatsu PAX template and is released under the [MIT License](https://opensource.org/license/mit).  
You are free to use, modify, and distribute it — with proper attribution.
