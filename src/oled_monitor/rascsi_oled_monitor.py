#!/usr/bin/env python3
#
# RaSCSI Updates:
#   Updates to output rascsi status to an OLED display
#   Copyright (C) 2020 Tony Kuker
#   Author: Tony Kuker
# Developed for:
#   https://www.makerfocus.com/collections/oled/products/2pcs-i2c-oled-display-module-0-91-inch-i2c-ssd1306-oled-display-module-1
#
# All other code: 
#    Copyright (c) 2017 Adafruit Industries
#    Author: Tony DiCola & James DeVito
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
from time import sleep
from sys import argv, exit
from board import I2C
from adafruit_ssd1306 import SSD1306_I2C
from PIL import Image, ImageDraw, ImageFont
from os import path, getcwd
from collections import deque
import rascsi_interface_pb2 as proto
from interrupt_handler import GracefulInterruptHandler
from socket_cmds import send_pb_command

WIDTH = 128
HEIGHT = 32  # Change to 64 if needed
BORDER = 5

# How long to delay between each update
delay_time_ms = 1000

# Define the Reset Pin
oled_reset = None 

# init i2c
i2c = I2C()

# 128x32 display with hardware I2C:
oled = SSD1306_I2C(WIDTH, HEIGHT, i2c, addr=0x3C, reset=oled_reset)

print ("Running with the following display:")
print (oled)
print ()
print ("Will update the OLED display every " + str(delay_time_ms) + "ms (approximately)")

# Attempt to read the first argument to the script; fall back to 2 (180 degrees)
if len(argv) > 1:
    if str(argv[1]) == "0":
        rotation = 0
    elif str(argv[1]) == "180":
        rotation = 2
    else:
        exit("Only 0 and 180 are valid arguments for screen rotation.")
else:
    print("Defaulting to 180 degrees screen rotation.")
    rotation = 2

# Clear display.
oled.rotation = rotation
oled.fill(0)
oled.show()

# Create blank image for drawing.
# Make sure to create image with mode '1' for 1-bit color.
image = Image.new("1", (oled.width, oled.height))

# Get drawing object to draw on image.
draw = ImageDraw.Draw(image)

# Draw a black filled box to clear the image.
draw.rectangle((0,0,WIDTH,HEIGHT), outline=0, fill=0)

# Draw some shapes.
# First define some constants to allow easy resizing of shapes.
padding = 0
top = padding
bottom = HEIGHT-padding
# Move left to right keeping track of the current x position for drawing shapes.
x = 0

# Load default font.
#font = ImageFont.load_default()

# Alternatively load a TTF font.  Make sure the .ttf font file is in the same directory as the python script!
# When using other fonds, you may need to adjust padding, font size, and line spacing
# Some other nice fonts to try: http://www.dafont.com/bitmap.php
font = ImageFont.truetype('type_writer.ttf', 8)


def device_list():
    """
    Sends a DEVICES_INFO command to the server.
    Returns a list of dicts with info on all attached devices.
    """
    command = proto.PbCommand()
    command.operation = proto.PbOperation.DEVICES_INFO
    data = send_pb_command(command.SerializeToString())
    result = proto.PbResult()
    result.ParseFromString(data)

    device_list = []
    n = 0

    while n < len(result.devices_info.devices):
        did = result.devices_info.devices[n].id
        dtype = proto.PbDeviceType.Name(result.devices_info.devices[n].type) 
        dstat = result.devices_info.devices[n].status
        dprop = result.devices_info.devices[n].properties

        # Building the status string
        dstat_msg = []
        if dstat.protected == True and dprop.protectable == True:
            dstat_msg.append("Write-Protected")
        if dstat.removed == True and dprop.removable == True:
            dstat_msg.append("No Media")
        if dstat.locked == True and dprop.lockable == True:
            dstat_msg.append("Locked")

        dfile = path.basename(result.devices_info.devices[n].file.name)
        dven = result.devices_info.devices[n].vendor
        dprod = result.devices_info.devices[n].product

        device_list.append(
                {
                    "id": did,
                    "device_type": dtype,
                    "status": ", ".join(dstat_msg),
                    "file": dfile,
                    "vendor": dven,
                    "product": dprod,
                }
            )
        n += 1

    return device_list

def get_ip_and_host():
    from socket import socket, gethostname, AF_INET, SOCK_DGRAM
    host = gethostname()
    sock = socket(AF_INET, SOCK_DGRAM)
    try:
        # mock ip address; doesn't have to be reachable
        sock.connect(('10.255.255.255', 1))
        ip_addr = sock.getsockname()[0]
    except Exception:
        ip_addr = '127.0.0.1'
    finally:
        sock.close()
    return ip_addr, host


def formatted_output():
    """
    Formats the strings to be displayed on the Screen
    Returns a list of str output
    """
    rascsi_list = device_list()
    output = []

    if len(rascsi_list):
        for line in rascsi_list:
            if line["device_type"] in ("SCCD", "SCRM", "SCMO"):
                if len(line["file"]):
                    output.append(f"{line['id']} {line['device_type'][2:4]} {line['file']} {line['status']}")
                else:
                    output.append(f"{line['id']} {line['device_type'][2:4]} {line['status']}")
            elif line["device_type"] in ("SCDP"):
                output.append(f"{line['id']} {line['device_type'][2:4]} {line['vendor']} {line['product']}")
            elif line["device_type"] in ("SCBR"):
                output.append(f"{line['id']} {line['device_type'][2:4]} {line['product']}")
            else:
                output.append(f"{line['id']} {line['device_type'][2:4]} {line['file']} {line['vendor']} {line['product']} {line['status']}")
    else:
        output.append("No image mounted!")

    output.append(f"IP {ip} - {host}")
    return output

def start_splash():
    splash = Image.open(f"{cwd}/splash_start.bmp").convert("1")
    draw.bitmap((0, 0), splash)
    oled.image(splash)
    oled.show()
    sleep(6)

def stop_splash():
    draw.rectangle((0,0,WIDTH,HEIGHT), outline=0, fill=0)
    splash = Image.open(f"{cwd}/splash_stop.bmp").convert("1")
    draw.bitmap((0, 0), splash)
    oled.image(splash)
    oled.show()

cwd = getcwd()

start_splash()

ip, host = get_ip_and_host()

with GracefulInterruptHandler() as handler:
    while True:

        ref_snapshot = formatted_output()
        snapshot = ref_snapshot
        output = deque(snapshot)

        while snapshot == ref_snapshot:
            # Draw a black filled box to clear the image.
            draw.rectangle((0,0,WIDTH,HEIGHT), outline=0, fill=0)
            y_pos = top
            for line in output:
                draw.text((x, y_pos), line, font=font, fill=255)
                y_pos += 8

            # Shift the index of the array by one to get a scrolling effect
            if len(output) > 5:
                output.rotate(-1)

            # Display image.
            oled.image(image)
            oled.show()
            sleep(1000/delay_time_ms)

            snapshot = formatted_output()

            if handler.interrupted:
                stop_splash()
                exit("Shutting down the OLED display...")
