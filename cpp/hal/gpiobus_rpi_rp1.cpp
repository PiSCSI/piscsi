// //---------------------------------------------------------------------------
// //
// //	SCSI Target Emulator PiSCSI
// //	for Raspberry Pi
// //
// //	Powered by XM6 TypeG Technology.
// //	Copyright (C) 2016-2020 GIMONS
// //
// //	[ GPIO-SCSI bus ]
// //
// //  Raspberry Pi 4:
// //     https://datasheets.raspberrypi.com/bcm2711/bcm2711-peripherals.pdf
// //  Raspberry Pi Zero:
// //     https://datasheets.raspberrypi.com/bcm2835/bcm2835-peripherals.pdf
// //
// //---------------------------------------------------------------------------

// #include <spdlog/spdlog.h>
// #include "hal/gpiobus_rpi_rp1.h"
// #include "hal/gpiobus.h"
// #include "hal/systimer.h"
// #include <map>
// #include <cstring>
// #ifdef __linux__
// #include <sys/epoll.h>
// #endif
// #include <sys/ioctl.h>
// #include <sys/mman.h>
// #include <sys/time.h>







// #include <assert.h>
// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>

// // #include "gpiochip.h"
// // #include "util.h"


// const GPIOBUS_RPi_rp1::GPIO_STATE_T GPIOBUS_RPi_rp1::gpio_state = {
//     .io = {RP1_IO_BANK0_OFFSET, RP1_IO_BANK1_OFFSET, RP1_IO_BANK2_OFFSET},
//     .pads = {RP1_PADS_BANK0_OFFSET, RP1_PADS_BANK1_OFFSET, RP1_PADS_BANK2_OFFSET},
//     .sys_rio = {RP1_SYS_RIO_BANK0_OFFSET, RP1_SYS_RIO_BANK1_OFFSET, RP1_SYS_RIO_BANK2_OFFSET},
// };

// const int GPIOBUS_RPi_rp1::rp1_bank_base[] = {0, 28, 34};



