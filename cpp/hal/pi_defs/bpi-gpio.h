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
#include "hal/board_type.h"
#include "hal/sbc_version.h"
#include <map>
#include <string>

#define GPIO_PA00 0
#define GPIO_PA01 1
#define GPIO_PA02 2
#define GPIO_PA03 3
#define GPIO_PA04 4
#define GPIO_PA05 5
#define GPIO_PA06 6
#define GPIO_PA07 7
#define GPIO_PA08 8
#define GPIO_PA09 9
#define GPIO_PA10 10
#define GPIO_PA11 11
#define GPIO_PA12 12
#define GPIO_PA13 13
#define GPIO_PA14 14
#define GPIO_PA15 15
#define GPIO_PA16 16
#define GPIO_PA17 17
#define GPIO_PA18 18
#define GPIO_PA19 19
#define GPIO_PA20 20
#define GPIO_PA21 21
#define GPIO_PA22 22
#define GPIO_PA23 23
#define GPIO_PA24 24
#define GPIO_PA25 25
#define GPIO_PA26 26
#define GPIO_PA27 27
#define GPIO_PA28 28
#define GPIO_PA29 29
#define GPIO_PA30 30
#define GPIO_PA31 31

#define GPIO_PB00 32
#define GPIO_PB01 1 + GPIO_PB00
#define GPIO_PB02 2 + GPIO_PB00
#define GPIO_PB03 3 + GPIO_PB00
#define GPIO_PB04 4 + GPIO_PB00
#define GPIO_PB05 5 + GPIO_PB00
#define GPIO_PB06 6 + GPIO_PB00
#define GPIO_PB07 7 + GPIO_PB00
#define GPIO_PB08 8 + GPIO_PB00
#define GPIO_PB09 9 + GPIO_PB00
#define GPIO_PB10 10 + GPIO_PB00
#define GPIO_PB11 11 + GPIO_PB00
#define GPIO_PB12 12 + GPIO_PB00
#define GPIO_PB13 13 + GPIO_PB00
#define GPIO_PB14 14 + GPIO_PB00
#define GPIO_PB15 15 + GPIO_PB00
#define GPIO_PB16 16 + GPIO_PB00
#define GPIO_PB17 17 + GPIO_PB00
#define GPIO_PB18 18 + GPIO_PB00
#define GPIO_PB19 19 + GPIO_PB00
#define GPIO_PB20 20 + GPIO_PB00
#define GPIO_PB21 21 + GPIO_PB00
#define GPIO_PB22 22 + GPIO_PB00
#define GPIO_PB23 23 + GPIO_PB00
#define GPIO_PB24 24 + GPIO_PB00
#define GPIO_PB25 25 + GPIO_PB00
#define GPIO_PB26 26 + GPIO_PB00
#define GPIO_PB27 27 + GPIO_PB00
#define GPIO_PB28 28 + GPIO_PB00
#define GPIO_PB29 29 + GPIO_PB00
#define GPIO_PB30 30 + GPIO_PB00
#define GPIO_PB31 31 + GPIO_PB00

