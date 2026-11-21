1. Fix NRESET timing and add a pullup. Steps:
   1. Add 10k pullup to 3.3V for NRESET on J203 SWD 6-pin pads between
      pin 2 and 6.
   2. Add 10nF cap to GND on J203 SWD 6-pin pads between pin 2 and 1.

3. Brige 3.3V to VDD33 on USB3343. Short USB3343 pins 15 and 16 together.

4. Permanently enable 3.3V. Short U1503 load switch pin3 to pin1.

5. Add /RESET signal to USB3343 controlled by STM32 PC4 port pin.
   1. Cut trace from USB3343 pin 22 to its very nearby via.
   2. Run wire from USB3343 pin 22 to STM32 pin 32 PC4 port pin.

