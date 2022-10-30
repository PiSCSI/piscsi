/*
Copyright (c) 2014-2017 Banana Pi
Updates Copyright (C) 2022 akuker

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "hal/pi_defs/bpi-gpio.h"

/* The following define the mapping of the Banana Pi CPU pins to the logical
 * GPIO numbers. The GPIO numbers are used by the software to set/configure
 * the CPU registers. */
#define BPI_M2_01 -1
#define BPI_M2_03 GPIO_PH19
#define BPI_M2_05 GPIO_PH18
#define BPI_M2_07 GPIO_PH09
#define BPI_M2_09 -1
#define BPI_M2_11 GPIO_PG07
#define BPI_M2_13 GPIO_PG06
#define BPI_M2_15 GPIO_PG09
#define BPI_M2_17 -1
#define BPI_M2_19 GPIO_PG15
#define BPI_M2_21 GPIO_PG16
#define BPI_M2_23 GPIO_PG14
#define BPI_M2_25 -1
#define BPI_M2_27 GPIO_PB06
#define BPI_M2_29 GPIO_PB00
#define BPI_M2_31 GPIO_PB01
#define BPI_M2_33 GPIO_PB02
#define BPI_M2_35 GPIO_PB03
#define BPI_M2_37 GPIO_PB04
#define BPI_M2_39 -1

#define BPI_M2_02 -1
#define BPI_M2_04 -1
#define BPI_M2_06 -1
#define BPI_M2_08 GPIO_PE04
#define BPI_M2_10 GPIO_PE05
#define BPI_M2_12 GPIO_PH10
#define BPI_M2_14 -1
#define BPI_M2_16 GPIO_PH11
#define BPI_M2_18 GPIO_PH12
#define BPI_M2_20 -1
#define BPI_M2_22 GPIO_PG08
#define BPI_M2_24 GPIO_PG13
#define BPI_M2_26 GPIO_PG12
#define BPI_M2_28 GPIO_PB05
#define BPI_M2_30 -1
#define BPI_M2_32 GPIO_PB07
#define BPI_M2_34 -1
#define BPI_M2_36 GPIO_PE06
#define BPI_M2_38 GPIO_PE07
#define BPI_M2_40 GPIO_PM02

const Banana_Pi_Gpio_Mapping banana_pi_m2_map{
    .phys_to_gpio_map =
        {
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_03, BPI_M2_03},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_05, BPI_M2_05},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_07, BPI_M2_07},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_08, BPI_M2_08},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_10, BPI_M2_10},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_11, BPI_M2_11},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_12, BPI_M2_12},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_13, BPI_M2_13},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_15, BPI_M2_15},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_16, BPI_M2_16},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_18, BPI_M2_18},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_19, BPI_M2_19},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_21, BPI_M2_21},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_22, BPI_M2_22},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_23, BPI_M2_23},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_24, BPI_M2_24},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_26, BPI_M2_26},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_27, BPI_M2_27},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_28, BPI_M2_28},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_29, BPI_M2_29},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_31, BPI_M2_31},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_32, BPI_M2_32},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_33, BPI_M2_33},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_35, BPI_M2_35},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_36, BPI_M2_36},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_37, BPI_M2_37},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_38, BPI_M2_38},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_40, BPI_M2_40},
        },

    .I2C_DEV    = "/dev/i2c-2",
    .SPI_DEV    = "/dev/spidev0.0",
    .I2C_OFFSET = 2,
    .SPI_OFFSET = 2,
    .PWM_OFFSET = 4,
};