#define GPIO_PC00 64
#define GPIO_PC01 1 + GPIO_PC00
#define GPIO_PC02 2 + GPIO_PC00
#define GPIO_PC03 3 + GPIO_PC00
#define GPIO_PC04 4 + GPIO_PC00
#define GPIO_PC05 5 + GPIO_PC00
#define GPIO_PC06 6 + GPIO_PC00
#define GPIO_PC07 7 + GPIO_PC00
#define GPIO_PC08 8 + GPIO_PC00
#define GPIO_PC09 9 + GPIO_PC00
#define GPIO_PC10 10 + GPIO_PC00
#define GPIO_PC11 11 + GPIO_PC00
#define GPIO_PC12 12 + GPIO_PC00
#define GPIO_PC13 13 + GPIO_PC00
#define GPIO_PC14 14 + GPIO_PC00
#define GPIO_PC15 15 + GPIO_PC00
#define GPIO_PC16 16 + GPIO_PC00
#define GPIO_PC17 17 + GPIO_PC00
#define GPIO_PC18 18 + GPIO_PC00
#define GPIO_PC19 19 + GPIO_PC00
#define GPIO_PC20 20 + GPIO_PC00
#define GPIO_PC21 21 + GPIO_PC00
#define GPIO_PC22 22 + GPIO_PC00
#define GPIO_PC23 23 + GPIO_PC00
#define GPIO_PC24 24 + GPIO_PC00
#define GPIO_PC25 25 + GPIO_PC00
#define GPIO_PC26 26 + GPIO_PC00
#define GPIO_PC27 27 + GPIO_PC00
#define GPIO_PC28 28 + GPIO_PC00
#define GPIO_PC29 29 + GPIO_PC00
#define GPIO_PC30 30 + GPIO_PC00
#define GPIO_PC31 31 + GPIO_PC00

#define GPIO_PD00 96
#define GPIO_PD01 1 + GPIO_PD00
#define GPIO_PD02 2 + GPIO_PD00
#define GPIO_PD03 3 + GPIO_PD00
#define GPIO_PD04 4 + GPIO_PD00
#define GPIO_PD05 5 + GPIO_PD00
#define GPIO_PD06 6 + GPIO_PD00
#define GPIO_PD07 7 + GPIO_PD00
#define GPIO_PD08 8 + GPIO_PD00
#define GPIO_PD09 9 + GPIO_PD00
#define GPIO_PD10 10 + GPIO_PD00
#define GPIO_PD11 11 + GPIO_PD00
#define GPIO_PD12 12 + GPIO_PD00
#define GPIO_PD13 13 + GPIO_PD00
#define GPIO_PD14 14 + GPIO_PD00
#define GPIO_PD15 15 + GPIO_PD00
#define GPIO_PD16 16 + GPIO_PD00
#define GPIO_PD17 17 + GPIO_PD00
#define GPIO_PD18 18 + GPIO_PD00
#define GPIO_PD19 19 + GPIO_PD00
#define GPIO_PD20 20 + GPIO_PD00
#define GPIO_PD21 21 + GPIO_PD00
#define GPIO_PD22 22 + GPIO_PD00
#define GPIO_PD23 23 + GPIO_PD00
#define GPIO_PD24 24 + GPIO_PD00
#define GPIO_PD25 25 + GPIO_PD00
#define GPIO_PD26 26 + GPIO_PD00
#define GPIO_PD27 27 + GPIO_PD00
#define GPIO_PD28 28 + GPIO_PD00
#define GPIO_PD29 29 + GPIO_PD00
#define GPIO_PD30 30 + GPIO_PD00
#define GPIO_PD31 31 + GPIO_PD00

#define GPIO_PE00 128
#define GPIO_PE01 1 + GPIO_PE00
#define GPIO_PE02 2 + GPIO_PE00
#define GPIO_PE03 3 + GPIO_PE00
#define GPIO_PE04 4 + GPIO_PE00
#define GPIO_PE05 5 + GPIO_PE00
#define GPIO_PE06 6 + GPIO_PE00
#define GPIO_PE07 7 + GPIO_PE00
#define GPIO_PE08 8 + GPIO_PE00
#define GPIO_PE09 9 + GPIO_PE00
#define GPIO_PE10 10 + GPIO_PE00
#define GPIO_PE11 11 + GPIO_PE00
#define GPIO_PE12 12 + GPIO_PE00
#define GPIO_PE13 13 + GPIO_PE00
#define GPIO_PE14 14 + GPIO_PE00
#define GPIO_PE15 15 + GPIO_PE00
#define GPIO_PE16 16 + GPIO_PE00
#define GPIO_PE17 17 + GPIO_PE00
#define GPIO_PE18 18 + GPIO_PE00
#define GPIO_PE19 19 + GPIO_PE00
#define GPIO_PE20 20 + GPIO_PE00
#define GPIO_PE21 21 + GPIO_PE00
#define GPIO_PE22 22 + GPIO_PE00
#define GPIO_PE23 23 + GPIO_PE00
#define GPIO_PE24 24 + GPIO_PE00
#define GPIO_PE25 25 + GPIO_PE00
#define GPIO_PE26 26 + GPIO_PE00
#define GPIO_PE27 27 + GPIO_PE00
#define GPIO_PE28 28 + GPIO_PE00
#define GPIO_PE29 29 + GPIO_PE00
#define GPIO_PE30 30 + GPIO_PE00
#define GPIO_PE31 31 + GPIO_PE00

