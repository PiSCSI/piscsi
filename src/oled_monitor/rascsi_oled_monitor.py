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
import logging
from board import I2C
from adafruit_ssd1306 import SSD1306_I2C
from PIL import Image, ImageDraw, ImageFont
from os import path
from collections import deque
from struct import pack, unpack
import socket
import rascsi_interface_pb2 as proto

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
    logging.warning(argv[1])
    logging.warning(type(argv[1]))
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
padding = -2
top = padding
bottom = HEIGHT-padding
# Move left to right keeping track of the current x position for drawing shapes.
x = 0

# Load default font.
font = ImageFont.load_default()

# Alternatively load a TTF font.  Make sure the .ttf font file is in the same directory as the python script!
# Some other nice fonts to try: http://www.dafont.com/bitmap.php
# font = ImageFont.truetype('Minecraftia.ttf', 8)


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

def rascsi_version():
    """
    Sends a VERSION_INFO command to the server.
    Returns a str containing the version info.
    """
    command = proto.PbCommand()
    command.operation = proto.PbOperation.VERSION_INFO
    data = send_pb_command(command.SerializeToString())
    result = proto.PbResult()
    result.ParseFromString(data)
    version = str(result.version_info.major_version) + "." +\
              str(result.version_info.minor_version) + "." +\
              str(result.version_info.patch_version)
    return version

def send_pb_command(payload):
    """
    Takes a str containing a serialized protobuf as argument.
    Establishes a socket connection with RaSCSI.
    """
    # Host and port number where rascsi is listening for socket connections
    HOST = 'localhost'
    PORT = 6868

    counter = 0
    tries = 100
    error_msg = ""

    while counter < tries:
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
                s.connect((HOST, PORT))
                return send_over_socket(s, payload)
        except socket.error as error:
            counter += 1
            logging.warning("The RaSCSI service is not responding - attempt " + \
                    str(counter) + "/" + str(tries))
            error_msg = str(error)

    logging.error(error_msg)


def send_over_socket(s, payload):
    """
    Takes a socket object and str payload with serialized protobuf.
    Sends payload to RaSCSI over socket and captures the response.
    Tries to extract and interpret the protobuf header to get response size.
    Reads data from socket in 2048 bytes chunks until all data is received.
    """

    # Prepending a little endian 32bit header with the message size
    s.send(pack("<i", len(payload)))
    s.send(payload)

    # Receive the first 4 bytes to get the response header
    response = s.recv(4)
    if len(response) >= 4:
        # Extracting the response header to get the length of the response message
        response_length = unpack("<i", response)[0]
        # Reading in chunks, to handle a case where the response message is very large
        chunks = []
        bytes_recvd = 0
        while bytes_recvd < response_length:
            chunk = s.recv(min(response_length - bytes_recvd, 2048))
            if chunk == b'':
                logging.error("Read an empty chunk from the socket. \
                        Socket connection has dropped unexpectedly. \
                        RaSCSI may have has crashed.")
            chunks.append(chunk)
            bytes_recvd = bytes_recvd + len(chunk)
        response_message = b''.join(chunks)
        return response_message
    else:
        logging.error("The response from RaSCSI did not contain a protobuf header. \
                RaSCSI may have crashed.")


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

    output.append(f"~~RaSCSI v{version}~~")
    return output

version = rascsi_version()

while True:

    snapshot = deque(formatted_output())
    output = snapshot

    while len(snapshot) == len(output):
        # Draw a black filled box to clear the image.
        draw.rectangle((0,0,WIDTH,HEIGHT), outline=0, fill=0)
        y_pos = top
        for line in output:
            draw.text((x, y_pos), line, font=font, fill=255)
            y_pos += 8

        #if len(snapshot) > 4:
        output.rotate(-1)

        # Display image.
        oled.image(image)
        oled.show()
        sleep(1000/delay_time_ms)

        snapshot = deque(formatted_output())
        if len(snapshot) < 5:
            break