// // static const char *GPIOBUS_RPi_rp1::rp1_gpio_fsel_names[GPIOBUS_RPi_rp1::RP1_NUM_GPIOS][GPIOBUS_RPi_rp1::RP1_FSEL_COUNT] =
// // {
// //     { "SPI0_SIO3" , "DPI_PCLK"     , "TXD1"         , "SDA0"         , 0              , "SYS_RIO00" , "PROC_RIO00" , "PIO0"       , "SPI2_CE0" , },
// //     { "SPI0_SIO2" , "DPI_DE"       , "RXD1"         , "SCL0"         , 0              , "SYS_RIO01" , "PROC_RIO01" , "PIO1"       , "SPI2_SIO1", },
// //     { "SPI0_CE3"  , "DPI_VSYNC"    , "CTS1"         , "SDA1"         , "IR_RX0"       , "SYS_RIO02" , "PROC_RIO02" , "PIO2"       , "SPI2_SIO0", },
// //     { "SPI0_CE2"  , "DPI_HSYNC"    , "RTS1"         , "SCL1"         , "IR_TX0"       , "SYS_RIO03" , "PROC_RIO03" , "PIO3"       , "SPI2_SCLK", },
// //     { "GPCLK0"    , "DPI_D0"       , "TXD2"         , "SDA2"         , "RI0"          , "SYS_RIO04" , "PROC_RIO04" , "PIO4"       , "SPI3_CE0" , },
// //     { "GPCLK1"    , "DPI_D1"       , "RXD2"         , "SCL2"         , "DTR0"         , "SYS_RIO05" , "PROC_RIO05" , "PIO5"       , "SPI3_SIO1", },
// //     { "GPCLK2"    , "DPI_D2"       , "CTS2"         , "SDA3"         , "DCD0"         , "SYS_RIO06" , "PROC_RIO06" , "PIO6"       , "SPI3_SIO0", },
// //     { "SPI0_CE1"  , "DPI_D3"       , "RTS2"         , "SCL3"         , "DSR0"         , "SYS_RIO07" , "PROC_RIO07" , "PIO7"       , "SPI3_SCLK", },
// //     { "SPI0_CE0"  , "DPI_D4"       , "TXD3"         , "SDA0"         , 0              , "SYS_RIO08" , "PROC_RIO08" , "PIO8"       , "SPI4_CE0" , },
// //     { "SPI0_MISO" , "DPI_D5"       , "RXD3"         , "SCL0"         , 0              , "SYS_RIO09" , "PROC_RIO09" , "PIO9"       , "SPI4_SIO0", },
// //     { "SPI0_MOSI" , "DPI_D6"       , "CTS3"         , "SDA1"         , 0              , "SYS_RIO010", "PROC_RIO010", "PIO10"      , "SPI4_SIO1", },
// //     { "SPI0_SCLK" , "DPI_D7"       , "RTS3"         , "SCL1"         , 0              , "SYS_RIO011", "PROC_RIO011", "PIO11"      , "SPI4_SCLK", },
// //     { "PWM0_CHAN0", "DPI_D8"       , "TXD4"         , "SDA2"         , "AAUD_LEFT"    , "SYS_RIO012", "PROC_RIO012", "PIO12"      , "SPI5_CE0" , },
// //     { "PWM0_CHAN1", "DPI_D9"       , "RXD4"         , "SCL2"         , "AAUD_RIGHT"   , "SYS_RIO013", "PROC_RIO013", "PIO13"      , "SPI5_SIO1", },
// //     { "PWM0_CHAN2", "DPI_D10"      , "CTS4"         , "SDA3"         , "TXD0"         , "SYS_RIO014", "PROC_RIO014", "PIO14"      , "SPI5_SIO0", },
// //     { "PWM0_CHAN3", "DPI_D11"      , "RTS4"         , "SCL3"         , "RXD0"         , "SYS_RIO015", "PROC_RIO015", "PIO15"      , "SPI5_SCLK", },
// //     { "SPI1_CE2"  , "DPI_D12"      , "DSI0_TE_EXT"  , 0              , "CTS0"         , "SYS_RIO016", "PROC_RIO016", "PIO16"      , },
// //     { "SPI1_CE1"  , "DPI_D13"      , "DSI1_TE_EXT"  , 0              , "RTS0"         , "SYS_RIO017", "PROC_RIO017", "PIO17"      , },
// //     { "SPI1_CE0"  , "DPI_D14"      , "I2S0_SCLK"    , "PWM0_CHAN2"   , "I2S1_SCLK"    , "SYS_RIO018", "PROC_RIO018", "PIO18"      , "GPCLK1",   },
// //     { "SPI1_MISO" , "DPI_D15"      , "I2S0_WS"      , "PWM0_CHAN3"   , "I2S1_WS"      , "SYS_RIO019", "PROC_RIO019", "PIO19"      , },
// //     { "SPI1_MOSI" , "DPI_D16"      , "I2S0_SDI0"    , "GPCLK0"       , "I2S1_SDI0"    , "SYS_RIO020", "PROC_RIO020", "PIO20"      , },
// //     { "SPI1_SCLK" , "DPI_D17"      , "I2S0_SDO0"    , "GPCLK1"       , "I2S1_SDO0"    , "SYS_RIO021", "PROC_RIO021", "PIO21"      , },
// //     { "SD0_CLK"   , "DPI_D18"      , "I2S0_SDI1"    , "SDA3"         , "I2S1_SDI1"    , "SYS_RIO022", "PROC_RIO022", "PIO22"      , },
// //     { "SD0_CMD"   , "DPI_D19"      , "I2S0_SDO1"    , "SCL3"         , "I2S1_SDO1"    , "SYS_RIO023", "PROC_RIO023", "PIO23"      , },
// //     { "SD0_DAT0"  , "DPI_D20"      , "I2S0_SDI2"    , 0              , "I2S1_SDI2"    , "SYS_RIO024", "PROC_RIO024", "PIO24"      , "SPI2_CE1" , },
// //     { "SD0_DAT1"  , "DPI_D21"      , "I2S0_SDO2"    , "MIC_CLK"      , "I2S1_SDO2"    , "SYS_RIO025", "PROC_RIO025", "PIO25"      , "SPI3_CE1" , },
// //     { "SD0_DAT2"  , "DPI_D22"      , "I2S0_SDI3"    , "MIC_DAT0"     , "I2S1_SDI3"    , "SYS_RIO026", "PROC_RIO026", "PIO26"      , "SPI5_CE1" , },
// //     { "SD0_DAT3"  , "DPI_D23"      , "I2S0_SDO3"    , "MIC_DAT1"     , "I2S1_SDO3"    , "SYS_RIO027", "PROC_RIO027", "PIO27"      , "SPI1_CE1" , },
// //     { "SD1_CLK"   , "SDA4"         , "I2S2_SCLK"    , "SPI6_MISO"    , "VBUS_EN0"     , "SYS_RIO10" , "PROC_RIO10" , },
// //     { "SD1_CMD"   , "SCL4"         , "I2S2_WS"      , "SPI6_MOSI"    , "VBUS_OC0"     , "SYS_RIO11" , "PROC_RIO11" , },
// //     { "SD1_DAT0"  , "SDA5"         , "I2S2_SDI0"    , "SPI6_SCLK"    , "TXD5"         , "SYS_RIO12" , "PROC_RIO12" , },
// //     { "SD1_DAT1"  , "SCL5"         , "I2S2_SDO0"    , "SPI6_CE0"     , "RXD5"         , "SYS_RIO13" , "PROC_RIO13" , },
// //     { "SD1_DAT2"  , "GPCLK3"       , "I2S2_SDI1"    , "SPI6_CE1"     , "CTS5"         , "SYS_RIO14" , "PROC_RIO14" , },
// //     { "SD1_DAT3"  , "GPCLK4"       , "I2S2_SDO1"    , "SPI6_CE2"     , "RTS5"         , "SYS_RIO15" , "PROC_RIO15" , },
// //     { "PWM1_CHAN2", "GPCLK3"       , "VBUS_EN0"     , "SDA4"         , "MIC_CLK"      , "SYS_RIO20" , "PROC_RIO20" , },
// //     { "SPI8_CE1"  , "PWM1_CHAN0"   , "VBUS_OC0"     , "SCL4"         , "MIC_DAT0"     , "SYS_RIO21" , "PROC_RIO21" , },
// //     { "SPI8_CE0"  , "TXD5"         , "PCIE_CLKREQ_N", "SDA5"         , "MIC_DAT1"     , "SYS_RIO22" , "PROC_RIO22" , },
// //     { "SPI8_MISO" , "RXD5"         , "MIC_CLK"      , "SCL5"         , "PCIE_CLKREQ_N", "SYS_RIO23" , "PROC_RIO23" , },
// //     { "SPI8_MOSI" , "RTS5"         , "MIC_DAT0"     , "SDA6"         , "AAUD_LEFT"    , "SYS_RIO24" , "PROC_RIO24" , "DSI0_TE_EXT", },
// //     { "SPI8_SCLK" , "CTS5"         , "MIC_DAT1"     , "SCL6"         , "AAUD_RIGHT"   , "SYS_RIO25" , "PROC_RIO25" , "DSI1_TE_EXT", },
// //     { "PWM1_CHAN1", "TXD5"         , "SDA4"         , "SPI6_MISO"    , "AAUD_LEFT"    , "SYS_RIO26" , "PROC_RIO26" , },
// //     { "PWM1_CHAN2", "RXD5"         , "SCL4"         , "SPI6_MOSI"    , "AAUD_RIGHT"   , "SYS_RIO27" , "PROC_RIO27" , },
// //     { "GPCLK5"    , "RTS5"         , "VBUS_EN1"     , "SPI6_SCLK"    , "I2S2_SCLK"    , "SYS_RIO28" , "PROC_RIO28" , },
// //     { "GPCLK4"    , "CTS5"         , "VBUS_OC1"     , "SPI6_CE0"     , "I2S2_WS"      , "SYS_RIO29" , "PROC_RIO29" , },
// //     { "GPCLK5"    , "SDA5"         , "PWM1_CHAN0"   , "SPI6_CE1"     , "I2S2_SDI0"    , "SYS_RIO210", "PROC_RIO210", },
// //     { "PWM1_CHAN3", "SCL5"         , "SPI7_CE0"     , "SPI6_CE2"     , "I2S2_SDO0"    , "SYS_RIO211", "PROC_RIO211", },
// //     { "GPCLK3"    , "SDA4"         , "SPI7_MOSI"    , "MIC_CLK"      , "I2S2_SDI1"    , "SYS_RIO212", "PROC_RIO212", "DSI0_TE_EXT", },
// //     { "GPCLK5"    , "SCL4"         , "SPI7_MISO"    , "MIC_DAT0"     , "I2S2_SDO1"    , "SYS_RIO213", "PROC_RIO213", "DSI1_TE_EXT", },
// //     { "PWM1_CHAN0", "PCIE_CLKREQ_N", "SPI7_SCLK"    , "MIC_DAT1"     , "TXD5"         , "SYS_RIO214", "PROC_RIO214", },
// //     { "SPI8_SCLK" , "SPI7_SCLK"    , "SDA5"         , "AAUD_LEFT"    , "RXD5"         , "SYS_RIO215", "PROC_RIO215", },
// //     { "SPI8_MISO" , "SPI7_MOSI"    , "SCL5"         , "AAUD_RIGHT"   , "VBUS_EN2"     , "SYS_RIO216", "PROC_RIO216", },
// //     { "SPI8_MOSI" , "SPI7_MISO"    , "SDA6"         , "AAUD_LEFT"    , "VBUS_OC2"     , "SYS_RIO217", "PROC_RIO217", },
// //     { "SPI8_CE0"  , 0              , "SCL6"         , "AAUD_RIGHT"   , "VBUS_EN3"     , "SYS_RIO218", "PROC_RIO218", },
// //     { "SPI8_CE1"  , "SPI7_CE0"     , 0              , "PCIE_CLKREQ_N", "VBUS_OC3"     , "SYS_RIO219", "PROC_RIO219", },
// // };

// /* static */ void GPIOBUS_RPi_rp1::rp1_gpio_get_bank(int num, int *bank, int *offset)
// {
//     *bank = *offset = 0;
//     if (num >= RP1_NUM_GPIOS)
//     {
//         assert(0);
//         return;
//     }

//     if (num < rp1_bank_base[1])
//         *bank = 0;
//     else if (num < rp1_bank_base[2])
//         *bank = 1;
//     else
//         *bank = 2;

//    *offset = num - rp1_bank_base[*bank];
// }

// /* static */ uint32_t GPIOBUS_RPi_rp1::rp1_gpio_ctrl_read(volatile uint32_t *base, int bank, int offset)
// {
//     return rp1_gpio_read32(base, gpio_state.io[bank], RP1_GPIO_IO_REG_CTRL_OFFSET(offset));
// }

// /* static */ void GPIOBUS_RPi_rp1::rp1_gpio_ctrl_write(volatile uint32_t *base, int bank, int offset,
//                                 uint32_t value)
// {
//     rp1_gpio_write32(base, gpio_state.io[bank], RP1_GPIO_IO_REG_CTRL_OFFSET(offset), value);
// }

// /* static */ uint32_t GPIOBUS_RPi_rp1::rp1_gpio_pads_read(volatile uint32_t *base, int bank, int offset)
// {
//     return rp1_gpio_read32(base, gpio_state.pads[bank], RP1_GPIO_PADS_REG_OFFSET(offset));
// }

// /* static */ void GPIOBUS_RPi_rp1::rp1_gpio_pads_write(volatile uint32_t *base, int bank, int offset,
//                                 uint32_t value)
// {
//     rp1_gpio_write32(base, gpio_state.pads[bank], RP1_GPIO_PADS_REG_OFFSET(offset), value);
// }

