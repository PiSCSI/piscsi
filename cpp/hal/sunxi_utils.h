//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI Reloaded
//	for Raspberry Pi
//
//	Copyright (C) 2022 akuker
//
//	[ Utility functions for working with Allwinner CPUs ]
//
//  This should include generic functions that can be applicable to
//  different variants of the SunXI (Allwinner) SoCs
//
//---------------------------------------------------------------------------

#pragma once

#include <cstdint>

class SunXI
{
  public:
    static inline int GPIO_BANK(int pin)
    {
        return (pin >> 5);
    }
    static inline int GPIO_NUM(int pin)
    {
        return (pin & 0x1F);
    }
    static inline int GPIO_CFG_INDEX(int pin)
    {
        return ((pin & 0x1F) >> 3);
    }
    static inline int GPIO_CFG_OFFSET(int pin)
    {
        return (((pin & 0x1F) & 0x7) << 2);
    }
    static inline int GPIO_PUL_INDEX(int pin)
    {
        return ((pin & 0x1F) >> 4);
    }
    static inline int GPIO_PUL_OFFSET(int pin)
    {
        return ((pin & 0x0F) << 1);
    }
    static inline int GPIO_DRV_INDEX(int pin)
    {
        return ((pin & 0x1F) >> 4);
    }
    static inline int GPIO_DRV_OFFSET(int pin)
    {
        return ((pin & 0x0F) << 1);
    }

    static inline void short_wait(void)
    {
        for (int i = 0; i < 150; i++) {
#ifndef __arm__
            usleep(1);
#else
            // wait 150 cycles
            asm volatile("nop");
#endif
        }
    }

    enum class gpio_configure_values_e : uint8_t {
        gpio_input      = 0b000,
        gpio_output     = 0b001,
        gpio_alt_func_1 = 0b010,
        gpio_alt_func_2 = 0b011,
        gpio_reserved_1 = 0b100,
        gpio_reserved_2 = 0b101,
        gpio_interupt   = 0b110,
        gpio_disable    = 0b111
    };

    typedef struct sunxi_gpio {
        unsigned int CFG[4];
        unsigned int DAT;
        unsigned int DRV[2];
        unsigned int PULL[2];
    } sunxi_gpio_t;

    /* gpio interrupt control */
    typedef struct sunxi_gpio_int {
        unsigned int CFG[3];
        unsigned int CTL;
        unsigned int STA;
        unsigned int DEB;
    } sunxi_gpio_int_t;

    typedef struct sunxi_gpio_reg {
        struct sunxi_gpio gpio_bank[9];
        unsigned char res[0xbc];
        struct sunxi_gpio_int gpio_int;
    } sunxi_gpio_reg_t;

    static const uint32_t PAGE_SIZE  = (4 * 1024);
    static const uint32_t BLOCK_SIZE = (4 * 1024);

    static const int SETUP_DEVMEM_FAIL  = 1;
    static const int SETUP_MALLOC_FAIL  = 2;
    static const int SETUP_MMAP_FAIL    = 3;
    static const int SETUP_CPUINFO_FAIL = 4;
    static const int SETUP_NOT_RPI_FAIL = 5;
    static const int INPUT              = 1;
    static const int OUTPUT             = 0;
    static const int ALT0               = 4;
    static const int HIGH               = 1;
    static const int LOW                = 0;
    static const int PUD_OFF            = 0;
    static const int PUD_DOWN           = 1;
    static const int PUD_UP             = 2;

    static const uint32_t SUNXI_R_GPIO_BASE       = 0x01F02000;
    static const uint32_t SUNXI_R_GPIO_REG_OFFSET = 0xC00;
    static const uint32_t SUNXI_GPIO_BASE         = 0x01C20000;
    static const uint32_t SUNXI_GPIO_REG_OFFSET   = 0x800;
    static const uint32_t SUNXI_CFG_OFFSET        = 0x00;
    static const uint32_t SUNXI_DATA_OFFSET       = 0x10;
    static const uint32_t SUNXI_PUD_OFFSET        = 0x1C;
    static const uint32_t SUNXI_BANK_SIZE         = 0x24;

    static const uint32_t MAP_SIZE = (4096 * 2);
    static const uint32_t MAP_MASK = (MAP_SIZE - 1);

    static const int FSEL_OFFSET         = 0;  // 0x0000
    static const int SET_OFFSET          = 7;  // 0x001c / 4
    static const int CLR_OFFSET          = 10; // 0x0028 / 4
    static const int PINLEVEL_OFFSET     = 13; // 0x0034 / 4
    static const int EVENT_DETECT_OFFSET = 16; // 0x0040 / 4
    static const int RISING_ED_OFFSET    = 19; // 0x004c / 4
    static const int FALLING_ED_OFFSET   = 22; // 0x0058 / 4
    static const int HIGH_DETECT_OFFSET  = 25; // 0x0064 / 4
    static const int LOW_DETECT_OFFSET   = 28; // 0x0070 / 4
    static const int PULLUPDN_OFFSET     = 37; // 0x0094 / 4
    static const int PULLUPDNCLK_OFFSET  = 38; // 0x0098 / 4

    static const uint32_t TMR_REGISTER_BASE   = 0x01C20C00;
    static const uint32_t TMR_IRQ_EN_REG      = 0x0;  // T imer IRQ Enable Register
    static const uint32_t TMR_IRQ_STA_REG     = 0x4;  // Timer Status Register
    static const uint32_t TMR0_CTRL_REG       = 0x10; // Timer 0 Control Register
    static const uint32_t TMR0_INTV_VALUE_REG = 0x14; // Timer 0 Interval Value Register
    static const uint32_t TMR0_CUR_VALUE_REG  = 0x18; // Timer 0 Current Value Register
    static const uint32_t TMR1_CTRL_REG       = 0x20; // Timer 1 Control Register
    static const uint32_t TMR1_INTV_VALUE_REG = 0x24; // Timer 1 Interval Value Register
    static const uint32_t TMR1_CUR_VALUE_REG  = 0x28; // Timer 1 Current Value Register
    static const uint32_t AVS_CNT_CTL_REG     = 0x80; // AVS Control Register
    static const uint32_t AVS_CNT0_REG        = 0x84; // AVS Counter 0 Register
    static const uint32_t AVS_CNT1_REG        = 0x88; // AVS Counter 1 Register
    static const uint32_t AVS_CNT_DIV_REG     = 0x8C; // AVS Divisor Register
    static const uint32_t WDOG0_IRQ_EN_REG    = 0xA0; // Watchdog 0 IRQ Enable Register
    static const uint32_t WDOG0_IRQ_STA_REG   = 0xA4; // Watchdog 0 Status Register
    static const uint32_t WDOG0_CTRL_REG      = 0xB0; // Watchdog 0 Control Register
    static const uint32_t WDOG0_CFG_REG       = 0xB4; // Watchdog 0 Configuration Register
    static const uint32_t WDOG0_MODE_REG      = 0xB8; // Watchdog 0 Mode Register
};