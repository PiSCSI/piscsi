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
import datetime

#import Adafruit_GPIO.SPI as SPI
import Adafruit_SSD1306

from PIL import Image
from PIL import ImageDraw
from PIL import ImageFont

import subprocess

# How long to delay between each update
delay_time_ms = 250

# Raspberry Pi pin configuration:
RST = None     # on the PiOLED this pin isnt used
# Note the following are only used with SPI:
DC = 23
SPI_PORT = 0
SPI_DEVICE = 0

# Beaglebone Black pin configuration:
# RST = 'P9_12'
# Note the following are only used with SPI:
# DC = 'P9_15'
# SPI_PORT = 1
# SPI_DEVICE = 0

# 128x32 display with hardware I2C:
disp = Adafruit_SSD1306.SSD1306_128_32(rst=RST)

# 128x64 display with hardware I2C:
# disp = Adafruit_SSD1306.SSD1306_128_64(rst=RST)

# Note you can change the I2C address by passing an i2c_address parameter like:
# disp = Adafruit_SSD1306.SSD1306_128_64(rst=RST, i2c_address=0x3C)

# Alternatively you can specify an explicit I2C bus number, for example
# with the 128x32 display you would use:
# disp = Adafruit_SSD1306.SSD1306_128_32(rst=RST, i2c_bus=2)

# 128x32 display with hardware SPI:
# disp = Adafruit_SSD1306.SSD1306_128_32(rst=RST, dc=DC, spi=SPI.SpiDev(SPI_PORT, SPI_DEVICE, max_speed_hz=8000000))

# 128x64 display with hardware SPI:
# disp = Adafruit_SSD1306.SSD1306_128_64(rst=RST, dc=DC, spi=SPI.SpiDev(SPI_PORT, SPI_DEVICE, max_speed_hz=8000000))

# Alternatively you can specify a software SPI implementation by providing
# digital GPIO pin numbers for all the required display pins.  For example
# on a Raspberry Pi with the 128x32 display you might use:
# disp = Adafruit_SSD1306.SSD1306_128_32(rst=RST, dc=DC, sclk=18, din=25, cs=22)

print "Running with the following display:"
print disp
print
print "Will update the OLED display every " + str(delay_time_ms) + "ms (approximately)"


# Initialize library.
disp.begin()

# Clear display.
disp.clear()
disp.display()

# Create blank image for drawing.
# Make sure to create image with mode '1' for 1-bit color.
width = disp.width
height = disp.height
image = Image.new('1', (width, height))

# Get drawing object to draw on image.
draw = ImageDraw.Draw(image)

# Draw a black filled box to clear the image.
draw.rectangle((0,0,width,height), outline=0, fill=0)

# Draw some shapes.
# First define some constants to allow easy resizing of shapes.
padding = -2
top = padding
bottom = height-padding
# Move left to right keeping track of the current x position for drawing shapes.
x = 0


# Load default font.
font = ImageFont.load_default()

# Alternatively load a TTF font.  Make sure the .ttf font file is in the same directory as the python script!
# Some other nice fonts to try: http://www.dafont.com/bitmap.php
# font = ImageFont.truetype('Minecraftia.ttf', 8)

while True:

    # Draw a black filled box to clear the image.
    draw.rectangle((0,0,width,height), outline=0, fill=0)
    
    cmd = "rasctl -l"
    rascsi_list = subprocess.check_output(cmd, shell=True)

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
				if line.startswith("|  0"):
					fields = line.split('|')
        				output = str.strip(fields[1]) + " " + str.strip(fields[3]) + " " + os.path.basename(str.strip(fields[4]))
				else:
					output = "No image mounted!"
				draw.text((x, y_pos), output, font=font, fill=255)
        			y_pos = y_pos + 8
    # If there is still room on the screen, we'll display the time. If there's not room it will just be clipped
    draw.text((x, y_pos), datetime.datetime.now().strftime("%d/%m/%Y %H:%M:%S"), font=font, fill=255)

    # Display image.
    disp.image(image)
    disp.display()
    time.sleep(1/delay_time_ms)
