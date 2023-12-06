# ESP32_BTAudio_GID
Simple program writing and reading data from MS-CAN bus in Opel vehicles such as Astra H or Zafira B in addition to acting as a bluetooth audio receiver. Still in active development.
Features:
- A2DP audio sink, data is output either over I2S to an external DAC, such as PCM5102 or internal DAC (poor quality and noise, mainly for debugging)
- reads steering wheel button presses, which allows for control of connected audio source (play/pause, previous/next track)
- receives bluetooth metadata from connected audio source and prints it to vehicle's center console display (tested on BID and GID)
- simulates button presses in Climate Control menu to allow for one-press enabling/disabling of AC compressor

Required hardware: ESP32 board (of the classic flavor, A2DP doesn't work on ESP32-C3), PCM5102A DAC module, any CAN transceiver module (in my case MCP2551).
Required connections:
- 5V to: ESP32 VIN pin, MCP2551 VCC pin, PCM5102 VIN pin;
- CAN bus: D4 to CAN_RX, D5 to CAN_TX, CANL and CANH wired up to the vehicle's MS-CAN (accessible by either OBD-II diagnostic port, radio, display, electronic climate control);
- I2S DAC: GND to SCK, D26 to BCK, D22 to DIN, D25 to LCK, D23 to XSMT;
- Configure jumpers on the back of the I2S DAC module: short 1-L, 2-L, 4-L, 3 NOT SHORTED.
  
Note that this should be soldered directly in the radio unit as the OBD-II port only provides unswitched 12V. Powering it from a 5V car charger also works.
Do not connect headphones to the DAC module, its output is supposed to only be connected to amplifier input - in case of this project either the AUX socket of radio's internal AUX input.

Uses excellent library for conversion of UTF-8 chars to UTF-16 chars, "utfcpp" by nemtrif: https://github.com/nemtrif/utfcpp
Depends on Arduino ESP32-A2DP library by pschatzmann: https://github.com/pschatzmann/ESP32-A2DP
Reverse engineering of the vehicles various messages was done by JJToB: https://github.com/JJToB/Car-CAN-Message-DB

This project comes with absolutely no warranty of any kind, I'm not responsible for your car going up in flames.