#define GPIO_PG00 192
#define GPIO_PG01 1 + GPIO_PG00
#define GPIO_PG02 2 + GPIO_PG00
#define GPIO_PG03 3 + GPIO_PG00
#define GPIO_PG04 4 + GPIO_PG00
#define GPIO_PG05 5 + GPIO_PG00
#define GPIO_PG06 6 + GPIO_PG00
#define GPIO_PG07 7 + GPIO_PG00
#define GPIO_PG08 8 + GPIO_PG00
#define GPIO_PG09 9 + GPIO_PG00
#define GPIO_PG10 10 + GPIO_PG00
#define GPIO_PG11 11 + GPIO_PG00
#define GPIO_PG12 12 + GPIO_PG00
#define GPIO_PG13 13 + GPIO_PG00
#define GPIO_PG14 14 + GPIO_PG00
#define GPIO_PG15 15 + GPIO_PG00
#define GPIO_PG16 16 + GPIO_PG00
#define GPIO_PG17 17 + GPIO_PG00
#define GPIO_PG18 18 + GPIO_PG00
#define GPIO_PG19 19 + GPIO_PG00
#define GPIO_PG20 20 + GPIO_PG00
#define GPIO_PG21 21 + GPIO_PG00
#define GPIO_PG22 22 + GPIO_PG00
#define GPIO_PG23 23 + GPIO_PG00
#define GPIO_PG24 24 + GPIO_PG00
#define GPIO_PG25 25 + GPIO_PG00
#define GPIO_PG26 26 + GPIO_PG00
#define GPIO_PG27 27 + GPIO_PG00
#define GPIO_PG28 28 + GPIO_PG00
#define GPIO_PG29 29 + GPIO_PG00
#define GPIO_PG30 30 + GPIO_PG00
#define GPIO_PG31 31 + GPIO_PG00

#define GPIO_PH00 224
#define GPIO_PH01 1 + GPIO_PH00
#define GPIO_PH02 2 + GPIO_PH00
#define GPIO_PH03 3 + GPIO_PH00
#define GPIO_PH04 4 + GPIO_PH00
#define GPIO_PH05 5 + GPIO_PH00
#define GPIO_PH06 6 + GPIO_PH00
#define GPIO_PH07 7 + GPIO_PH00
#define GPIO_PH08 8 + GPIO_PH00
#define GPIO_PH09 9 + GPIO_PH00
#define GPIO_PH10 10 + GPIO_PH00
#define GPIO_PH11 11 + GPIO_PH00
#define GPIO_PH12 12 + GPIO_PH00
#define GPIO_PH13 13 + GPIO_PH00
#define GPIO_PH14 14 + GPIO_PH00
#define GPIO_PH15 15 + GPIO_PH00
#define GPIO_PH16 16 + GPIO_PH00
#define GPIO_PH17 17 + GPIO_PH00
#define GPIO_PH18 18 + GPIO_PH00
#define GPIO_PH19 19 + GPIO_PH00
#define GPIO_PH20 20 + GPIO_PH00
#define GPIO_PH21 21 + GPIO_PH00
#define GPIO_PH22 22 + GPIO_PH00
#define GPIO_PH23 23 + GPIO_PH00
#define GPIO_PH24 24 + GPIO_PH00
#define GPIO_PH25 25 + GPIO_PH00
#define GPIO_PH26 26 + GPIO_PH00
#define GPIO_PH27 27 + GPIO_PH00
#define GPIO_PH28 28 + GPIO_PH00
#define GPIO_PH29 29 + GPIO_PH00
#define GPIO_PH30 30 + GPIO_PH00
#define GPIO_PH31 31 + GPIO_PH00

