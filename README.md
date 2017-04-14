# Reflow oven controller
Manage the reflow profile of a DIY wireless reflow oven.
This is a Qt application which connects to DIY reflow oven via wireless serial port.
The host has a USB to serial port converter in which the HC11 radio module is connected to.
Reflow controller has ATMEGA 168 microcontroller which controls relays and measures the oven temperature.

Application has realtime temperature display. Each of four relays can be manually controlled as well.

Automatic run mode has preheat and reflow times and temperatures.

