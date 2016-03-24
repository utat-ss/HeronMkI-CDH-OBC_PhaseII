# CDH-OBC_PhaseII

This code is intended to be used for the main computer of HERON, UTAT Space Systems' first cubesat designed for the Canadian Satellite Design Challenge.

The main computer is a board we have designed ourselves using the ATSAM3X8E and based on the Arduino DUE.

We've been using Atmel Studio 6.2 as a development environment, and using a J-Link (Edu Version) to program/debug the ATSAM.

This program consists of several tasks charged which managing specific parts of the satellite as well as the drivers and APIs underneath that allow everything to run smoothly.

We currently have 3 subsystem computers in our system which act as "dummy microcontrollers", their management is controlled by tasks: coms.c, eps.c, payload.c.

We'e using FreeRTOS as the underlying operating system and ESA's Packet Utilization Standard for communicating with our ground station.

Feel free to use our software, just be sure to give us proper credit!
(And buy us a beer if you meet us :) )
