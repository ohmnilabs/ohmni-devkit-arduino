# Ohmni Devkit - Arduino Module Firmware

This barebones Arduino project is the starting point for you to add 
your own electronics and hardware to Ohmni.  It is designed for an
Arduino Nano but can be easily adapted to any Arduino board.

Once flashed, you can simply attach the arduino to Ohmni (NOTE: power
down the robot BEFORE plugging anything in or out) using the
FlexAdapter included in the dev kit.  By default, this firmware only
supports simple RAM read/write commands to turn on and off the LED
on board the Arduino Nano.  But you can easily modify the code to add
your own commands that control a transistor, do stepper motor stuff, 
talk to custom sensors, etc!

Once this are plugged in and you power on Ohmni, you can use bot_shell.js
or Ohmni API to send command packets to the arduino.  For this sample
firmware, the SID of the module is set to 0x50 and it supports the
led and led_status commands.  In bot_shell, you can do:

led 0x50 0 - turn off the Nano's LED
led 0x50 1 - turn on the Nano's LED

You can read the state of the LED using:

led_status 0x50

If you want to attach multiple Arduinos to Ohmni, simply add compile
the firmware with a different SID for each one so that you can uniquely
address each one.

----

Hope this gets you started quickly! Let us know if you have any
feedback or questions.

Team OhmniLabs


