#!/usr/bin/env python3

import RPi.GPIO as gpio
import time

pin_settle_delay = 0.01

err_count = 0

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

rascsi_ind_gpio = 31
rascsi_tad_gpio = 26
rascsi_dtd_gpio = 24
rascsi_none = -1

gpio_map = [
    { 'gpio_num': scsi_d0_gpio,  'attached_to': scsi_ack_gpio, 'dir_ctrl': rascsi_dtd_gpio},
    { 'gpio_num': scsi_d1_gpio,  'attached_to': scsi_sel_gpio, 'dir_ctrl': rascsi_dtd_gpio},
    { 'gpio_num': scsi_d2_gpio,  'attached_to': scsi_atn_gpio, 'dir_ctrl': rascsi_dtd_gpio},
    { 'gpio_num': scsi_d3_gpio,  'attached_to': scsi_rst_gpio, 'dir_ctrl': rascsi_dtd_gpio},
    { 'gpio_num': scsi_d4_gpio,  'attached_to': scsi_cd_gpio,  'dir_ctrl': rascsi_dtd_gpio},
    { 'gpio_num': scsi_d5_gpio,  'attached_to': scsi_io_gpio,  'dir_ctrl': rascsi_dtd_gpio},
    { 'gpio_num': scsi_d6_gpio,  'attached_to': scsi_msg_gpio, 'dir_ctrl': rascsi_dtd_gpio},
    { 'gpio_num': scsi_d7_gpio,  'attached_to': scsi_req_gpio, 'dir_ctrl': rascsi_dtd_gpio},
    { 'gpio_num': scsi_dp_gpio,  'attached_to': scsi_bsy_gpio, 'dir_ctrl': rascsi_dtd_gpio},
    { 'gpio_num': scsi_atn_gpio, 'attached_to': scsi_d2_gpio, 'dir_ctrl': rascsi_ind_gpio},
    { 'gpio_num': scsi_rst_gpio, 'attached_to': scsi_d3_gpio, 'dir_ctrl': rascsi_ind_gpio},
    { 'gpio_num': scsi_ack_gpio, 'attached_to': scsi_d0_gpio, 'dir_ctrl': rascsi_ind_gpio},
    { 'gpio_num': scsi_req_gpio, 'attached_to': scsi_d7_gpio, 'dir_ctrl': rascsi_tad_gpio},
    { 'gpio_num': scsi_msg_gpio, 'attached_to': scsi_d6_gpio, 'dir_ctrl': rascsi_tad_gpio},
    { 'gpio_num': scsi_cd_gpio,  'attached_to': scsi_d4_gpio, 'dir_ctrl': rascsi_tad_gpio},
    { 'gpio_num': scsi_io_gpio,  'attached_to': scsi_d5_gpio, 'dir_ctrl': rascsi_tad_gpio},
    { 'gpio_num': scsi_bsy_gpio, 'attached_to': scsi_dp_gpio, 'dir_ctrl': rascsi_tad_gpio},
    { 'gpio_num': scsi_sel_gpio, 'attached_to': scsi_d1_gpio, 'dir_ctrl': rascsi_ind_gpio},
]

scsi_signals = {
    scsi_d0_gpio: 'D0',
    scsi_d1_gpio: 'D1',
    scsi_d2_gpio: 'D2',
    scsi_d3_gpio: 'D3',
    scsi_d4_gpio: 'D4',
    scsi_d5_gpio: 'D5',
    scsi_d6_gpio: 'D6',
    scsi_d7_gpio: 'D7',
    scsi_dp_gpio: 'DP',
    scsi_atn_gpio: 'ATN',
    scsi_rst_gpio: 'RST',
    scsi_ack_gpio: 'ACK',
    scsi_req_gpio: 'REQ',
    scsi_msg_gpio: 'MSG',
    scsi_cd_gpio: 'CD',
    scsi_io_gpio: 'IO',
    scsi_bsy_gpio: 'BSY',
    scsi_sel_gpio: 'SEL'
}

# DB7 -> REQ
# DB6 -> MSG
# DB5 -> I/O
# DB3 -> RST
# DB0 -> ACK
# DB4 -> CD
# DB2 -> ATN

def print_all():
    for cur_gpio in gpio_map:
        print(cur_gpio['name']+"="+str(gpio.input(cur_gpio['gpio_num'])) + "  ", end='', flush=True)
    print("")

def set_dtd_out():
    gpio.output(rascsi_dtd_gpio,gpio.LOW)
def set_dtd_in():
    gpio.output(rascsi_dtd_gpio,gpio.HIGH)

def set_ind_out():
    gpio.output(rascsi_ind_gpio,gpio.HIGH)
