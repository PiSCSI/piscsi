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
#define BPI_M1P_01 -1
#define BPI_M1P_03 GPIO_PB21
#define BPI_M1P_05 GPIO_PB20
#define BPI_M1P_07 GPIO_PI03
#define BPI_M1P_09 -1
#define BPI_M1P_11 GPIO_PI19
#define BPI_M1P_13 GPIO_PI18
#define BPI_M1P_15 GPIO_PI17
#define BPI_M1P_17 -1
#define BPI_M1P_19 GPIO_PI12
#define BPI_M1P_21 GPIO_PI13
#define BPI_M1P_23 GPIO_PI11
#define BPI_M1P_25 -1
#define BPI_M1P_27 GPIO_PI01
#define BPI_M1P_29 GPIO_PB05
#define BPI_M1P_31 GPIO_PB06
#define BPI_M1P_33 GPIO_PB07
#define BPI_M1P_35 GPIO_PB08
#define BPI_M1P_37 GPIO_PB03
#define BPI_M1P_39 -1

#define BPI_M1P_02 -1
#define BPI_M1P_04 -1
#define BPI_M1P_06 -1
#define BPI_M1P_08 GPIO_PH00
#define BPI_M1P_10 GPIO_PH01
#define BPI_M1P_12 GPIO_PH02
#define BPI_M1P_14 -1
#define BPI_M1P_16 GPIO_PH20
#define BPI_M1P_18 GPIO_PH21
#define BPI_M1P_20 -1
#define BPI_M1P_22 GPIO_PI16
#define BPI_M1P_24 GPIO_PI10
#define BPI_M1P_26 GPIO_PI14
#define BPI_M1P_28 GPIO_PI00
#define BPI_M1P_30 -1
#define BPI_M1P_32 GPIO_PB12
#define BPI_M1P_34 -1
#define BPI_M1P_36 GPIO_PI21
#define BPI_M1P_38 GPIO_PI20
#define BPI_M1P_40 GPIO_PB13

const Banana_Pi_Gpio_Mapping banana_pi_m1_m1p_r1_map{
    .phys_to_gpio_map =
        {
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_03, BPI_M1P_03},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_05, BPI_M1P_05},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_07, BPI_M1P_07},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_08, BPI_M1P_08},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_10, BPI_M1P_10},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_11, BPI_M1P_11},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_12, BPI_M1P_12},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_13, BPI_M1P_13},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_15, BPI_M1P_15},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_16, BPI_M1P_16},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_18, BPI_M1P_18},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_19, BPI_M1P_19},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_21, BPI_M1P_21},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_22, BPI_M1P_22},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_23, BPI_M1P_23},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_24, BPI_M1P_24},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_26, BPI_M1P_26},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_27, BPI_M1P_27},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_28, BPI_M1P_28},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_29, BPI_M1P_29},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_31, BPI_M1P_31},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_32, BPI_M1P_32},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_33, BPI_M1P_33},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_35, BPI_M1P_35},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_36, BPI_M1P_36},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_37, BPI_M1P_37},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_38, BPI_M1P_38},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_40, BPI_M1P_40},
        },

    .I2C_DEV    = "/dev/i2c-2",
    .SPI_DEV    = "/dev/spidev0.0",
    .I2C_OFFSET = 2,
    .SPI_OFFSET = 2,
    .PWM_OFFSET = 2,
};