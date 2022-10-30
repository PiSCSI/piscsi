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
#define BPI_M2U_01 -1
#define BPI_M2U_03 GPIO_PB21
#define BPI_M2U_05 GPIO_PB20
#define BPI_M2U_07 GPIO_PB03
#define BPI_M2U_09 -1
#define BPI_M2U_11 GPIO_PI20
#define BPI_M2U_13 GPIO_PI21
#define BPI_M2U_15 GPIO_PH25
#define BPI_M2U_17 -1
#define BPI_M2U_19 GPIO_PC00
#define BPI_M2U_21 GPIO_PC01
#define BPI_M2U_23 GPIO_PC02
#define BPI_M2U_25 -1
#define BPI_M2U_27 GPIO_PI01
#define BPI_M2U_29 GPIO_PH00
#define BPI_M2U_31 GPIO_PH01
#define BPI_M2U_33 GPIO_PH02
#define BPI_M2U_35 GPIO_PH03
#define BPI_M2U_37 GPIO_PH04
#define BPI_M2U_39 -1

#define BPI_M2U_02 -1
#define BPI_M2U_04 -1
#define BPI_M2U_06 -1
#define BPI_M2U_08 GPIO_PI18
#define BPI_M2U_10 GPIO_PI19
#define BPI_M2U_12 GPIO_PI17
#define BPI_M2U_14 -1
#define BPI_M2U_16 GPIO_PI16
#define BPI_M2U_18 GPIO_PH26
#define BPI_M2U_20 -1
#define BPI_M2U_22 GPIO_PH27
#define BPI_M2U_24 GPIO_PC23
#define BPI_M2U_26 GPIO_PH24
#define BPI_M2U_28 GPIO_PI00
#define BPI_M2U_30 -1
#define BPI_M2U_32 GPIO_PD20
#define BPI_M2U_34 -1
#define BPI_M2U_36 GPIO_PH07
#define BPI_M2U_38 GPIO_PH06
#define BPI_M2U_40 GPIO_PH05

const Banana_Pi_Gpio_Mapping banana_pi_m2u_map{
    .phys_to_gpio_map =
        {
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_03, BPI_M2U_03},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_05, BPI_M2U_05},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_07, BPI_M2U_07},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_08, BPI_M2U_08},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_10, BPI_M2U_10},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_11, BPI_M2U_11},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_12, BPI_M2U_12},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_13, BPI_M2U_13},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_15, BPI_M2U_15},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_16, BPI_M2U_16},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_18, BPI_M2U_18},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_19, BPI_M2U_19},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_21, BPI_M2U_21},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_22, BPI_M2U_22},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_23, BPI_M2U_23},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_24, BPI_M2U_24},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_26, BPI_M2U_26},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_27, BPI_M2U_27},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_28, BPI_M2U_28},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_29, BPI_M2U_29},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_31, BPI_M2U_31},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_32, BPI_M2U_32},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_33, BPI_M2U_33},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_35, BPI_M2U_35},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_36, BPI_M2U_36},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_37, BPI_M2U_37},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_38, BPI_M2U_38},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_40, BPI_M2U_40},
        },

    .I2C_DEV    = "/dev/i2c-2",
    .SPI_DEV    = "/dev/spidev0.0",
    .I2C_OFFSET = 2,
    .SPI_OFFSET = 3,
    .PWM_OFFSET = 3,
};