// /* static */ uint32_t GPIOBUS_RPi_rp1::rp1_gpio_sys_rio_out_read(volatile uint32_t *base, int bank,
//                                           int offset)
// {
//     UNUSED(offset);
//     return rp1_gpio_read32(base, gpio_state.sys_rio[bank], RP1_GPIO_SYS_RIO_REG_OUT_OFFSET);
// }

// /* static */ uint32_t GPIOBUS_RPi_rp1::rp1_gpio_sys_rio_sync_in_read(volatile uint32_t *base, int bank,
//                                               int offset)
// {
//     UNUSED(offset);
//     return rp1_gpio_read32(base, gpio_state.sys_rio[bank],
//                            RP1_GPIO_SYS_RIO_REG_SYNC_IN_OFFSET);
// }

// /* static */ void GPIOBUS_RPi_rp1::rp1_gpio_sys_rio_out_set(volatile uint32_t *base, int bank, int offset)
// {
//     rp1_gpio_write32(base, gpio_state.sys_rio[bank],
//                      RP1_GPIO_SYS_RIO_REG_OUT_OFFSET + RP1_SET_OFFSET, 1U << offset);
// }

// /* static */ void GPIOBUS_RPi_rp1::rp1_gpio_sys_rio_out_clr(volatile uint32_t *base, int bank, int offset)
// {
//     rp1_gpio_write32(base, gpio_state.sys_rio[bank],
//                      RP1_GPIO_SYS_RIO_REG_OUT_OFFSET + RP1_CLR_OFFSET, 1U << offset);
// }

// /* static */ uint32_t GPIOBUS_RPi_rp1::rp1_gpio_sys_rio_oe_read(volatile uint32_t *base, int bank)
// {
//     return rp1_gpio_read32(base, gpio_state.sys_rio[bank],
//                            RP1_GPIO_SYS_RIO_REG_OE_OFFSET);
// }

// /* static */ void GPIOBUS_RPi_rp1::rp1_gpio_sys_rio_oe_clr(volatile uint32_t *base, int bank, int offset)
// {
//     rp1_gpio_write32(base, gpio_state.sys_rio[bank],
//                      RP1_GPIO_SYS_RIO_REG_OE_OFFSET + RP1_CLR_OFFSET,
//                      1U << offset);
// }

// /* static */ void GPIOBUS_RPi_rp1::rp1_gpio_sys_rio_oe_set(volatile uint32_t *base, int bank, int offset)
// {
//     rp1_gpio_write32(base, gpio_state.sys_rio[bank],
//                      RP1_GPIO_SYS_RIO_REG_OE_OFFSET + RP1_SET_OFFSET,
//                      1U << offset);
// }

// /* static */ void GPIOBUS_RPi_rp1::rp1_gpio_set_dir(void *priv, uint32_t gpio, GPIO_DIR_T dir)
// {
//     volatile uint32_t *base = (volatile uint32_t*)priv;
//     int bank, offset;

//     rp1_gpio_get_bank(gpio, &bank, &offset);

//     if (dir == DIR_INPUT)
//         rp1_gpio_sys_rio_oe_clr(base, bank, offset);
//     else if (dir == DIR_OUTPUT)
//         rp1_gpio_sys_rio_oe_set(base, bank, offset);
//     else
//         assert(0);
// }

// /* static */ GPIOBUS_RPi_rp1::GPIO_DIR_T GPIOBUS_RPi_rp1::rp1_gpio_get_dir(void *priv, unsigned gpio)
// {
//     volatile uint32_t *base = (volatile uint32_t*)priv;
//     int bank, offset;
//     GPIO_DIR_T dir;
//     uint32_t reg;

//     rp1_gpio_get_bank(gpio, &bank, &offset);
//     reg = rp1_gpio_sys_rio_oe_read(base, bank);

//     dir = (reg & (1U << offset)) ? DIR_OUTPUT : DIR_INPUT;

//     return dir;
// }

// /* static */ GPIOBUS_RPi_rp1::GPIO_FSEL_T GPIOBUS_RPi_rp1::rp1_gpio_get_fsel(void *priv, unsigned gpio)
// {
//     volatile uint32_t *base = (volatile uint32_t *)priv;
//     int bank, offset;
//     uint32_t reg;
//     GPIO_FSEL_T fsel;
//     RP1_FSEL_T rsel;

//     rp1_gpio_get_bank(gpio, &bank, &offset);
//     reg = rp1_gpio_ctrl_read(base, bank, offset);
//     rsel = ((reg & RP1_GPIO_CTRL_FSEL_MASK) >> RP1_GPIO_CTRL_FSEL_LSB);
//     if (rsel == RP1_FSEL_SYS_RIO)
//         fsel = GPIO_FSEL_GPIO;
//     else if (rsel == RP1_FSEL_NULL)
//         fsel = GPIO_FSEL_NONE;
//     else if (rsel < RP1_FSEL_COUNT)
//         fsel = (GPIO_FSEL_T)rsel;
//     else
//         fsel = GPIO_FSEL_MAX;

//     return fsel;
// }

// /* static */ void GPIOBUS_RPi_rp1::rp1_gpio_set_fsel(void *priv, unsigned gpio, const GPIO_FSEL_T func)
// {
//     volatile uint32_t *base = (volatile uint32_t*)priv;
//     int bank, offset;
//     uint32_t ctrl_reg;
//     uint32_t pad_reg;
//     uint32_t old_pad_reg;
//     RP1_FSEL_T rsel;

//     if (func < (GPIO_FSEL_T)RP1_FSEL_COUNT)
//         rsel = (RP1_FSEL_T)func;
//     else if (func == GPIO_FSEL_INPUT ||
//              func == GPIO_FSEL_OUTPUT ||
//              func == GPIO_FSEL_GPIO)
//         rsel = RP1_FSEL_SYS_RIO;
//     else if (func == GPIO_FSEL_NONE)
//         rsel = RP1_FSEL_NULL;
//     else
//         return;

//     rp1_gpio_get_bank(gpio, &bank, &offset);
//     if (func == GPIO_FSEL_INPUT)
//         rp1_gpio_set_dir(priv, gpio, DIR_INPUT);
//     else if (func == GPIO_FSEL_OUTPUT)
//         rp1_gpio_set_dir(priv, gpio, DIR_OUTPUT);

//     ctrl_reg = rp1_gpio_ctrl_read(base, bank, offset) & ~RP1_GPIO_CTRL_FSEL_MASK;
//     ctrl_reg |= rsel << RP1_GPIO_CTRL_FSEL_LSB;
//     rp1_gpio_ctrl_write(base, bank, offset, ctrl_reg);

//     pad_reg = rp1_gpio_pads_read(base, bank, offset);
//     old_pad_reg = pad_reg;
//     if (rsel == RP1_FSEL_NULL)
//     {
//         // Disable input
//         pad_reg &= ~RP1_PADS_IE_SET;
//     }
//     else
//     {
//         // Enable input
//         pad_reg |= RP1_PADS_IE_SET;
//     }

//     if (rsel != RP1_FSEL_NULL)
//     {
//         // Enable peripheral func output
//         pad_reg &= ~RP1_PADS_OD_SET;
//     }
//     else
//     {
//         // Disable peripheral func output
//         pad_reg |= RP1_PADS_OD_SET;
//     }

//     if (pad_reg != old_pad_reg)
//         rp1_gpio_pads_write(base, bank, offset, pad_reg);
// }

// /* static */ int GPIOBUS_RPi_rp1::rp1_gpio_get_level(void *priv, unsigned gpio)
// {
//     volatile uint32_t *base = (volatile uint32_t *)priv;
//     int bank, offset;
//     uint32_t pad_reg;
//     uint32_t reg;
//     int level;

