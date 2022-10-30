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

#define BPI_M64_01 -1
#define BPI_M64_03 GPIO_PH03
#define BPI_M64_05 GPIO_PH02
#define BPI_M64_07 GPIO_PH06
#define BPI_M64_09 -1
#define BPI_M64_11 GPIO_PH07
#define BPI_M64_13 GPIO_PH10
#define BPI_M64_15 GPIO_PH11
#define BPI_M64_17 -1
#define BPI_M64_19 GPIO_PD02
#define BPI_M64_21 GPIO_PD03
#define BPI_M64_23 GPIO_PD01
#define BPI_M64_25 -1
#define BPI_M64_27 GPIO_PC04
#define BPI_M64_29 GPIO_PC07
#define BPI_M64_31 GPIO_PB05
#define BPI_M64_33 GPIO_PB04
#define BPI_M64_35 GPIO_PB06
#define BPI_M64_37 GPIO_PL12
#define BPI_M64_39 -1

#define BPI_M64_02 -1
#define BPI_M64_04 -1
#define BPI_M64_06 -1
#define BPI_M64_08 GPIO_PB00
#define BPI_M64_10 GPIO_PB01
#define BPI_M64_12 GPIO_PB03
#define BPI_M64_14 -1
#define BPI_M64_16 GPIO_PB02
#define BPI_M64_18 GPIO_PD04
#define BPI_M64_20 -1
#define BPI_M64_22 GPIO_PC00
#define BPI_M64_24 GPIO_PD00
#define BPI_M64_26 GPIO_PC02
#define BPI_M64_28 GPIO_PC03
#define BPI_M64_30 -1
#define BPI_M64_32 GPIO_PB07
#define BPI_M64_34 -1
#define BPI_M64_36 GPIO_PL09
#define BPI_M64_38 GPIO_PL07
#define BPI_M64_40 GPIO_PL08

const Banana_Pi_Gpio_Mapping banana_pi_m64_map{
    .phys_to_gpio_map =
        {
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_03, BPI_M64_03},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_05, BPI_M64_05},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_07, BPI_M64_07},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_08, BPI_M64_08},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_10, BPI_M64_10},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_11, BPI_M64_11},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_12, BPI_M64_12},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_13, BPI_M64_13},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_15, BPI_M64_15},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_16, BPI_M64_16},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_18, BPI_M64_18},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_19, BPI_M64_19},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_21, BPI_M64_21},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_22, BPI_M64_22},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_23, BPI_M64_23},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_24, BPI_M64_24},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_26, BPI_M64_26},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_27, BPI_M64_27},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_28, BPI_M64_28},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_29, BPI_M64_29},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_31, BPI_M64_31},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_32, BPI_M64_32},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_33, BPI_M64_33},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_35, BPI_M64_35},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_36, BPI_M64_36},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_37, BPI_M64_37},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_38, BPI_M64_38},
            {board_type::pi_physical_pin_e::PI_PHYS_PIN_40, BPI_M64_40},
        },

    .I2C_DEV    = "/dev/i2c-1",
    .SPI_DEV    = "/dev/spidev1.0",
    .I2C_OFFSET = 2,
    .SPI_OFFSET = 4,
    .PWM_OFFSET = -1,
};

//map phys_num(index) to bp gpio_num(element)
 int physToGpio_BPI_M64 [64] =
{
          -1,                //0
          -1,        -1,     //1, 2
   BPI_M64_03,        -1,     //3, 4
   BPI_M64_05,        -1,     //5, 6
   BPI_M64_07, BPI_M64_08,     //7, 8
          -1, BPI_M64_10,     //9, 10
   BPI_M64_11, BPI_M64_12,     //11, 12
   BPI_M64_13,        -1,     //13, 14
   BPI_M64_15, BPI_M64_16,     //15, 16
          -1, BPI_M64_18,     //17, 18
   BPI_M64_19,        -1,     //19, 20
   BPI_M64_21, BPI_M64_22,     //21, 22
   BPI_M64_23, BPI_M64_24,     //23, 24
          -1, BPI_M64_26,     //25, 26
   BPI_M64_27, BPI_M64_28,     //27, 28
   BPI_M64_29,        -1,     //29, 30
   BPI_M64_31, BPI_M64_32,     //31, 32      
   BPI_M64_33,        -1,     //33, 34
   BPI_M64_35, BPI_M64_36,     //35, 36
   BPI_M64_37, BPI_M64_38,     //37, 38
          -1, BPI_M64_40,     //39, 40
   -1,   -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, //41-> 55
   -1,   -1, -1, -1, -1, -1, -1, -1 // 56-> 63
} ;