#define GPIO_PI00 256
#define GPIO_PI01 1 + GPIO_PI00
#define GPIO_PI02 2 + GPIO_PI00
#define GPIO_PI03 3 + GPIO_PI00
#define GPIO_PI04 4 + GPIO_PI00
#define GPIO_PI05 5 + GPIO_PI00
#define GPIO_PI06 6 + GPIO_PI00
#define GPIO_PI07 7 + GPIO_PI00
#define GPIO_PI08 8 + GPIO_PI00
#define GPIO_PI09 9 + GPIO_PI00
#define GPIO_PI10 10 + GPIO_PI00
#define GPIO_PI11 11 + GPIO_PI00
#define GPIO_PI12 12 + GPIO_PI00
#define GPIO_PI13 13 + GPIO_PI00
#define GPIO_PI14 14 + GPIO_PI00
#define GPIO_PI15 15 + GPIO_PI00
#define GPIO_PI16 16 + GPIO_PI00
#define GPIO_PI17 17 + GPIO_PI00
#define GPIO_PI18 18 + GPIO_PI00
#define GPIO_PI19 19 + GPIO_PI00
#define GPIO_PI20 20 + GPIO_PI00
#define GPIO_PI21 21 + GPIO_PI00
#define GPIO_PI22 22 + GPIO_PI00
#define GPIO_PI23 23 + GPIO_PI00
#define GPIO_PI24 24 + GPIO_PI00
#define GPIO_PI25 25 + GPIO_PI00
#define GPIO_PI26 26 + GPIO_PI00
#define GPIO_PI27 27 + GPIO_PI00
#define GPIO_PI28 28 + GPIO_PI00
#define GPIO_PI29 29 + GPIO_PI00
#define GPIO_PI30 30 + GPIO_PI00
#define GPIO_PI31 31 + GPIO_PI00

#define GPIO_PL00 352
#define GPIO_PL01 1 + GPIO_PL00
#define GPIO_PL02 2 + GPIO_PL00
#define GPIO_PL03 3 + GPIO_PL00
#define GPIO_PL04 4 + GPIO_PL00
#define GPIO_PL05 5 + GPIO_PL00
#define GPIO_PL06 6 + GPIO_PL00
#define GPIO_PL07 7 + GPIO_PL00
#define GPIO_PL08 8 + GPIO_PL00
#define GPIO_PL09 9 + GPIO_PL00
#define GPIO_PL10 10 + GPIO_PL00
#define GPIO_PL11 11 + GPIO_PL00
#define GPIO_PL12 12 + GPIO_PL00
#define GPIO_PL13 13 + GPIO_PL00
#define GPIO_PL14 14 + GPIO_PL00
#define GPIO_PL15 15 + GPIO_PL00
#define GPIO_PL16 16 + GPIO_PL00
#define GPIO_PL17 17 + GPIO_PL00
#define GPIO_PL18 18 + GPIO_PL00
#define GPIO_PL19 19 + GPIO_PL00
#define GPIO_PL20 20 + GPIO_PL00
#define GPIO_PL21 21 + GPIO_PL00
#define GPIO_PL22 22 + GPIO_PL00
#define GPIO_PL23 23 + GPIO_PL00
#define GPIO_PL24 24 + GPIO_PL00
#define GPIO_PL25 25 + GPIO_PL00
#define GPIO_PL26 26 + GPIO_PL00
#define GPIO_PL27 27 + GPIO_PL00
#define GPIO_PL28 28 + GPIO_PL00
#define GPIO_PL29 29 + GPIO_PL00
#define GPIO_PL30 30 + GPIO_PL00
#define GPIO_PL31 31 + GPIO_PL00