//     rp1_gpio_get_bank(gpio, &bank, &offset);
//     pad_reg = rp1_gpio_pads_read(base, bank, offset);
//     if (!(pad_reg & RP1_PADS_IE_SET))
// 	return -1;
//     reg = rp1_gpio_sys_rio_sync_in_read(base, bank, offset);
//     level = (reg & (1U << offset)) ? 1 : 0;

//     return level;
// }

// /* static */ void GPIOBUS_RPi_rp1::rp1_gpio_set_drive(void *priv, unsigned gpio, GPIO_DRIVE_T drv)
// {
//     volatile uint32_t *base = (volatile uint32_t*)priv;
//     int bank, offset;

//     rp1_gpio_get_bank(gpio, &bank, &offset);
//     if (drv == DRIVE_HIGH)
//         rp1_gpio_sys_rio_out_set(base, bank, offset);
//     else if (drv == DRIVE_LOW)
//         rp1_gpio_sys_rio_out_clr(base, bank, offset);
// }

// /* static */ void GPIOBUS_RPi_rp1::rp1_gpio_set_pull(void *priv, unsigned gpio, GPIO_PULL_T pull)
// {
//     volatile uint32_t *base = (volatile uint32_t*)priv;
//     uint32_t reg;
//     int bank, offset;

//     rp1_gpio_get_bank(gpio, &bank, &offset);
//     reg = rp1_gpio_pads_read(base, bank, offset);
//     reg &= ~(RP1_PADS_PDE_SET | RP1_PADS_PUE_SET);
//     if (pull == PULL_UP)
//         reg |= RP1_PADS_PUE_SET;
//     else if (pull == PULL_DOWN)
//         reg |= RP1_PADS_PDE_SET;
//     rp1_gpio_pads_write(base, bank, offset, reg);
// }

// /* static */ GPIOBUS_RPi_rp1::GPIO_PULL_T GPIOBUS_RPi_rp1::rp1_gpio_get_pull(void *priv, unsigned gpio)
// {
//     volatile uint32_t *base = (volatile uint32_t*)priv;
//     uint32_t reg;
//     GPIO_PULL_T pull = PULL_NONE;
//     int bank, offset;

//     rp1_gpio_get_bank(gpio, &bank, &offset);
//     reg = rp1_gpio_pads_read(base, bank, offset);
//     if (reg & RP1_PADS_PUE_SET)
//         pull = PULL_UP;
//     else if (reg & RP1_PADS_PDE_SET)
//         pull = PULL_DOWN;

//     return pull;
// }

// /* static */ GPIOBUS_RPi_rp1::GPIO_DRIVE_T GPIOBUS_RPi_rp1::rp1_gpio_get_drive(void *priv, unsigned gpio)
// {
//     volatile uint32_t *base = (volatile uint32_t*)priv;
//     uint32_t reg;
//     int bank, offset;

//     rp1_gpio_get_bank(gpio, &bank, &offset);
//     reg = rp1_gpio_sys_rio_out_read(base, bank, offset);
//     return (reg & (1U << offset)) ? DRIVE_HIGH : DRIVE_LOW;
// }

// /* static */ const char *GPIOBUS_RPi_rp1::rp1_gpio_get_name(void *priv, unsigned gpio)
// {
//     // /* static */ char name_buf[16];
//     // UNUSED(priv);

//     // if (gpio >= RP1_NUM_GPIOS)
//     //     return NULL;

//     // sprintf(name_buf, "GPIO%d", gpio);
//     // return name_buf;
//     return nullptr;
// }

// /* static */ const char *GPIOBUS_RPi_rp1::rp1_gpio_get_fsel_name(void *priv, unsigned gpio, GPIO_FSEL_T fsel)
// {
//     const char *name = NULL;
//     UNUSED(priv);
//     return "unknown";
//     // switch (fsel)
//     // {
//     // case GPIO_FSEL_GPIO:
//     //     name = "gpio";
//     //     break;
//     // case GPIO_FSEL_INPUT:
//     //     name = "input";
//     //     break;
//     // case GPIO_FSEL_OUTPUT:
//     //     name = "output";
//     //     break;
//     // case GPIO_FSEL_NONE:
//     //     name = "none";
//     //     break;
//     // case GPIO_FSEL_FUNC0:
//     // case GPIO_FSEL_FUNC1:
//     // case GPIO_FSEL_FUNC2:
//     // case GPIO_FSEL_FUNC3:
//     // case GPIO_FSEL_FUNC4:
//     // case GPIO_FSEL_FUNC5:
//     // case GPIO_FSEL_FUNC6:
//     // case GPIO_FSEL_FUNC7:
//     // case GPIO_FSEL_FUNC8:
//     //     if (gpio < RP1_NUM_GPIOS)
//     //     {
//     //         name = rp1_gpio_fsel_names[gpio][fsel - GPIO_FSEL_FUNC0];
//     //         if (!name)
//     //             name = "-";
//     //     }
//     //     break;
//     // default:
//     //     return NULL;
//     // }
//     // return name;
// }

// /* static */void *GPIOBUS_RPi_rp1::rp1_gpio_create_instance(const GPIO_CHIP_T *chip,
//                                       const char *dtnode)
// {
//     UNUSED(dtnode);
//     return (void *)chip;
// }

// /* static */ int GPIOBUS_RPi_rp1::rp1_gpio_count(void *priv)
// {
//     UNUSED(priv);
//     return RP1_NUM_GPIOS;
// }

// /* static */ void *GPIOBUS_RPi_rp1::rp1_gpio_probe_instance(void *priv, volatile uint32_t *base)
// {
//     UNUSED(priv);
//     return (void *)base;
// }












// // //---------------------------------------------------------------------------
// // //
// // //	imported from bcm_host.c
// // //
// // //---------------------------------------------------------------------------
// // uint32_t GPIOBUS_RPi_rp1::get_dt_ranges(const char *filename, uint32_t offset)
// // {
// //     GPIO_FUNCTION_TRACE
// //     uint32_t address = ~0;
// //     if (FILE *fp = fopen(filename, "rb"); fp) {
// //         fseek(fp, offset, SEEK_SET);
// //         if (array<uint8_t, 4> buf; fread(buf.data(), 1, buf.size(), fp) == buf.size()) {
// //             address = (int)buf[0] << 24 | (int)buf[1] << 16 | (int)buf[2] << 8 | (int)buf[3] << 0;
// //         }
// //         fclose(fp);
// //     }
// //     return address;
// // }

// // uint32_t GPIOBUS_RPi_rp1::bcm_host_get_peripheral_address()
// // {
// //     GPIO_FUNCTION_TRACE
// // #ifdef __linux__
// //     uint32_t address = get_dt_ranges("/proc/device-tree/soc/ranges", 4);
// //     if (address == 0) {
// //         address = get_dt_ranges("/proc/device-tree/soc/ranges", 8);
// //     }
// //     address = (address == (uint32_t)~0) ? 0x20000000 : address;
// //     return address;
// // #else
// //     return 0;
// // #endif
// // }

// bool GPIOBUS_RPi_rp1::Init(mode_e mode)
// {
//     GPIOBUS::Init(mode);

//     return true;
// // #if defined(__x86_64__) || defined(__X86__)
// //     (void)baseaddr;
// //     level = new uint32_t();
// //     return true;
// // #else
// //     int i;
// // #ifdef USE_SEL_EVENT_ENABLE
// //     epoll_event ev = {};
// // #endif

// //     // Get the base address
// //     baseaddr = (uint32_t)bcm_host_get_peripheral_address();

// //     // Open /dev/mem
// //     int fd = open("/dev/mem", O_RDWR | O_SYNC);
// //     if (fd == -1) {
// //         spdlog::error("Error: Unable to open /dev/mem. Are you running as root?");
// //         return false;
// //     }

