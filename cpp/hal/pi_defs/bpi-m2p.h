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

#pragma once

#include "hal/pi_defs/bpi-gpio.h"

/* The following define the mapping of the Banana Pi CPU pins to the logical
 * GPIO numbers. The GPIO numbers are used by the software to set/configure
 * the CPU registers. */
#define BPI_M2P_01 -1
#define BPI_M2P_03 GPIO_PA12
#define BPI_M2P_05 GPIO_PA11
#define BPI_M2P_07 GPIO_PA06
#define BPI_M2P_09 -1
#define BPI_M2P_11 GPIO_PA01
#define BPI_M2P_13 GPIO_PA00
#define BPI_M2P_15 GPIO_PA03
#define BPI_M2P_17 -1
#define BPI_M2P_19 GPIO_PC00
#define BPI_M2P_21 GPIO_PC01
#define BPI_M2P_23 GPIO_PC02
#define BPI_M2P_25 -1
#define BPI_M2P_27 GPIO_PA19
#define BPI_M2P_29 GPIO_PA07
#define BPI_M2P_31 GPIO_PA08
#define BPI_M2P_33 GPIO_PA09
#define BPI_M2P_35 GPIO_PA10
#define BPI_M2P_37 GPIO_PA17
#define BPI_M2P_39 -1

#define BPI_M2P_02 -1
#define BPI_M2P_04 -1
#define BPI_M2P_06 -1
#define BPI_M2P_08 GPIO_PA13
#define BPI_M2P_10 GPIO_PA14
#define BPI_M2P_12 GPIO_PA16
#define BPI_M2P_14 -1
#define BPI_M2P_16 GPIO_PA15
#define BPI_M2P_18 GPIO_PC04
#define BPI_M2P_20 -1
#define BPI_M2P_22 GPIO_PA02
#define BPI_M2P_24 GPIO_PC03
#define BPI_M2P_26 GPIO_PC07
#define BPI_M2P_28 GPIO_PA18
#define BPI_M2P_30 -1
#define BPI_M2P_32 GPIO_PL02
#define BPI_M2P_34 -1
#define BPI_M2P_36 GPIO_PL04
#define BPI_M2P_38 GPIO_PA21
#define BPI_M2P_40 GPIO_PA20
