# DB-15_to_JVS
Adaptation of TeensyJVS for use with SNK/SuperGun style fightsticks.

This repository contains the sourcecode for the DB-15_to_JVS boards I designed.  The intention is to allow users to modify code as needed for special projects like console controllers or analog inputs.  The board and code have currently only been tested with the Teensy3.5, but should also work with the Teensy3.6 and Teensy4.1.

I would also like to add that charcole's work is awesome.

This code has only been tested on the following hardware:
- Naomi 1 (jp and US BIOS)

And the following games:
- Dead or Alive 2 (US BIOS)
- Moero! Justice Gakuen (US BIOS)
- Marvel vs Capcom 2 (US BIOS)
- Melty Blood Actress Again (jp BIOS)
- Radrgy Noa (jp BIOS)
- Power Stone 2 (US BIOS)

All work in 2-player mode.  Power Stone 2 verified in 4-player mode.

# TeensyJVS
Jamma Video Standard (JVS) IO board implemented using a Teensy 3.1

Quick and dirty implementation of the JVS IO standard (http://superusr.free.fr/arcade/JVS/JVST_VER3.pdf) for adding controls to a SEGA NAOMI (and other JVS compatible boards) without having to shell out for an official IO board.

Intended to be compiled for the Teensy 3.1 in the Arduino IDE. The comments at the top of the file have the schematic draw in ASCII art.

Video of it in action: https://www.youtube.com/watch?v=nQ9IQh23H0I

These links were help when trying to decifer the spec...
- https://github.com/TheOnlyJoey/openjvs/wiki/Protocol
- http://wiki.pcbotaku.com/wiki/JVS_I/O