// //     // Map peripheral region memory
// //     void *map = mmap(NULL, 0x1000100, PROT_READ | PROT_WRITE, MAP_SHARED, fd, baseaddr);
// //     if (map == MAP_FAILED) {
// //         spdlog::error("Error: Unable to map memory: "+ string(strerror(errno)));
// //         close(fd);
// //         return false;
// //     }

// //     // Determine the type of raspberry pi from the base address
// //     if (baseaddr == 0xfe000000) {
// //         rpitype = 4;
// //     } else if (baseaddr == 0x3f000000) {
// //         rpitype = 2;
// //     } else {
// //         rpitype = 1;
// //     }

// //     // GPIO
// //     gpio = (uint32_t *)map;
// //     gpio += GPIO_OFFSET / sizeof(uint32_t);
// //     level = &gpio[GPIO_LEV_0];

// //     // PADS
// //     pads = (uint32_t *)map;
// //     pads += PADS_OFFSET / sizeof(uint32_t);

// //     // System timer
// //     SysTimer::Init();

// //     // Interrupt controller
// //     irpctl = (uint32_t *)map;
// //     irpctl += IRPT_OFFSET / sizeof(uint32_t);

// //     // Quad-A7 control
// //     qa7regs = (uint32_t *)map;
// //     qa7regs += QA7_OFFSET / sizeof(uint32_t);

// //     // Map GIC memory
// //     if (rpitype == 4) {
// //         map = mmap(NULL, 8192, PROT_READ | PROT_WRITE, MAP_SHARED, fd, ARM_GICD_BASE);
// //         if (map == MAP_FAILED) {
// //             close(fd);
// //             return false;
// //         }
// //         gicd = (uint32_t *)map;
// //         gicc = (uint32_t *)map;
// //         gicc += (ARM_GICC_BASE - ARM_GICD_BASE) / sizeof(uint32_t);
// //     } else {
// //         gicd = NULL;
// //         gicc = NULL;
// //     }
// //     close(fd);

// //     // Set Drive Strength to 16mA
// //     DrvConfig(7);

// //     // Set pull up/pull down
// // #if SIGNAL_CONTROL_MODE == 0
// //     int pullmode = GPIO_PULLNONE;
// // #elif SIGNAL_CONTROL_MODE == 1
// //     int pullmode = GPIO_PULLUP;
// // #else
// //     int pullmode = GPIO_PULLDOWN;
// // #endif

// //     // Initialize all signals
// //     for (i = 0; SignalTable[i] >= 0; i++) {
// //         int j = SignalTable[i];
// //         PinSetSignal(j, OFF);
// //         PinConfig(j, GPIO_INPUT);
// //         PullConfig(j, pullmode);
// //     }

// //     // Set control signals
// //     PinSetSignal(PIN_ACT, OFF);
// //     PinSetSignal(PIN_TAD, OFF);
// //     PinSetSignal(PIN_IND, OFF);
// //     PinSetSignal(PIN_DTD, OFF);
// //     PinConfig(PIN_ACT, GPIO_OUTPUT);
// //     PinConfig(PIN_TAD, GPIO_OUTPUT);
// //     PinConfig(PIN_IND, GPIO_OUTPUT);
// //     PinConfig(PIN_DTD, GPIO_OUTPUT);

// //     // Set the ENABLE signal
// //     // This is used to show that the application is running
// //     PinSetSignal(PIN_ENB, ENB_OFF);
// //     PinConfig(PIN_ENB, GPIO_OUTPUT);

// //     // GPIO Function Select (GPFSEL) registers backup
// //     gpfsel[0] = gpio[GPIO_FSEL_0];
// //     gpfsel[1] = gpio[GPIO_FSEL_1];
// //     gpfsel[2] = gpio[GPIO_FSEL_2];
// //     gpfsel[3] = gpio[GPIO_FSEL_3];

// //     // Initialize SEL signal interrupt
// // #ifdef USE_SEL_EVENT_ENABLE
// //     // GPIO chip open
// //     fd = open("/dev/gpiochip0", 0);
// //     if (fd == -1) {
// //         spdlog::error("Unable to open /dev/gpiochip0. If PiSCSI is running, please shut it down first.");
// //         return false;
// //     }

// //     // Event request setting
// //     strcpy(selevreq.consumer_label, "PiSCSI");
// //     selevreq.lineoffset  = PIN_SEL;
// //     selevreq.handleflags = GPIOHANDLE_REQUEST_INPUT;
// // #if SIGNAL_CONTROL_MODE < 2
// //     selevreq.eventflags  = GPIOEVENT_REQUEST_FALLING_EDGE;
// // #else
// //     selevreq.eventflags = GPIOEVENT_REQUEST_RISING_EDGE;
// // #endif // SIGNAL_CONTROL_MODE

// //     // Get event request
// //     if (ioctl(fd, GPIO_GET_LINEEVENT_IOCTL, &selevreq) == -1) {
// //         spdlog::error("Unable to register event request. If PiSCSI is running, please shut it down first.");
// //         close(fd);
// //         return false;
// //     }

// //     // Close GPIO chip file handle
// //     close(fd);

// //     // epoll initialization
// //     epfd       = epoll_create(1);
// //     ev.events  = EPOLLIN | EPOLLPRI;
// //     ev.data.fd = selevreq.fd;
// //     epoll_ctl(epfd, EPOLL_CTL_ADD, selevreq.fd, &ev);
// // #else
// //     // Edge detection setting
// // #if SIGNAL_CONTROL_MODE == 2
// //     gpio[GPIO_AREN_0] = 1 << PIN_SEL;
// // #else
// //     gpio[GPIO_AFEN_0] = 1 << PIN_SEL;
// // #endif // SIGNAL_CONTROL_MODE

// //     // Clear event - GPIO Pin Event Detect Status
// //     gpio[GPIO_EDS_0] = 1 << PIN_SEL;

// //     // Register interrupt handler
// //     setIrqFuncAddress(IrqHandler);

// //     // GPIO interrupt setting
// //     if (rpitype == 4) {
// //         // GIC Invalid
// //         gicd[GICD_CTLR] = 0;

// //         // Route all interupts to core 0
// //         for (i = 0; i < 8; i++) {
// //             gicd[GICD_ICENABLER0 + i] = 0xffffffff;
// //             gicd[GICD_ICPENDR0 + i]   = 0xffffffff;
// //             gicd[GICD_ICACTIVER0 + i] = 0xffffffff;
// //         }
// //         for (i = 0; i < 64; i++) {
// //             gicd[GICD_IPRIORITYR0 + i] = 0xa0a0a0a0;
// //             gicd[GICD_ITARGETSR0 + i]  = 0x01010101;
// //         }

// //         // Set all interrupts as level triggers
// //         for (i = 0; i < 16; i++) {
// //             gicd[GICD_ICFGR0 + i] = 0;
// //         }

// //         // GIC Invalid
// //         gicd[GICD_CTLR] = 1;

// //         // Enable CPU interface for core 0
// //         gicc[GICC_PMR]  = 0xf0;
// //         gicc[GICC_CTLR] = 1;

// //         // Enable interrupts
// //         gicd[GICD_ISENABLER0 + (GIC_GPIO_IRQ / 32)] = 1 << (GIC_GPIO_IRQ % 32);
// //     } else {
// //         // Enable interrupts
// //         irpctl[IRPT_ENB_IRQ_2] = (1 << (GPIO_IRQ % 32));
// //     }
// // #endif // USE_SEL_EVENT_ENABLE

// //     // Create work table
// //     MakeTable();

// //     // Finally, enable ENABLE
// //     // Show the user that this app is running
// //     SetControl(PIN_ENB, ENB_ON);

// //     return true;
// // #endif // ifdef __x86_64__ || __X86__
// }

// void GPIOBUS_RPi_rp1::Cleanup()
// {
// #if defined(__x86_64__) || defined(__X86__)
//     return;
// #else
//     // Release SEL signal interrupt
// #ifdef USE_SEL_EVENT_ENABLE
//     close(selevreq.fd);
// #endif // USE_SEL_EVENT_ENABLE