#define GPIO_PM00 384
#define GPIO_PM01 1 + GPIO_PM00
#define GPIO_PM02 2 + GPIO_PM00
#define GPIO_PM03 3 + GPIO_PM00
#define GPIO_PM04 4 + GPIO_PM00
#define GPIO_PM05 5 + GPIO_PM00
#define GPIO_PM06 6 + GPIO_PM00
#define GPIO_PM07 7 + GPIO_PM00
#define GPIO_PM08 8 + GPIO_PM00
#define GPIO_PM09 9 + GPIO_PM00
#define GPIO_PM10 10 + GPIO_PM00
#define GPIO_PM11 11 + GPIO_PM00
#define GPIO_PM12 12 + GPIO_PM00
#define GPIO_PM13 13 + GPIO_PM00
#define GPIO_PM14 14 + GPIO_PM00
#define GPIO_PM15 15 + GPIO_PM00
#define GPIO_PM16 16 + GPIO_PM00
#define GPIO_PM17 17 + GPIO_PM00
#define GPIO_PM18 18 + GPIO_PM00
#define GPIO_PM19 19 + GPIO_PM00
#define GPIO_PM20 20 + GPIO_PM00
#define GPIO_PM21 21 + GPIO_PM00
#define GPIO_PM22 22 + GPIO_PM00
#define GPIO_PM23 23 + GPIO_PM00
#define GPIO_PM24 24 + GPIO_PM00
#define GPIO_PM25 25 + GPIO_PM00
#define GPIO_PM26 26 + GPIO_PM00
#define GPIO_PM27 27 + GPIO_PM00
#define GPIO_PM28 28 + GPIO_PM00
#define GPIO_PM29 29 + GPIO_PM00
#define GPIO_PM30 30 + GPIO_PM00
#define GPIO_PM31 31 + GPIO_PM00

// struct Banana_Pi_Gpio_Mapping_Struct {
//     const std::map<board_type::pi_physical_pin_e, int> phys_to_gpio_map;

//     const std::string I2C_DEV;
//     const std::string SPI_DEV;

//     const int I2C_OFFSET;
//     const int SPI_OFFSET;
//     const int PWM_OFFSET;
// };

// typedef struct Banana_Pi_Gpio_Mapping_Struct Banana_Pi_Gpio_Mapping;

// extern const Banana_Pi_Gpio_Mapping banana_pi_m1_m1p_r1_map;
// extern const Banana_Pi_Gpio_Mapping banana_pi_m2_map;
// extern const Banana_Pi_Gpio_Mapping banana_pi_m2m_1p1_map;
// extern const Banana_Pi_Gpio_Mapping banana_pi_m2m_map;
// extern const Banana_Pi_Gpio_Mapping banana_pi_m2p_map;
// extern const Banana_Pi_Gpio_Mapping banana_pi_m2u_map;
// extern const Banana_Pi_Gpio_Mapping banana_pi_m3_map;
// extern const Banana_Pi_Gpio_Mapping banana_pi_m64_map;

// extern int physToGpio_BPI_M1P[64];
// extern int physToGpio_BPI_M2[64];
// extern int physToGpio_BPI_M2M[64];
// extern int physToGpio_BPI_M2M_1P1[64];
// extern int physToGpio_BPI_M2P[64];
// extern int physToGpio_BPI_M2U[64];
// extern int physToGpio_BPI_M3[64];
// extern int physToGpio_BPI_M64[64];

// class BPI_GPIO
// {
//   public:
//     static shared_ptr<Banana_Pi_Gpio_Mapping> GetBpiGpioMapping(SBC_Version::sbc_version_type sbc_version);
// };
