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
#define BPI_M3_01 -1
#define BPI_M3_03 GPIO_PH05
#define BPI_M3_05 GPIO_PH04
#define BPI_M3_07 GPIO_PL10
#define BPI_M3_09 -1
#define BPI_M3_11 GPIO_PC04
#define BPI_M3_13 GPIO_PC07
#define BPI_M3_15 GPIO_PC17
#define BPI_M3_17 -1
#define BPI_M3_19 GPIO_PC00
#define BPI_M3_21 GPIO_PC01
#define BPI_M3_23 GPIO_PC02
#define BPI_M3_25 -1
#define BPI_M3_27 GPIO_PH03
#define BPI_M3_29 GPIO_PC18
#define BPI_M3_31 GPIO_PG10
#define BPI_M3_33 GPIO_PG11
#define BPI_M3_35 GPIO_PG12
#define BPI_M3_37 GPIO_PE04
#define BPI_M3_39 -1

#define BPI_M3_02 -1
#define BPI_M3_04 -1
#define BPI_M3_06 -1
#define BPI_M3_08 GPIO_PB00
#define BPI_M3_10 GPIO_PB01
#define BPI_M3_12 GPIO_PB03
#define BPI_M3_14 -1
#define BPI_M3_16 GPIO_PB02
#define BPI_M3_18 GPIO_PL08
#define BPI_M3_20 -1
#define BPI_M3_22 GPIO_PL09
#define BPI_M3_24 GPIO_PC03
#define BPI_M3_26 GPIO_PH10
#define BPI_M3_28 GPIO_PH02
#define BPI_M3_30 -1
#define BPI_M3_32 GPIO_PG13
#define BPI_M3_34 -1
#define BPI_M3_36 GPIO_PE05
#define BPI_M3_38 GPIO_PE18
#define BPI_M3_40 GPIO_PE19

const Banana_Pi_Gpio_Mapping banana_pi_m3_map{
    .phys_to_gpio_map =
        {
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_03, BPI_M3_03},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_05, BPI_M3_05},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_07, BPI_M3_07},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_08, BPI_M3_08},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_10, BPI_M3_10},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_11, BPI_M3_11},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_12, BPI_M3_12},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_13, BPI_M3_13},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_15, BPI_M3_15},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_16, BPI_M3_16},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_18, BPI_M3_18},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_19, BPI_M3_19},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_21, BPI_M3_21},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_22, BPI_M3_22},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_23, BPI_M3_23},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_24, BPI_M3_24},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_26, BPI_M3_26},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_27, BPI_M3_27},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_28, BPI_M3_28},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_29, BPI_M3_29},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_31, BPI_M3_31},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_32, BPI_M3_32},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_33, BPI_M3_33},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_35, BPI_M3_35},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_36, BPI_M3_36},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_37, BPI_M3_37},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_38, BPI_M3_38},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_40, BPI_M3_40},
        },

    .I2C_DEV    = "/dev/i2c-2",
    .SPI_DEV    = "/dev/spidev0.0",
    .I2C_OFFSET = 2,
    .SPI_OFFSET = 3,
    .PWM_OFFSET = 2,
};
