#!/usr/bin/python
#
# RaSCSI Updates:
#   Updates to output rascsi status to an OLED display
#   Copyright (C) 2020 Tony Kuker
#   Author: Tony Kuker
# Developed for: https://www.amazon.com/MakerFocus-Display-SSD1306-3-3V-5V-Arduino/dp/B079BN2J8V
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

while True:

    # Draw a black filled box to clear the image.
    draw.rectangle((0,0,WIDTH,HEIGHT), outline=0, fill=0)
    
    cmd = "rasctl -l"
    rascsi_list = subprocess.check_output(cmd, shell=True).decode(sys.stdout.encoding)

    y_pos = top
    # Draw all of the meaningful data to the 'image'
    #
    # Example rascstl -l output:
    #    pi@rascsi:~ $ rasctl -l
    #
    #    +----+----+------+-------------------------------------
    #    | ID | UN | TYPE | DEVICE STATUS
    #    +----+----+------+-------------------------------------
    #    |  1 |  0 | SCHD | /home/pi/harddisk.hda
    #    |  6 |  0 | SCCD | NO MEDIA
    #    +----+----+------+-------------------------------------
    #    pi@rascsi:~ $ 
    for line in rascsi_list.split('\n'):
        # Skip empty strings, divider lines and the header line...
        if (len(line) == 0) or line.startswith("+---") or line.startswith("| ID | UN"):
          continue
        else:
          if line.startswith("|  "):
            fields = line.split('|')
            output = str.strip(fields[1]) + " " + str.strip(fields[3]) + " " + os.path.basename(str.strip(fields[4]))
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