//     // // Set control signals
//     // PinSetSignal(PIN_ENB, OFF);
//     // PinSetSignal(PIN_ACT, OFF);
//     // PinSetSignal(PIN_TAD, OFF);
//     // PinSetSignal(PIN_IND, OFF);
//     // PinSetSignal(PIN_DTD, OFF);
//     // PinConfig(PIN_ACT, GPIO_INPUT);
//     // PinConfig(PIN_TAD, GPIO_INPUT);
//     // PinConfig(PIN_IND, GPIO_INPUT);
//     // PinConfig(PIN_DTD, GPIO_INPUT);

//     // // Initialize all signals
//     // for (int i = 0; SignalTable[i] >= 0; i++) {
//     //     int pin = SignalTable[i];
//     //     PinSetSignal(pin, OFF);
//     //     PinConfig(pin, GPIO_INPUT);
//     //     PullConfig(pin, GPIO_PULLNONE);
//     // }

//     // // Set drive strength back to 8mA
//     // DrvConfig(3);
// #endif // ifdef __x86_64__ || __X86__
// }

// void GPIOBUS_RPi_rp1::Reset()
// {
// // #if defined(__x86_64__) || defined(__X86__)
// #if 1
//     return;
// #else
//     int i;
//     int j;

//     // Turn off active signal
//     SetControl(PIN_ACT, ACT_OFF);

//     // Set all signals to off
//     for (i = 0;; i++) {
//         j = SignalTable[i];
//         if (j < 0) {
//             break;
//         }

//         SetSignal(j, OFF);
//     }

//     if (actmode == mode_e::TARGET) {
//         // Target mode

//         // Set target signal to input
//         SetControl(PIN_TAD, TAD_IN);
//         SetMode(PIN_BSY, IN);
//         SetMode(PIN_MSG, IN);
//         SetMode(PIN_CD, IN);
//         SetMode(PIN_REQ, IN);
//         SetMode(PIN_IO, IN);

//         // Set the initiator signal to input
//         SetControl(PIN_IND, IND_IN);
//         SetMode(PIN_SEL, IN);
//         SetMode(PIN_ATN, IN);
//         SetMode(PIN_ACK, IN);
//         SetMode(PIN_RST, IN);

//         // Set data bus signals to input
//         SetControl(PIN_DTD, DTD_IN);
//         SetMode(PIN_DT0, IN);
//         SetMode(PIN_DT1, IN);
//         SetMode(PIN_DT2, IN);
//         SetMode(PIN_DT3, IN);
//         SetMode(PIN_DT4, IN);
//         SetMode(PIN_DT5, IN);
//         SetMode(PIN_DT6, IN);
//         SetMode(PIN_DT7, IN);
//         SetMode(PIN_DP, IN);
//     } else {
//         // Initiator mode

//         // Set target signal to input
//         SetControl(PIN_TAD, TAD_IN);
//         SetMode(PIN_BSY, IN);
//         SetMode(PIN_MSG, IN);
//         SetMode(PIN_CD, IN);
//         SetMode(PIN_REQ, IN);
//         SetMode(PIN_IO, IN);

//         // Set the initiator signal to output
//         SetControl(PIN_IND, IND_OUT);
//         SetMode(PIN_SEL, OUT);
//         SetMode(PIN_ATN, OUT);
//         SetMode(PIN_ACK, OUT);
//         SetMode(PIN_RST, OUT);

//         // Set the data bus signals to output
//         SetControl(PIN_DTD, DTD_OUT);
//         SetMode(PIN_DT0, OUT);
//         SetMode(PIN_DT1, OUT);
//         SetMode(PIN_DT2, OUT);
//         SetMode(PIN_DT3, OUT);
//         SetMode(PIN_DT4, OUT);
//         SetMode(PIN_DT5, OUT);
//         SetMode(PIN_DT6, OUT);
//         SetMode(PIN_DT7, OUT);
//         SetMode(PIN_DP, OUT);
//     }

//     // Initialize all signals
//     signals          = 0;
// #endif // ifdef __x86_64__ || __X86__
// }

// void GPIOBUS_RPi_rp1::SetENB(bool ast)
// {
//     PinSetSignal(PIN_ENB, ast ? ENB_ON : ENB_OFF);
// }

// bool GPIOBUS_RPi_rp1::GetBSY() const
// {
//     return GetSignal(PIN_BSY);
// }

// void GPIOBUS_RPi_rp1::SetBSY(bool ast)
// {
//     // Set BSY signal
//     SetSignal(PIN_BSY, ast);

//     if (ast) {
//         // Turn on ACTIVE signal
//         SetControl(PIN_ACT, ACT_ON);

//         // Set Target signal to output
//         SetControl(PIN_TAD, TAD_OUT);

//     	SetMode(PIN_BSY, OUT);
//     	SetMode(PIN_MSG, OUT);
//     	SetMode(PIN_CD, OUT);
//     	SetMode(PIN_REQ, OUT);
//     	SetMode(PIN_IO, OUT);
//     } else {
//         // Turn off the ACTIVE signal
//         SetControl(PIN_ACT, ACT_OFF);

//         // Set the target signal to input
//     	SetControl(PIN_TAD, TAD_IN);

//     	SetMode(PIN_BSY, IN);
//     	SetMode(PIN_MSG, IN);
//     	SetMode(PIN_CD, IN);
//     	SetMode(PIN_REQ, IN);
//     	SetMode(PIN_IO, IN);
//     }
// }

// bool GPIOBUS_RPi_rp1::GetSEL() const
// {
//     return GetSignal(PIN_SEL);
// }

// void GPIOBUS_RPi_rp1::SetSEL(bool ast)
// {
//     if (actmode == mode_e::INITIATOR && ast) {
//         // Turn on ACTIVE signal
//         SetControl(PIN_ACT, ACT_ON);
//     }

//     // Set SEL signal
//     SetSignal(PIN_SEL, ast);
// }

// bool GPIOBUS_RPi_rp1::GetATN() const
// {
//     return GetSignal(PIN_ATN);
// }

// void GPIOBUS_RPi_rp1::SetATN(bool ast)
// {
//     SetSignal(PIN_ATN, ast);
// }

// bool GPIOBUS_RPi_rp1::GetACK() const
// {
//     return GetSignal(PIN_ACK);
// }

// void GPIOBUS_RPi_rp1::SetACK(bool ast)
// {
//     SetSignal(PIN_ACK, ast);
// }

// bool GPIOBUS_RPi_rp1::GetACT() const
// {
//     return GetSignal(PIN_ACT);
// }

// void GPIOBUS_RPi_rp1::SetACT(bool ast)
// {
//     SetSignal(PIN_ACT, ast);
// }

// bool GPIOBUS_RPi_rp1::GetRST() const
// {
//     return GetSignal(PIN_RST);
// }

// void GPIOBUS_RPi_rp1::SetRST(bool ast)
// {
//     SetSignal(PIN_RST, ast);
// }

// bool GPIOBUS_RPi_rp1::GetMSG() const
// {
//     return GetSignal(PIN_MSG);
// }

// void GPIOBUS_RPi_rp1::SetMSG(bool ast)
// {
//     SetSignal(PIN_MSG, ast);
// }

// bool GPIOBUS_RPi_rp1::GetCD() const
// {
//     return GetSignal(PIN_CD);
// }

// void GPIOBUS_RPi_rp1::SetCD(bool ast)
// {
//     SetSignal(PIN_CD, ast);
// }

// bool GPIOBUS_RPi_rp1::GetIO()
// {
//     bool ast = GetSignal(PIN_IO);

