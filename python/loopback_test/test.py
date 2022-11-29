#!/usr/bin/env python3
# BSD 3-Clause License
#
# Copyright (c) 2021, akuker
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this
#    list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its
#    contributors may be used to endorse or promote products derived from
#    this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import RPi.GPIO as gpio
import time

pin_settle_delay = 0.01

err_count = 0

# Define constants for each of the SCSI signals, based upon their
# raspberry pi pin number (since we're using BOARD mode of RPi.GPIO)
scsi_d0_gpio = 19
scsi_d1_gpio = 23
scsi_d2_gpio = 32
scsi_d3_gpio = 33
scsi_d4_gpio = 8
scsi_d5_gpio = 10
scsi_d6_gpio = 36
scsi_d7_gpio = 11
scsi_dp_gpio = 12
scsi_atn_gpio = 35
scsi_rst_gpio = 38
scsi_ack_gpio = 40
scsi_req_gpio = 15
scsi_msg_gpio = 16
scsi_cd_gpio = 18
scsi_io_gpio = 22
scsi_bsy_gpio = 37
scsi_sel_gpio = 13

# Pin numbers of the direction controllers of the RaSCSI board
rascsi_ind_gpio = 31
rascsi_tad_gpio = 26
rascsi_dtd_gpio = 24
rascsi_none = -1

# Matrix showing all of the SCSI signals, along what signal they're looped back to.
# dir_ctrl indicates which direction control pin is associated with that output
gpio_map = [
    {
        "gpio_num": scsi_d0_gpio,
        "attached_to": scsi_ack_gpio,
        "dir_ctrl": rascsi_dtd_gpio,
    },
    {
        "gpio_num": scsi_d1_gpio,
        "attached_to": scsi_sel_gpio,
        "dir_ctrl": rascsi_dtd_gpio,
    },
    {
        "gpio_num": scsi_d2_gpio,
        "attached_to": scsi_atn_gpio,
        "dir_ctrl": rascsi_dtd_gpio,
    },
    {
        "gpio_num": scsi_d3_gpio,
        "attached_to": scsi_rst_gpio,
        "dir_ctrl": rascsi_dtd_gpio,
    },
    {
        "gpio_num": scsi_d4_gpio,
        "attached_to": scsi_cd_gpio,
        "dir_ctrl": rascsi_dtd_gpio,
    },
    {
        "gpio_num": scsi_d5_gpio,
        "attached_to": scsi_io_gpio,
        "dir_ctrl": rascsi_dtd_gpio,
    },
    {
        "gpio_num": scsi_d6_gpio,
        "attached_to": scsi_msg_gpio,
        "dir_ctrl": rascsi_dtd_gpio,
    },
    {
        "gpio_num": scsi_d7_gpio,
        "attached_to": scsi_req_gpio,
        "dir_ctrl": rascsi_dtd_gpio,
    },
    {
        "gpio_num": scsi_dp_gpio,
        "attached_to": scsi_bsy_gpio,
        "dir_ctrl": rascsi_dtd_gpio,
    },
    {
        "gpio_num": scsi_atn_gpio,
        "attached_to": scsi_d2_gpio,
        "dir_ctrl": rascsi_ind_gpio,
    },
    {
        "gpio_num": scsi_rst_gpio,
        "attached_to": scsi_d3_gpio,
        "dir_ctrl": rascsi_ind_gpio,
    },
    {
        "gpio_num": scsi_ack_gpio,
        "attached_to": scsi_d0_gpio,
        "dir_ctrl": rascsi_ind_gpio,
    },
    {
        "gpio_num": scsi_req_gpio,
        "attached_to": scsi_d7_gpio,
        "dir_ctrl": rascsi_tad_gpio,
    },
    {
        "gpio_num": scsi_msg_gpio,
        "attached_to": scsi_d6_gpio,
        "dir_ctrl": rascsi_tad_gpio,
    },
    {
        "gpio_num": scsi_cd_gpio,
        "attached_to": scsi_d4_gpio,
        "dir_ctrl": rascsi_tad_gpio,
    },
    {
        "gpio_num": scsi_io_gpio,
        "attached_to": scsi_d5_gpio,
        "dir_ctrl": rascsi_tad_gpio,
    },
    {
        "gpio_num": scsi_bsy_gpio,
        "attached_to": scsi_dp_gpio,
        "dir_ctrl": rascsi_tad_gpio,
    },
    {
        "gpio_num": scsi_sel_gpio,
        "attached_to": scsi_d1_gpio,
        "dir_ctrl": rascsi_ind_gpio,
    },
]

