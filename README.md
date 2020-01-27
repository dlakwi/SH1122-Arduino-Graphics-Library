# SH1122-Arduino-Graphics-Library
Arduino graphics library code providing point, line, and character plotting commands for use with an SH1122 256x64 I2C 128x64 OLED display

This code is based on the work of David Johnson-Davies at http://www.technoblogy.com/, in particular, the Tiny Graphics Library http://www.technoblogy.com/show?23OS.
The Tiny Graphics Library was developed for the SH1106 128x64 I2C OLED display.

The SH1106 is a monochrome display with 1 bit per pixel, arranged as 8 vertical pixels per byte.
The SH1122 is a 16 gray scale display with 4 bits per pixel, arranged as 2 horizontal pixels per byte.
Individual pixels on both I2C displays can be changed by using Read-Modify-Write operation as opposed to using MCU buffer RAM. Note that GDRAM cannot be reading using an SPI interface.
