#!/usr/bin/env python3

"""
 RaSCSI Updates:
   Updates to output rascsi status to an OLED display
   Copyright (C) 2020 Tony Kuker
   Author: Tony Kuker
 Developed for:
   https://www.makerfocus.com/collections/oled/products/2pcs-i2c-oled-display-module-0-91-inch-i2c-ssd1306-oled-display-module-1

 All other code:
    Copyright (c) 2017 Adafruit Industries
    Author: Tony DiCola & James DeVito

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
"""
from time import sleep
from sys import argv
from collections import deque
from board import I2C
from adafruit_ssd1306 import SSD1306_I2C
from PIL import Image, ImageDraw, ImageFont
from interrupt_handler import GracefulInterruptHandler
from pi_cmds import get_ip_and_host
from ractl_cmds import device_list

# Read positional arguments; expecting exactly two, or none
# Arg 1 is the rotation in degrees, arg 2 is the screen height in pixels
# Valid values are 0/180 for ROTATION, 32/64 for HEIGHT
if len(argv) == 3:
    if int(argv[1]) == 0:
        ROTATION = 0
    else:
        # 2 means 180 degrees
        ROTATION = 2
    if int(argv[2]) > 32:
        HEIGHT = 64
        LINES = 8
    else:
        HEIGHT = 32
        LINES = 3
else:
    # Default settings
    ROTATION = 2
    HEIGHT = 32
    LINES = 4
    print("No valid parameters detected; defaulting to 32 px height, 180 degrees rotation.")

WIDTH = 128
BORDER = 5

# How long to delay between each update
DELAY_TIME_MS = 1000

# Define the Reset Pin
OLED_RESET = None

# init i2c
I2C = I2C()

# 128x32 display with hardware I2C:
OLED = SSD1306_I2C(WIDTH, HEIGHT, I2C, addr=0x3C, reset=OLED_RESET)

print("Running with the following display:")
print(OLED)
print()
print("Will update the OLED display every " + str(DELAY_TIME_MS) + "ms (approximately)")

# Clear display.
OLED.rotation = ROTATION
OLED.fill(0)
OLED.show()

# Create blank image for drawing.
# Make sure to create image with mode '1' for 1-bit color.
IMAGE = Image.new("1", (OLED.width, OLED.height))

# Get drawing object to draw on image.
DRAW = ImageDraw.Draw(IMAGE)

# Draw a black filled box to clear the image.
DRAW.rectangle((0, 0, WIDTH, HEIGHT), outline=0, fill=0)

# Draw some shapes.
# First define some constants to allow easy resizing of shapes.
# Depending on the font used, you may want to change the value of PADDING
PADDING = 0
TOP = PADDING
BOTTOM = HEIGHT - PADDING
# Move left to right keeping track of the current x position for drawing shapes.
X_POS = 0

# Alternatively load a TTF font.  Make sure the .ttf font file
# is in the same directory as the python script!
# When using other fonts, you may need to adjust padding, font size, and line spacing.
# Some other nice fonts to try: http://www.dafont.com/bitmap.php
FONT = ImageFont.truetype('type_writer.ttf', 8)

IP_ADDR, HOSTNAME = get_ip_and_host()


def formatted_output():
    """
    Formats the strings to be displayed on the Screen
    Returns a (list) of (str) output
    """
    rascsi_list = device_list()
    output = []

    if rascsi_list:
        for line in rascsi_list:
            if line["device_type"] in ("SCCD", "SCRM", "SCMO"):
                # Print image file name only when there is an image attached
                if line["file"]:
                    output.append(f"{line['id']} {line['device_type'][2:4]} "
                                  f"{line['file']} {line['status']}")
                else:
                    output.append(f"{line['id']} {line['device_type'][2:4]} {line['status']}")
            # Special handling for the DaynaPort device
            elif line["device_type"] == "SCDP":
                output.append(f"{line['id']} {line['device_type'][2:4]} {line['vendor']} "
                              f"{line['product']}")
            # Special handling for the Host Bridge device
            elif line["device_type"] == "SCBR":
                output.append(f"{line['id']} {line['device_type'][2:4]} {line['product']}")
            # Print only the Vendor/Product info if it's not generic RaSCSI
            elif line["vendor"] not in "RaSCSI":
                output.append(f"{line['id']} {line['device_type'][2:4]} {line['file']} "
                              f"{line['vendor']} {line['product']} {line['status']}")
            else:
                output.append(f"{line['id']} {line['device_type'][2:4]} {line['file']} "
                              f"{line['status']}")
    else:
        output.append("No image mounted!")

    output.append(f"IP {IP_ADDR} - {HOSTNAME}")
    return output


def start_splash():
    """
    Displays a splash screen for the startup sequence
    Make sure the splash bitmap image is in the same dir as this script
    """
    splash = Image.open(f"splash_start_{HEIGHT}.bmp").convert("1")
    DRAW.bitmap((0, 0), splash)
    OLED.image(splash)
    OLED.show()
    sleep(6)

def stop_splash():
    """
    Displays a splash screen for the shutdown sequence
    Make sure the splash bitmap image is in the same dir as this script
    """
    DRAW.rectangle((0, 0, WIDTH, HEIGHT), outline=0, fill=0)
    splash = Image.open(f"splash_stop_{HEIGHT}.bmp").convert("1")
    DRAW.bitmap((0, 0), splash)
    OLED.image(splash)
    OLED.show()

# Show a startup splash bitmap image before starting the main loop
start_splash()

with GracefulInterruptHandler() as handler:
    """
    The main loop of displaying attached device info, and other info
    """
    while True:

        # The reference snapshot of attached devices that will be compared against each cycle
        # to identify changes in RaSCSI backend
        ref_snapshot = formatted_output()
        # The snapshot updated each cycle that will compared with ref_snapshot
        snapshot = ref_snapshot
        # The active output that will be displayed on the screen
        active_output = deque(snapshot)

        while snapshot == ref_snapshot:
            # Draw a black filled box to clear the image.
            DRAW.rectangle((0, 0, WIDTH, HEIGHT), outline=0, fill=0)
            y_pos = TOP
            for output_line in active_output:
                DRAW.text((X_POS, y_pos), output_line, font=FONT, fill=255)
                y_pos += 8

            # Shift the index of the array by one to get a scrolling effect
            if len(active_output) > LINES:
                active_output.rotate(-1)

            # Display image.
            OLED.image(IMAGE)
            OLED.show()
            sleep(1000/DELAY_TIME_MS)

            snapshot = formatted_output()

            if handler.interrupted:
                # Catch interrupt signals and show a shutdown splash bitmap image
                stop_splash()
                exit("Shutting down the OLED display...")