# List of all of the SCSI signals that is also a dictionary to their human readable name
scsi_signals = {
    scsi_d0_gpio: "D0",
    scsi_d1_gpio: "D1",
    scsi_d2_gpio: "D2",
    scsi_d3_gpio: "D3",
    scsi_d4_gpio: "D4",
    scsi_d5_gpio: "D5",
    scsi_d6_gpio: "D6",
    scsi_d7_gpio: "D7",
    scsi_dp_gpio: "DP",
    scsi_atn_gpio: "ATN",
    scsi_rst_gpio: "RST",
    scsi_ack_gpio: "ACK",
    scsi_req_gpio: "REQ",
    scsi_msg_gpio: "MSG",
    scsi_cd_gpio: "CD",
    scsi_io_gpio: "IO",
    scsi_bsy_gpio: "BSY",
    scsi_sel_gpio: "SEL",
}


# Debug function that just dumps the status of all of the scsi signals to the console
def print_all():
    for cur_gpio in gpio_map:
        print(
            cur_gpio["name"] + "=" + str(gpio.input(cur_gpio["gpio_num"])) + "  ",
            end="",
            flush=True,
        )
    print("")


# Set transceivers IC1 and IC2 to OUTPUT
def set_dtd_out():
    gpio.output(rascsi_dtd_gpio, gpio.LOW)


# Set transceivers IC1 and IC2 to INPUT
def set_dtd_in():
    gpio.output(rascsi_dtd_gpio, gpio.HIGH)


# Set transceiver IC4 to OUTPUT
def set_ind_out():
    gpio.output(rascsi_ind_gpio, gpio.HIGH)


# Set transceiver IC4 to INPUT
def set_ind_in():
    gpio.output(rascsi_ind_gpio, gpio.LOW)


# Set transceiver IC3 to OUTPUT
def set_tad_out():
    gpio.output(rascsi_tad_gpio, gpio.HIGH)


# Set transceiver IC3 to INPUT
def set_tad_in():
    gpio.output(rascsi_tad_gpio, gpio.LOW)


# Set the specified transciever to an OUTPUT. All of the other transceivers
# will be set to inputs. If a non-existent direction gpio is specified, this
# will set all of the transceivers to inputs.
def set_output_channel(out_gpio):
    if out_gpio == rascsi_tad_gpio:
        set_tad_out()
    else:
        set_tad_in()
    if out_gpio == rascsi_dtd_gpio:
        set_dtd_out()
    else:
        set_dtd_in()
    if out_gpio == rascsi_ind_gpio:
        set_ind_out()
    else:
        set_ind_in()


