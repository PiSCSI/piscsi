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
import time
import os
import sys
import datetime
import board
import busio
import adafruit_ssd1306
from PIL import Image, ImageDraw, ImageFont
import subprocess
import rascsi_interface_pb2 as proto

WIDTH = 128
HEIGHT = 32  # Change to 64 if needed
BORDER = 5

# How long to delay between each update
delay_time_ms = 250

# Define the Reset Pin
oled_reset = None 

# init i2c
i2c = board.I2C()

# 128x32 display with hardware I2C:
oled = adafruit_ssd1306.SSD1306_I2C(WIDTH, HEIGHT, i2c, addr=0x3C, reset=oled_reset)

print ("Running with the following display:")
print (oled)
print ()
print ("Will update the OLED display every " + str(delay_time_ms) + "ms (approximately)")

# Clear display.
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
        dun = result.devices_info.devices[n].unit
        dtype = proto.PbDeviceType.Name(result.devices_info.devices[n].type) 
        dstat = result.devices_info.devices[n].status
        dprop = result.devices_info.devices[n].properties

        # Building the status string
        dstat_msg = []
        if dprop.read_only == True:
            dstat_msg.append("Read-Only")
        if dstat.protected == True and dprop.protectable == True:
            dstat_msg.append("Write-Protected")
        if dstat.removed == True and dprop.removable == True:
            dstat_msg.append("No Media")
        if dstat.locked == True and dprop.lockable == True:
            dstat_msg.append("Locked")

        from os import path
        dpath = result.devices_info.devices[n].file.name
        dfile = path.basename(dpath)
        dparam = result.devices_info.devices[n].params
        dven = result.devices_info.devices[n].vendor
        dprod = result.devices_info.devices[n].product
        drev = result.devices_info.devices[n].revision
        dblock = result.devices_info.devices[n].block_size
        dsize = int(result.devices_info.devices[n].block_count) * int(dblock)

        device_list.append(
                {
                    "id": did,
                    "un": dun,
                    "device_type": dtype,
                    "status": ", ".join(dstat_msg),
                    "path": dpath,
                    "file": dfile,
                    "params": dparam,
                    "vendor": dven,
                    "product": dprod,
                    "revision": drev,
                    "block_size": dblock,
                    "size": dsize,
                }
            )
        n += 1

    return device_list

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

    import socket
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
    from struct import pack, unpack

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

while True:

    # Draw a black filled box to clear the image.
    draw.rectangle((0,0,WIDTH,HEIGHT), outline=0, fill=0)
    
    rascsi_list = device_list()

    y_pos = top
    if len(rascsi_list):
        for line in rascsi_list:
            output = f"{line['id']} {line['device_type'][2:4]} {line['file']}"
            draw.text((x, y_pos), output, font=font, fill=255)
            y_pos += 8
    else:
        output = "No image mounted!"
        draw.text((x, y_pos), output, font=font, fill=255)
        y_pos += 8

    # If there is still room on the screen, we'll display the time. If there's not room it will just be clipped
    draw.text((x, y_pos), datetime.datetime.now().strftime("%d/%m/%Y %H:%M:%S"), font=font, fill=255)

    # Display image.
    oled.image(image)
    oled.show()
    time.sleep(1/delay_time_ms)