//     if (actmode == mode_e::INITIATOR) {
//         // Change the data input/output direction by IO signal
//         if (ast) {
//             SetControl(PIN_DTD, DTD_IN);
//             SetMode(PIN_DT0, IN);
//             SetMode(PIN_DT1, IN);
//             SetMode(PIN_DT2, IN);
//             SetMode(PIN_DT3, IN);
//             SetMode(PIN_DT4, IN);
//             SetMode(PIN_DT5, IN);
//             SetMode(PIN_DT6, IN);
//             SetMode(PIN_DT7, IN);
//             SetMode(PIN_DP, IN);
//         } else {
//             SetControl(PIN_DTD, DTD_OUT);
//             SetMode(PIN_DT0, OUT);
//             SetMode(PIN_DT1, OUT);
//             SetMode(PIN_DT2, OUT);
//             SetMode(PIN_DT3, OUT);
//             SetMode(PIN_DT4, OUT);
//             SetMode(PIN_DT5, OUT);
//             SetMode(PIN_DT6, OUT);
//             SetMode(PIN_DT7, OUT);
//             SetMode(PIN_DP, OUT);
//         }
//     }

//     return ast;
// }

// void GPIOBUS_RPi_rp1::SetIO(bool ast)
// {
//     SetSignal(PIN_IO, ast);

//     if (actmode == mode_e::TARGET) {
//         // Change the data input/output direction by IO signal
//         if (ast) {
//             SetControl(PIN_DTD, DTD_OUT);
//             SetDAT(0);
//             SetMode(PIN_DT0, OUT);
//             SetMode(PIN_DT1, OUT);
//             SetMode(PIN_DT2, OUT);
//             SetMode(PIN_DT3, OUT);
//             SetMode(PIN_DT4, OUT);
//             SetMode(PIN_DT5, OUT);
//             SetMode(PIN_DT6, OUT);
//             SetMode(PIN_DT7, OUT);
//             SetMode(PIN_DP, OUT);
//         } else {
//             SetControl(PIN_DTD, DTD_IN);
//             SetMode(PIN_DT0, IN);
//             SetMode(PIN_DT1, IN);
//             SetMode(PIN_DT2, IN);
//             SetMode(PIN_DT3, IN);
//             SetMode(PIN_DT4, IN);
//             SetMode(PIN_DT5, IN);
//             SetMode(PIN_DT6, IN);
//             SetMode(PIN_DT7, IN);
//             SetMode(PIN_DP, IN);
//         }
//     }
// }

// bool GPIOBUS_RPi_rp1::GetREQ() const
// {
//     return GetSignal(PIN_REQ);
// }

// void GPIOBUS_RPi_rp1::SetREQ(bool ast)
// {
//     SetSignal(PIN_REQ, ast);
// }

// //---------------------------------------------------------------------------
// //
// // Get data signals
// //
// //---------------------------------------------------------------------------
// uint8_t GPIOBUS_RPi_rp1::GetDAT()
// {
//     uint32_t data = Acquire();
//     data          = ((data >> (PIN_DT0 - 0)) & (1 << 0)) | ((data >> (PIN_DT1 - 1)) & (1 << 1)) |
//            ((data >> (PIN_DT2 - 2)) & (1 << 2)) | ((data >> (PIN_DT3 - 3)) & (1 << 3)) |
//            ((data >> (PIN_DT4 - 4)) & (1 << 4)) | ((data >> (PIN_DT5 - 5)) & (1 << 5)) |
//            ((data >> (PIN_DT6 - 6)) & (1 << 6)) | ((data >> (PIN_DT7 - 7)) & (1 << 7));

//     return (uint8_t)data;
// }

// void GPIOBUS_RPi_rp1::SetDAT(uint8_t dat)
// {
//     UNUSED(dat);
//     // Write to ports
// #if 0
//     uint32_t fsel = gpfsel[0];
//     fsel &= tblDatMsk[0][dat];
//     fsel |= tblDatSet[0][dat];
//     gpfsel[0] = fsel;
//     gpio[GPIO_FSEL_0] = fsel;

//     fsel = gpfsel[1];
//     fsel &= tblDatMsk[1][dat];
//     fsel |= tblDatSet[1][dat];
//     gpfsel[1] = fsel;
//     gpio[GPIO_FSEL_1] = fsel;

//     fsel = gpfsel[2];
//     fsel &= tblDatMsk[2][dat];
//     fsel |= tblDatSet[2][dat];
//     gpfsel[2] = fsel;
//     gpio[GPIO_FSEL_2] = fsel;
// #else
//     // gpio[GPIO_CLR_0] = tblDatMsk[dat];
//     // gpio[GPIO_SET_0] = tblDatSet[dat];
// #endif
// }

// // //---------------------------------------------------------------------------
// // //
// // //	Signal table
// // //
// // //---------------------------------------------------------------------------
// // const array<int, 19> GPIOBUS_RPi_rp1::SignalTable = {PIN_DT0, PIN_DT1, PIN_DT2, PIN_DT3, PIN_DT4, PIN_DT5, PIN_DT6,
// //                                                        PIN_DT7, PIN_DP,  PIN_SEL, PIN_ATN, PIN_RST, PIN_ACK, PIN_BSY,
// //                                                        PIN_MSG, PIN_CD,  PIN_IO,  PIN_REQ, -1};

// // //---------------------------------------------------------------------------
// // //
// // //	Create work table
// // //
// // //---------------------------------------------------------------------------
// // void GPIOBUS_RPi_rp1::MakeTable(void)
// // {
// //     const array<int, 9> pintbl = {PIN_DT0, PIN_DT1, PIN_DT2, PIN_DT3, PIN_DT4, PIN_DT5, PIN_DT6, PIN_DT7, PIN_DP};

// //     array<bool, 256> tblParity;

// //     // Create parity table
// //     for (uint32_t i = 0; i < 0x100; i++) {
// //         uint32_t bits   = i;
// //         uint32_t parity = 0;
// //         for (int j = 0; j < 8; j++) {
// //             parity ^= bits & 1;
// //             bits >>= 1;
// //         }
// //         parity       = ~parity;
// //         tblParity[i] = parity & 1;
// //     }

// // #if SIGNAL_CONTROL_MODE == 0
// //     // Mask and setting data generation
// //     for (auto &tbl : tblDatMsk) {
// //         tbl.fill(-1);
// //     }
// //     for (auto &tbl : tblDatSet) {
// //         tbl.fill(0);
// //     }

// //     for (uint32_t i = 0; i < 0x100; i++) {
// //         // Bit string for inspection
// //         uint32_t bits = i;

// //         // Get parity
// //         if (tblParity[i]) {
// //             bits |= (1 << 8);
// //         }

// //         // Bit check
// //         for (int j = 0; j < 9; j++) {
// //             // Index and shift amount calculation
// //             int index = pintbl[j] / 10;
// //             int shift = (pintbl[j] % 10) * 3;

// //             // Mask data
// //             tblDatMsk[index][i] &= ~(0x7 << shift);

// //             // Setting data
// //             if (bits & 1) {
// //                 tblDatSet[index][i] |= (1 << shift);
// //             }

// //             bits >>= 1;
// //         }
// //     }
// // #else
// //     for (uint32_t i = 0; i < 0x100; i++) {
// //         // Bit string for inspection
// //         uint32_t bits = i;

// //         // Get parity
// //         if (tblParity[i]) {
// //             bits |= (1 << 8);
// //         }

// // #if SIGNAL_CONTROL_MODE == 1
// //         // Negative logic is inverted
// //         bits = ~bits;
// // #endif

// //         // Create GPIO register information
// //         uint32_t gpclr = 0;
// //         uint32_t gpset = 0;
// //         for (int j = 0; j < 9; j++) {
// //             if (bits & 1) {
// //                 gpset |= (1 << pintbl[j]);
// //             } else {
// //                 gpclr |= (1 << pintbl[j]);
// //             }
// //             bits >>= 1;
// //         }

// //         tblDatMsk[i] = gpclr;
// //         tblDatSet[i] = gpset;
// //     }
// // #endif
// // }