def set_ind_in():
    gpio.output(rascsi_ind_gpio,gpio.LOW)

def set_tad_out():
    gpio.output(rascsi_tad_gpio,gpio.HIGH)
def set_tad_in():
    gpio.output(rascsi_tad_gpio,gpio.LOW)

def set_output_channel(out_gpio):
    if(out_gpio == rascsi_tad_gpio):
        set_tad_out()
    else:
        set_tad_in()
    if(out_gpio == rascsi_dtd_gpio):
        set_dtd_out()
    else:
        set_dtd_in()
    if(out_gpio == rascsi_ind_gpio):
        set_ind_out()
    else:
        set_ind_in()


def test_gpio_pin(gpio_rec):
    global err_count

    set_output_channel(gpio_rec['dir_ctrl'])

    ############################################
    # set the test gpio low
    gpio.output(gpio_rec['gpio_num'], gpio.LOW)

    time.sleep(pin_settle_delay)

    # loop through all of the gpios
    for cur_gpio in scsi_signals:
        # all of the gpios should be high except for the test gpio and the connected gpio
        cur_val = gpio.input(cur_gpio)
        if( cur_gpio == gpio_rec['gpio_num']):
            if(cur_val != gpio.LOW):
                print("Error: Test commanded GPIO " + scsi_signals[gpio_rec['gpio_num']] + " to be low, but it did not respond")
                err_count = err_count+1
        elif (cur_gpio == gpio_rec['attached_to']):
            if(cur_val != gpio.LOW):
                print("Error: GPIO " + scsi_signals[gpio_rec['gpio_num']] + " should drive " + scsi_signals[gpio_rec['attached_to']] + " low, but did not")
                err_count = err_count+1
        else:
            if(cur_val != gpio.HIGH):
                print("Error: GPIO " + scsi_signals[gpio_rec['gpio_num']] + " incorrectly pulled " + scsi_signals[cur_gpio] + " LOW, when it shouldn't have")
                err_count = err_count+1

    ############################################
    # set the transceivers to input
    set_output_channel(rascsi_none)

    time.sleep(pin_settle_delay)

    # loop through all of the gpios
    for cur_gpio in scsi_signals:
        # all of the gpios should be high except for the test gpio
        cur_val = gpio.input(cur_gpio)
        if( cur_gpio == gpio_rec['gpio_num']):
            if(cur_val != gpio.LOW):
                print("Error: Test commanded GPIO " + scsi_signals[gpio_rec['gpio_num']] + " to be low, but it did not respond")
                err_count = err_count+1
        else:
            if(cur_val != gpio.HIGH):
                print("Error: GPIO " + scsi_signals[gpio_rec['gpio_num']] + " incorrectly pulled " + scsi_signals[cur_gpio] + " LOW, when it shouldn't have")
                err_count = err_count+1


    # Set the transceiver back to output
    set_output_channel(gpio_rec['dir_ctrl'])

    #############################################
    # set the test gpio high
    gpio.output(gpio_rec['gpio_num'], gpio.HIGH)

    time.sleep(pin_settle_delay)

    # loop through all of the gpios
    for cur_gpio in scsi_signals:
        # all of the gpios should be high
        cur_val = gpio.input(cur_gpio)
        if( cur_gpio == gpio_rec['gpio_num']):
            if(cur_val != gpio.HIGH):
                print("Error: Test commanded GPIO " + scsi_signals[gpio_rec['gpio_num']] + " to be high, but it did not respond")
                err_count = err_count+1
        else:
            if(cur_val != gpio.HIGH):
                print("Error: GPIO " + scsi_signals[gpio_rec['gpio_num']] + " incorrectly pulled " + scsi_signals[cur_gpio] + " LOW, when it shouldn't have")
                err_count = err_count+1


def setup():
    gpio.setmode(gpio.BOARD)
    gpio.setwarnings(False)
    for cur_gpio in gpio_map:
        gpio.setup(cur_gpio['gpio_num'], gpio.OUT, initial=gpio.HIGH)

    # Setup direction control
    gpio.setup(rascsi_ind_gpio, gpio.OUT)
    gpio.setup(rascsi_tad_gpio, gpio.OUT)
    gpio.setup(rascsi_dtd_gpio, gpio.OUT)


# Press the green button in the gutter to run the script.
if __name__ == '__main__':

    setup()

    for cur_gpio in gpio_map:
        test_gpio_pin(cur_gpio)

    print("Total errors: " + str(err_count))
    gpio.cleanup()

    if(err_count == 0):
        print("-------- Test PASSED --------")
    else:
        print("!!!!!!!! Test FAILED !!!!!!!!")
