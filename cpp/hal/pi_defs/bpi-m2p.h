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
const int BPI_M2P_01 = GPIO_NOT_USED;
const int BPI_M2P_03 = GPIO_PA12;
const int BPI_M2P_05 = GPIO_PA11;
const int BPI_M2P_07 = GPIO_PA06;
const int BPI_M2P_09 = GPIO_NOT_USED;
const int BPI_M2P_11 = GPIO_PA01;
const int BPI_M2P_13 = GPIO_PA00;
const int BPI_M2P_15 = GPIO_PA03;
const int BPI_M2P_17 = GPIO_NOT_USED;
const int BPI_M2P_19 = GPIO_PC00;
const int BPI_M2P_21 = GPIO_PC01;
const int BPI_M2P_23 = GPIO_PC02;
const int BPI_M2P_25 = GPIO_NOT_USED;
const int BPI_M2P_27 = GPIO_PA19;
const int BPI_M2P_29 = GPIO_PA07;
const int BPI_M2P_31 = GPIO_PA08;
const int BPI_M2P_33 = GPIO_PA09;
const int BPI_M2P_35 = GPIO_PA10;
const int BPI_M2P_37 = GPIO_PA17;
const int BPI_M2P_39 = GPIO_NOT_USED;

const int BPI_M2P_02 = GPIO_NOT_USED;
const int BPI_M2P_04 = GPIO_NOT_USED;
const int BPI_M2P_06 = GPIO_NOT_USED;
const int BPI_M2P_08 = GPIO_PA13;
const int BPI_M2P_10 = GPIO_PA14;
const int BPI_M2P_12 = GPIO_PA16;
const int BPI_M2P_14 = GPIO_NOT_USED;
const int BPI_M2P_16 = GPIO_PA15;
const int BPI_M2P_18 = GPIO_PC04;
const int BPI_M2P_20 = GPIO_NOT_USED;
const int BPI_M2P_22 = GPIO_PA02;
const int BPI_M2P_24 = GPIO_PC03;
const int BPI_M2P_26 = GPIO_PC07;
const int BPI_M2P_28 = GPIO_PA18;
const int BPI_M2P_30 = GPIO_NOT_USED;
const int BPI_M2P_32 = GPIO_PL02;
const int BPI_M2P_34 = GPIO_NOT_USED;
const int BPI_M2P_36 = GPIO_PL04;
const int BPI_M2P_38 = GPIO_PA21;
const int BPI_M2P_40 = GPIO_PA20;