// //---------------------------------------------------------------------------
// // //
// // //	Control signal setting
// // //
// // //---------------------------------------------------------------------------
// // void GPIOBUS_RPi_rp1::SetControl(int pin, bool ast)
// // {
// //     PinSetSignal(pin, ast);
// // }

// // //---------------------------------------------------------------------------
// // //
// // //	Input/output mode setting
// // //
// // // Set direction fo pin (IN / OUT)
// // //   Used with: TAD, BSY, MSG, CD, REQ, O, SEL, IND, ATN, ACK, RST, DT*
// // //
// // //---------------------------------------------------------------------------
// // void GPIOBUS_RPi_rp1::SetMode(int pin, int mode)
// // {
// // #if 1
// //     if (mode == OUT) {
// //         return;
// //     }
// // #endif // SIGNAL_CONTROL_MODE

// //     // int index     = pin / 10;
// //     // int shift     = (pin % 10) * 3;
// //     // uint32_t data = gpfsel[index];
// //     // data &= ~(0x7 << shift);
// //     // if (mode == OUT) {
// //     //     data |= (1 << shift);
// //     // }
// //     // gpio[index]   = data;
// //     // gpfsel[index] = data;
// // }

// // //---------------------------------------------------------------------------
// // //
// // //	Get input signal value
// // //
// // //---------------------------------------------------------------------------
// // bool GPIOBUS_RPi_rp1::GetSignal(int pin) const
// // {
// //     return (signals >> pin) & 1;
// // }

// // //---------------------------------------------------------------------------
// // //
// // //	Set output signal value
// // //
// // //  Sets the output value. Used with:
// // //     PIN_ENB, ACT, TAD, IND, DTD, BSY, SignalTable
// // //
// // //---------------------------------------------------------------------------
// // void GPIOBUS_RPi_rp1::SetSignal(int pin, bool ast)
// // {
// // #if SIGNAL_CONTROL_MODE == 0
// //     int index     = pin / 10;
// //     int shift     = (pin % 10) * 3;
// //     uint32_t data = gpfsel[index];
// //     if (ast) {
// //         data |= (1 << shift);
// //     } else {
// //         data &= ~(0x7 << shift);
// //     }
// //     gpio[index]   = data;
// //     gpfsel[index] = data;
// // #elif SIGNAL_CONTROL_MODE == 1
// //     if (ast) {
// //         gpio[GPIO_CLR_0] = 0x1 << pin;
// //     } else {
// //         gpio[GPIO_SET_0] = 0x1 << pin;
// //     }
// // #elif SIGNAL_CONTROL_MODE == 2
// //     if (ast) {
// //         gpio[GPIO_SET_0] = 0x1 << pin;
// //     } else {
// //         gpio[GPIO_CLR_0] = 0x1 << pin;
// //     }
// // #endif // SIGNAL_CONTROL_MODE
// // }

// // void GPIOBUS_RPi_rp1::DisableIRQ()
// // {
// // #ifdef __linux__
// //     if (rpitype == 4) {
// //         // RPI4 is disabled by GICC
// //         giccpmr        = gicc[GICC_PMR];
// //         gicc[GICC_PMR] = 0;
// //     } else if (rpitype == 2) {
// //         // RPI2,3 disable core timer IRQ
// //         tintcore          = sched_getcpu() + QA7_CORE0_TINTC;
// //         tintctl           = qa7regs[tintcore];
// //         qa7regs[tintcore] = 0;
// //     } else {
// //         // Stop system timer interrupt with interrupt controller
// //         irptenb                = irpctl[IRPT_ENB_IRQ_1];
// //         irpctl[IRPT_DIS_IRQ_1] = irptenb & 0xf;
// //     }
// // #else
// //     (void)0;
// // #endif
// // }

// // void GPIOBUS_RPi_rp1::EnableIRQ()
// // {
// //     if (rpitype == 4) {
// //         // RPI4 enables interrupts via the GICC
// //         gicc[GICC_PMR] = giccpmr;
// //     } else if (rpitype == 2) {
// //         // RPI2,3 re-enable core timer IRQ
// //         qa7regs[tintcore] = tintctl;
// //     } else {
// //         // Restart the system timer interrupt with the interrupt controller
// //         irpctl[IRPT_ENB_IRQ_1] = irptenb & 0xf;
// //     }
// // }

// //---------------------------------------------------------------------------
// //
// //	Pin direction setting (input/output)
// //
// // Used in Init() for ACT, TAD, IND, DTD, ENB to set direction (GPIO_OUTPUT vs GPIO_INPUT)
// // Also used on SignalTable
// // Only used in Init and Cleanup. Reset uses SetMode
// //---------------------------------------------------------------------------
// // void GPIOBUS_RPi_rp1::PinConfig(int pin, int mode)
// // {
// //     // Check for invalid pin
// //     if (pin < 0) {
// //         return;
// //     }

// //     int index     = pin / 10;
// //     uint32_t mask = ~(0x7 << ((pin % 10) * 3));
// //     gpio[index]   = (gpio[index] & mask) | ((mode & 0x7) << ((pin % 10) * 3));
// // }

// // //---------------------------------------------------------------------------
// // //
// // //	Pin pull-up/pull-down setting
// // //
// // //---------------------------------------------------------------------------
// // void GPIOBUS_RPi_rp1::PullConfig(int pin, int mode)
// // {
// //     uint32_t pull;

// //     // Check for invalid pin
// //     if (pin < 0) {
// //         return;
// //     }

// //     if (rpitype == 4) {
// //         switch (mode) {
// //         case GPIO_PULLNONE:
// //             pull = 0;
// //             break;
// //         case GPIO_PULLUP:
// //             pull = 1;
// //             break;
// //         case GPIO_PULLDOWN:
// //             pull = 2;
// //             break;
// //         default:
// //             return;
// //         }

// //         pin &= 0x1f;
// //         int shift     = (pin & 0xf) << 1;
// //         uint32_t bits = gpio[GPIO_PUPPDN0 + (pin >> 4)];
// //         bits &= ~(3 << shift);
// //         bits |= (pull << shift);
// //         gpio[GPIO_PUPPDN0 + (pin >> 4)] = bits;
// //     } else {
// //         pin &= 0x1f;
// //         gpio[GPIO_PUD] = mode & 0x3;
// //         SysTimer::SleepUsec(2);
// //         gpio[GPIO_CLK_0] = 0x1 << pin;
// //         SysTimer::SleepUsec(2);
// //         gpio[GPIO_PUD]   = 0;
// //         gpio[GPIO_CLK_0] = 0;
// //     }
// // }

// // //---------------------------------------------------------------------------
// // //
// // //	Set output pin
// // //
// // //---------------------------------------------------------------------------
// // void GPIOBUS_RPi_rp1::PinSetSignal(int pin, bool ast)
// // {
// //     // Check for invalid pin
// //     if (pin < 0) {
// //         return;
// //     }

// //     if (ast) {
// //         gpio[GPIO_SET_0] = 0x1 << pin;
// //     } else {
// //         gpio[GPIO_CLR_0] = 0x1 << pin;
// //     }
// // }

// // //---------------------------------------------------------------------------
// // //
// // //	Set the signal drive strength
// // //
// // //---------------------------------------------------------------------------
// // void GPIOBUS_RPi_rp1::DrvConfig(uint32_t drive)
// // {
// //     uint32_t data  = pads[PAD_0_27];
// //     pads[PAD_0_27] = (0xFFFFFFF8 & data) | drive | 0x5a000000;
// // }

// //---------------------------------------------------------------------------
// //
// //	Bus signal acquisition
// //
// //---------------------------------------------------------------------------
// uint32_t GPIOBUS_RPi_rp1::Acquire()
// {
//     return 0;
// //     signals = *level;

// // #if SIGNAL_CONTROL_MODE < 2
// //     // Invert if negative logic (internal processing is unified to positive logic)
// //     signals = ~signals;
// // #endif // SIGNAL_CONTROL_MODE

// //     return signals;
// }
