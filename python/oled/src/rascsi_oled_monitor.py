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

import argparse
import sys
from time import sleep
from collections import deque
from unidecode import unidecode
from board import I2C
from adafruit_ssd1306 import SSD1306_I2C
from PIL import Image, ImageDraw, ImageFont
from interrupt_handler import GracefulInterruptHandler
from rascsi.ractl_cmds import RaCtlCmds
from rascsi.socket_cmds import SocketCmds
from rascsi.sys_cmds import SysCmds

parser = argparse.ArgumentParser(description="RaSCSI OLED Monitor script")
parser.add_argument(
    "--rotation",
    type=int,
    choices=[0, 180],
    default=180,
    action="store",
    help="The rotation of the screen buffer in degrees",
    )
parser.add_argument(
    "--height",
    type=int,
    choices=[32, 64],
    default=32,
    action="store",
    help="The pixel height of the screen buffer",
    )
parser.add_argument(
    "--refresh_interval",
    type=int,
    choices=range(0, 10000),
    default=1000,
    action="store",
    help="Interval in ms between each screen refresh",
    )
parser.add_argument(
    "--password",
    type=str,
    default="",
    action="store",
    help="Token password string for authenticating with the backend",
    )
parser.add_argument(
    "--host",
    type=str,
    default="localhost",
    action="store",
    help="Backend hostname. Default: localhost",
)
parser.add_argument(
    "--port",
    type=str,
    default=6868,
    action="store",
    help="Backend port number. Default: 6868",
)
args = parser.parse_args()

if args.rotation == 0:
    ROTATION = 0
elif args.rotation == 180:
    ROTATION = 2

if args.height == 64:
    HEIGHT = 64
    LINES = 8
elif args.height == 32:
    HEIGHT = 32
    LINES = 4

TOKEN = args.password

sock_cmd = SocketCmds(host=args.host, port=args.port)
ractl_cmd = RaCtlCmds(sock_cmd=sock_cmd, token=TOKEN)
sys_cmd = SysCmds()

WIDTH = 128
BORDER = 5

# Define the Reset Pin
OLED_RESET = None

# init i2c
I2C = I2C()

# 128x32 display with hardware I2C:
OLED = SSD1306_I2C(WIDTH, HEIGHT, I2C, addr=0x3C, reset=OLED_RESET)
OLED.rotation = ROTATION

print("Running with the following display:")
print(OLED)
print()
print("Will update the OLED display every " + str(args.refresh_interval) + "ms (approximately)")

# Show a startup splash bitmap image before starting the main loop
# Convert the image to mode '1' for 1-bit color (monochrome)
# Make sure the splash bitmap image is in the same dir as this script
IMAGE_STOP = Image.open(f"resources/splash_stop_{HEIGHT}.bmp").convert("1")
IMAGE = Image.open(f"resources/splash_start_{HEIGHT}.bmp").convert("1")
OLED.image(IMAGE)
OLED.show()

# Draw some shapes.
# First define some constants to allow easy resizing of shapes.
# Depending on the font used, you may want to change the value of PADDING
PADDING = 0
TOP = PADDING
BOTTOM = HEIGHT - PADDING
# Move left to right keeping track of the current x position for drawing shapes.
X_POS = 0

# Font size in pixels. Not all TTF fonts have bitmap representations for all sizes.
FONT_SIZE = 8
# Vertical spacing between each line of text. Adjust in accordance with font size.
# Depending on the design of the font glyphs, this may be larger than FONT_SIZE.
LINE_SPACING = 8

# Load a TTF font for rendering glyphs on the screen.
# Make sure the .ttf font file is in the same directory as the python script!
# When using other fonts, you may need to adjust PADDING, FONT_SIZE,
# LINE_SPACING, and LINES.
# Some other nice fonts to try: http://www.dafont.com/bitmap.php
FONT = ImageFont.truetype('resources/type_writer.ttf', FONT_SIZE)

REMOVABLE_DEVICE_TYPES = ractl_cmd.get_removable_device_types()
PERIPHERAL_DEVICE_TYPES = ractl_cmd.get_peripheral_device_types()

# After how many screen updates the network data should be refreshed
NETWORK_REFRESH_TICKS = 10
network_refresh_counter = NETWORK_REFRESH_TICKS

ip_addr, hostname = sys_cmd.get_ip_and_host()

# Keep the pretty splash up on screen for a number of seconds
sleep(2)

# Get drawing object to draw on image.
DRAW = ImageDraw.Draw(IMAGE)

# Draw a black filled box to clear the image.
DRAW.rectangle((0, 0, WIDTH, HEIGHT), outline=0, fill=0)

def formatted_output():
    """
    Formats the strings to be displayed on the Screen
    Returns a (list) of (str) output
    """
    device_list = ractl_cmd.list_devices()["device_list"]
    output = []

    if not TOKEN and not ractl_cmd.is_token_auth()["status"]:
        output += ["Permission denied!"]
    elif device_list:
        has_luns = False
        for line in device_list:
            # If any device is using a LUN > 0 then display LUN for all devices
            if int(line["unit"]) > 0:
                has_luns = True
                break
        for device in device_list:
            line = [str(device["id"])]
            if has_luns:
                line += [str(device["unit"])]
            line += [device["device_type"][2:4]]
            if device["file"]:
                # Transliterate non-Latin characters in the file name
                line += [unidecode(device["file"])]
            elif device["device_type"] in REMOVABLE_DEVICE_TYPES:
                line += ["[No Media]"]
            # Print only the Vendor/Product info if it's not generic RaSCSI
            if device["vendor"] not in "RaSCSI":
                line += [device["vendor"], device["product"]]
            output += [" ".join(line)]
    else:
        output += ["No device attached!"]

    if ip_addr:
        output += [f"{hostname} ({ip_addr})"]
    else:
        output += ["No IP address assigned", "Check network connection"]
    return output

def shutdown():
    """
    Display the shutdown splash, then blank the screen after a sleep
    Finally shuts down the script
    """
    OLED.image(IMAGE_STOP)
    OLED.show()
    OLED.fill(0)
    sleep(700/1000)
    OLED.show()
    sys.exit("Shutting down the OLED display...")


with GracefulInterruptHandler() as handler:
    """
    The main display loop inside an interrupt handler
    """
    while True:
        # The reference snapshot of attached devices that will be compared against each cycle
        # to identify changes in the backend
        ref_snapshot = formatted_output()
        # The snapshot updated each cycle that will compared with ref_snapshot
        snapshot = ref_snapshot
        # The active output that will be displayed on the screen
        active_output = deque(snapshot)

        while snapshot == ref_snapshot:
            # When the counter reaches 0, refresh the ip address and hostname data
            if not network_refresh_counter:
                ip_addr, hostname = sys_cmd.get_ip_and_host()
                network_refresh_counter = NETWORK_REFRESH_TICKS
            else:
                network_refresh_counter -= 1

            # Draw a black filled box to clear the image.
            DRAW.rectangle((0, 0, WIDTH, HEIGHT), outline=0, fill=0)
            Y_POS = TOP
            for output_line in active_output:
                DRAW.text((X_POS, Y_POS), output_line, font=FONT, fill=255)
                Y_POS += LINE_SPACING

            # Shift the index of the array by one to get a scrolling effect
            if len(active_output) > LINES:
                active_output.rotate(-1)

            # Display image.
            OLED.image(IMAGE)
            OLED.show()
            sleep(args.refresh_interval/1000)

            snapshot = formatted_output()

            if handler.interrupted:
                shutdown()