# Main test procedure. This will execute for each of the SCSI pins to make sure its connected
# properly.
def test_gpio_pin(gpio_rec):
    global err_count

    set_output_channel(gpio_rec["dir_ctrl"])

    ############################################
    # set the test gpio low
    gpio.output(gpio_rec["gpio_num"], gpio.LOW)

    time.sleep(pin_settle_delay)

    # loop through all of the gpios
    for cur_gpio in scsi_signals:
        # all of the gpios should be high except for the test gpio and the connected gpio
        cur_val = gpio.input(cur_gpio)
        if cur_gpio == gpio_rec["gpio_num"]:
            if cur_val != gpio.LOW:
                print(
                    "Error: Test commanded GPIO "
                    + scsi_signals[gpio_rec["gpio_num"]]
                    + " to be low, but it did not respond"
                )
                err_count = err_count + 1
        elif cur_gpio == gpio_rec["attached_to"]:
            if cur_val != gpio.LOW:
                print(
                    "Error: GPIO "
                    + scsi_signals[gpio_rec["gpio_num"]]
                    + " should drive "
                    + scsi_signals[gpio_rec["attached_to"]]
                    + " low, but did not"
                )
                err_count = err_count + 1
        else:
            if cur_val != gpio.HIGH:
                print(
                    "Error: GPIO "
                    + scsi_signals[gpio_rec["gpio_num"]]
                    + " incorrectly pulled "
                    + scsi_signals[cur_gpio]
                    + " LOW, when it shouldn't have"
                )
                err_count = err_count + 1

    ############################################
    # set the transceivers to input
    set_output_channel(rascsi_none)

    time.sleep(pin_settle_delay)

    # loop through all of the gpios
    for cur_gpio in scsi_signals:
        # all of the gpios should be high except for the test gpio
        cur_val = gpio.input(cur_gpio)
        if cur_gpio == gpio_rec["gpio_num"]:
            if cur_val != gpio.LOW:
                print(
                    "Error: Test commanded GPIO "
                    + scsi_signals[gpio_rec["gpio_num"]]
                    + " to be low, but it did not respond"
                )
                err_count = err_count + 1
        else:
            if cur_val != gpio.HIGH:
                print(
                    "Error: GPIO "
                    + scsi_signals[gpio_rec["gpio_num"]]
                    + " incorrectly pulled "
                    + scsi_signals[cur_gpio]
                    + " LOW, when it shouldn't have"
                )
                err_count = err_count + 1

    # Set the transceiver back to output
    set_output_channel(gpio_rec["dir_ctrl"])

    #############################################
    # set the test gpio high
    gpio.output(gpio_rec["gpio_num"], gpio.HIGH)

    time.sleep(pin_settle_delay)

    # loop through all of the gpios
    for cur_gpio in scsi_signals:
        # all of the gpios should be high
        cur_val = gpio.input(cur_gpio)
        if cur_gpio == gpio_rec["gpio_num"]:
            if cur_val != gpio.HIGH:
                print(
                    "Error: Test commanded GPIO "
                    + scsi_signals[gpio_rec["gpio_num"]]
                    + " to be high, but it did not respond"
                )
                err_count = err_count + 1
        else:
            if cur_val != gpio.HIGH:
                print(
                    "Error: GPIO "
                    + scsi_signals[gpio_rec["gpio_num"]]
                    + " incorrectly pulled "
                    + scsi_signals[cur_gpio]
                    + " LOW, when it shouldn't have"
                )
                err_count = err_count + 1


# Initialize the GPIO library, set all of the gpios associated with SCSI signals to outputs and set
# all of the direction control gpios to outputs
def setup():
    gpio.setmode(gpio.BOARD)
    gpio.setwarnings(False)
    for cur_gpio in gpio_map:
        gpio.setup(cur_gpio["gpio_num"], gpio.OUT, initial=gpio.HIGH)

    # Setup direction control
    gpio.setup(rascsi_ind_gpio, gpio.OUT)
    gpio.setup(rascsi_tad_gpio, gpio.OUT)
    gpio.setup(rascsi_dtd_gpio, gpio.OUT)


# Main functions for running the actual test.
if __name__ == "__main__":
    # setup the GPIOs
    setup()
    # Test each SCSI signal in the gpio_map
    for cur_gpio in gpio_map:
        test_gpio_pin(cur_gpio)

    # Print the test results
    if err_count == 0:
        print("-------- Test PASSED --------")
    else:
        print("!!!!!!!! Test FAILED !!!!!!!!")
    print("Total errors: " + str(err_count))

    gpio.cleanup()
