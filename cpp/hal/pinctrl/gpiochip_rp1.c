#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gpiochip.h"
#include "util.h"

#define RP1_NUM_GPIOS 54

#define RP1_IO_BANK0_OFFSET      0x00000000
#define RP1_IO_BANK1_OFFSET      0x00004000
#define RP1_IO_BANK2_OFFSET      0x00008000
#define RP1_SYS_RIO_BANK0_OFFSET 0x00010000
#define RP1_SYS_RIO_BANK1_OFFSET 0x00014000
#define RP1_SYS_RIO_BANK2_OFFSET 0x00018000
#define RP1_PADS_BANK0_OFFSET    0x00020000
#define RP1_PADS_BANK1_OFFSET    0x00024000
#define RP1_PADS_BANK2_OFFSET    0x00028000

#define RP1_RW_OFFSET  0x0000
#define RP1_XOR_OFFSET 0x1000
#define RP1_SET_OFFSET 0x2000
#define RP1_CLR_OFFSET 0x3000

#define RP1_GPIO_CTRL_FSEL_LSB     0
#define RP1_GPIO_CTRL_FSEL_MASK    (0x1f << RP1_GPIO_CTRL_FSEL_LSB)
#define RP1_GPIO_CTRL_OUTOVER_LSB  12
#define RP1_GPIO_CTRL_OUTOVER_MASK (0x03 << RP1_GPIO_CTRL_OUTOVER_LSB)
#define RP1_GPIO_CTRL_OEOVER_LSB   14
#define RP1_GPIO_CTRL_OEOVER_MASK  (0x03 << RP1_GPIO_CTRL_OEOVER_LSB)

#define RP1_PADS_OD_SET       (1 << 7)
#define RP1_PADS_IE_SET       (1 << 6)
#define RP1_PADS_PUE_SET      (1 << 3)
#define RP1_PADS_PDE_SET      (1 << 2)

#define RP1_GPIO_IO_REG_STATUS_OFFSET(offset) (((offset * 2) + 0) * sizeof(uint32_t))
#define RP1_GPIO_IO_REG_CTRL_OFFSET(offset)   (((offset * 2) + 1) * sizeof(uint32_t))
#define RP1_GPIO_PADS_REG_OFFSET(offset)      (sizeof(uint32_t) + (offset * sizeof(uint32_t)))

#define RP1_GPIO_SYS_RIO_REG_OUT_OFFSET        0x0
#define RP1_GPIO_SYS_RIO_REG_OE_OFFSET         0x4
#define RP1_GPIO_SYS_RIO_REG_SYNC_IN_OFFSET    0x8

#define rp1_gpio_write32(base, peri_offset, reg_offset, value) \
    base[(peri_offset + reg_offset)/4] = value

#define rp1_gpio_read32(base, peri_offset, reg_offset) \
    base[(peri_offset + reg_offset)/4]

typedef struct
{
   uint32_t io[3];
   uint32_t pads[3];
   uint32_t sys_rio[3];
} GPIO_STATE_T;

typedef enum
{
    RP1_FSEL_ALT0       = 0x0,
    RP1_FSEL_ALT1       = 0x1,
    RP1_FSEL_ALT2       = 0x2,
    RP1_FSEL_ALT3       = 0x3,
    RP1_FSEL_ALT4       = 0x4,
    RP1_FSEL_ALT5       = 0x5,
    RP1_FSEL_ALT6       = 0x6,
    RP1_FSEL_ALT7       = 0x7,
    RP1_FSEL_ALT8       = 0x8,
    RP1_FSEL_COUNT,
    RP1_FSEL_SYS_RIO    = RP1_FSEL_ALT5,
    RP1_FSEL_NULL       = 0x1f
} RP1_FSEL_T;

static const GPIO_STATE_T gpio_state = {
    .io = {RP1_IO_BANK0_OFFSET, RP1_IO_BANK1_OFFSET, RP1_IO_BANK2_OFFSET},
    .pads = {RP1_PADS_BANK0_OFFSET, RP1_PADS_BANK1_OFFSET, RP1_PADS_BANK2_OFFSET},
    .sys_rio = {RP1_SYS_RIO_BANK0_OFFSET, RP1_SYS_RIO_BANK1_OFFSET, RP1_SYS_RIO_BANK2_OFFSET},
};

static const int rp1_bank_base[] = {0, 28, 34};

static const char *rp1_gpio_fsel_names[RP1_NUM_GPIOS][RP1_FSEL_COUNT] =
{
    { "SPI0_SIO3" , "DPI_PCLK"     , "TXD1"         , "SDA0"         , 0              , "SYS_RIO00" , "PROC_RIO00" , "PIO0"       , "SPI2_CE0" , },
    { "SPI0_SIO2" , "DPI_DE"       , "RXD1"         , "SCL0"         , 0              , "SYS_RIO01" , "PROC_RIO01" , "PIO1"       , "SPI2_SIO1", },
    { "SPI0_CE3"  , "DPI_VSYNC"    , "CTS1"         , "SDA1"         , "IR_RX0"       , "SYS_RIO02" , "PROC_RIO02" , "PIO2"       , "SPI2_SIO0", },
    { "SPI0_CE2"  , "DPI_HSYNC"    , "RTS1"         , "SCL1"         , "IR_TX0"       , "SYS_RIO03" , "PROC_RIO03" , "PIO3"       , "SPI2_SCLK", },
    { "GPCLK0"    , "DPI_D0"       , "TXD2"         , "SDA2"         , "RI0"          , "SYS_RIO04" , "PROC_RIO04" , "PIO4"       , "SPI3_CE0" , },
    { "GPCLK1"    , "DPI_D1"       , "RXD2"         , "SCL2"         , "DTR0"         , "SYS_RIO05" , "PROC_RIO05" , "PIO5"       , "SPI3_SIO1", },
    { "GPCLK2"    , "DPI_D2"       , "CTS2"         , "SDA3"         , "DCD0"         , "SYS_RIO06" , "PROC_RIO06" , "PIO6"       , "SPI3_SIO0", },
    { "SPI0_CE1"  , "DPI_D3"       , "RTS2"         , "SCL3"         , "DSR0"         , "SYS_RIO07" , "PROC_RIO07" , "PIO7"       , "SPI3_SCLK", },
    { "SPI0_CE0"  , "DPI_D4"       , "TXD3"         , "SDA0"         , 0              , "SYS_RIO08" , "PROC_RIO08" , "PIO8"       , "SPI4_CE0" , },
    { "SPI0_MISO" , "DPI_D5"       , "RXD3"         , "SCL0"         , 0              , "SYS_RIO09" , "PROC_RIO09" , "PIO9"       , "SPI4_SIO0", },
    { "SPI0_MOSI" , "DPI_D6"       , "CTS3"         , "SDA1"         , 0              , "SYS_RIO010", "PROC_RIO010", "PIO10"      , "SPI4_SIO1", },
    { "SPI0_SCLK" , "DPI_D7"       , "RTS3"         , "SCL1"         , 0              , "SYS_RIO011", "PROC_RIO011", "PIO11"      , "SPI4_SCLK", },
    { "PWM0_CHAN0", "DPI_D8"       , "TXD4"         , "SDA2"         , "AAUD_LEFT"    , "SYS_RIO012", "PROC_RIO012", "PIO12"      , "SPI5_CE0" , },
    { "PWM0_CHAN1", "DPI_D9"       , "RXD4"         , "SCL2"         , "AAUD_RIGHT"   , "SYS_RIO013", "PROC_RIO013", "PIO13"      , "SPI5_SIO1", },
    { "PWM0_CHAN2", "DPI_D10"      , "CTS4"         , "SDA3"         , "TXD0"         , "SYS_RIO014", "PROC_RIO014", "PIO14"      , "SPI5_SIO0", },
    { "PWM0_CHAN3", "DPI_D11"      , "RTS4"         , "SCL3"         , "RXD0"         , "SYS_RIO015", "PROC_RIO015", "PIO15"      , "SPI5_SCLK", },
    { "SPI1_CE2"  , "DPI_D12"      , "DSI0_TE_EXT"  , 0              , "CTS0"         , "SYS_RIO016", "PROC_RIO016", "PIO16"      , },
    { "SPI1_CE1"  , "DPI_D13"      , "DSI1_TE_EXT"  , 0              , "RTS0"         , "SYS_RIO017", "PROC_RIO017", "PIO17"      , },
    { "SPI1_CE0"  , "DPI_D14"      , "I2S0_SCLK"    , "PWM0_CHAN2"   , "I2S1_SCLK"    , "SYS_RIO018", "PROC_RIO018", "PIO18"      , "GPCLK1",   },
    { "SPI1_MISO" , "DPI_D15"      , "I2S0_WS"      , "PWM0_CHAN3"   , "I2S1_WS"      , "SYS_RIO019", "PROC_RIO019", "PIO19"      , },
    { "SPI1_MOSI" , "DPI_D16"      , "I2S0_SDI0"    , "GPCLK0"       , "I2S1_SDI0"    , "SYS_RIO020", "PROC_RIO020", "PIO20"      , },
    { "SPI1_SCLK" , "DPI_D17"      , "I2S0_SDO0"    , "GPCLK1"       , "I2S1_SDO0"    , "SYS_RIO021", "PROC_RIO021", "PIO21"      , },
    { "SD0_CLK"   , "DPI_D18"      , "I2S0_SDI1"    , "SDA3"         , "I2S1_SDI1"    , "SYS_RIO022", "PROC_RIO022", "PIO22"      , },
    { "SD0_CMD"   , "DPI_D19"      , "I2S0_SDO1"    , "SCL3"         , "I2S1_SDO1"    , "SYS_RIO023", "PROC_RIO023", "PIO23"      , },
    { "SD0_DAT0"  , "DPI_D20"      , "I2S0_SDI2"    , 0              , "I2S1_SDI2"    , "SYS_RIO024", "PROC_RIO024", "PIO24"      , "SPI2_CE1" , },
    { "SD0_DAT1"  , "DPI_D21"      , "I2S0_SDO2"    , "MIC_CLK"      , "I2S1_SDO2"    , "SYS_RIO025", "PROC_RIO025", "PIO25"      , "SPI3_CE1" , },
    { "SD0_DAT2"  , "DPI_D22"      , "I2S0_SDI3"    , "MIC_DAT0"     , "I2S1_SDI3"    , "SYS_RIO026", "PROC_RIO026", "PIO26"      , "SPI5_CE1" , },
    { "SD0_DAT3"  , "DPI_D23"      , "I2S0_SDO3"    , "MIC_DAT1"     , "I2S1_SDO3"    , "SYS_RIO027", "PROC_RIO027", "PIO27"      , "SPI1_CE1" , },
    { "SD1_CLK"   , "SDA4"         , "I2S2_SCLK"    , "SPI6_MISO"    , "VBUS_EN0"     , "SYS_RIO10" , "PROC_RIO10" , },
    { "SD1_CMD"   , "SCL4"         , "I2S2_WS"      , "SPI6_MOSI"    , "VBUS_OC0"     , "SYS_RIO11" , "PROC_RIO11" , },
    { "SD1_DAT0"  , "SDA5"         , "I2S2_SDI0"    , "SPI6_SCLK"    , "TXD5"         , "SYS_RIO12" , "PROC_RIO12" , },
    { "SD1_DAT1"  , "SCL5"         , "I2S2_SDO0"    , "SPI6_CE0"     , "RXD5"         , "SYS_RIO13" , "PROC_RIO13" , },
    { "SD1_DAT2"  , "GPCLK3"       , "I2S2_SDI1"    , "SPI6_CE1"     , "CTS5"         , "SYS_RIO14" , "PROC_RIO14" , },
    { "SD1_DAT3"  , "GPCLK4"       , "I2S2_SDO1"    , "SPI6_CE2"     , "RTS5"         , "SYS_RIO15" , "PROC_RIO15" , },
    { "PWM1_CHAN2", "GPCLK3"       , "VBUS_EN0"     , "SDA4"         , "MIC_CLK"      , "SYS_RIO20" , "PROC_RIO20" , },
    { "SPI8_CE1"  , "PWM1_CHAN0"   , "VBUS_OC0"     , "SCL4"         , "MIC_DAT0"     , "SYS_RIO21" , "PROC_RIO21" , },
    { "SPI8_CE0"  , "TXD5"         , "PCIE_CLKREQ_N", "SDA5"         , "MIC_DAT1"     , "SYS_RIO22" , "PROC_RIO22" , },
    { "SPI8_MISO" , "RXD5"         , "MIC_CLK"      , "SCL5"         , "PCIE_CLKREQ_N", "SYS_RIO23" , "PROC_RIO23" , },
    { "SPI8_MOSI" , "RTS5"         , "MIC_DAT0"     , "SDA6"         , "AAUD_LEFT"    , "SYS_RIO24" , "PROC_RIO24" , "DSI0_TE_EXT", },
    { "SPI8_SCLK" , "CTS5"         , "MIC_DAT1"     , "SCL6"         , "AAUD_RIGHT"   , "SYS_RIO25" , "PROC_RIO25" , "DSI1_TE_EXT", },
    { "PWM1_CHAN1", "TXD5"         , "SDA4"         , "SPI6_MISO"    , "AAUD_LEFT"    , "SYS_RIO26" , "PROC_RIO26" , },
    { "PWM1_CHAN2", "RXD5"         , "SCL4"         , "SPI6_MOSI"    , "AAUD_RIGHT"   , "SYS_RIO27" , "PROC_RIO27" , },
    { "GPCLK5"    , "RTS5"         , "VBUS_EN1"     , "SPI6_SCLK"    , "I2S2_SCLK"    , "SYS_RIO28" , "PROC_RIO28" , },
    { "GPCLK4"    , "CTS5"         , "VBUS_OC1"     , "SPI6_CE0"     , "I2S2_WS"      , "SYS_RIO29" , "PROC_RIO29" , },
    { "GPCLK5"    , "SDA5"         , "PWM1_CHAN0"   , "SPI6_CE1"     , "I2S2_SDI0"    , "SYS_RIO210", "PROC_RIO210", },
    { "PWM1_CHAN3", "SCL5"         , "SPI7_CE0"     , "SPI6_CE2"     , "I2S2_SDO0"    , "SYS_RIO211", "PROC_RIO211", },
    { "GPCLK3"    , "SDA4"         , "SPI7_MOSI"    , "MIC_CLK"      , "I2S2_SDI1"    , "SYS_RIO212", "PROC_RIO212", "DSI0_TE_EXT", },
    { "GPCLK5"    , "SCL4"         , "SPI7_MISO"    , "MIC_DAT0"     , "I2S2_SDO1"    , "SYS_RIO213", "PROC_RIO213", "DSI1_TE_EXT", },
    { "PWM1_CHAN0", "PCIE_CLKREQ_N", "SPI7_SCLK"    , "MIC_DAT1"     , "TXD5"         , "SYS_RIO214", "PROC_RIO214", },
    { "SPI8_SCLK" , "SPI7_SCLK"    , "SDA5"         , "AAUD_LEFT"    , "RXD5"         , "SYS_RIO215", "PROC_RIO215", },
    { "SPI8_MISO" , "SPI7_MOSI"    , "SCL5"         , "AAUD_RIGHT"   , "VBUS_EN2"     , "SYS_RIO216", "PROC_RIO216", },
    { "SPI8_MOSI" , "SPI7_MISO"    , "SDA6"         , "AAUD_LEFT"    , "VBUS_OC2"     , "SYS_RIO217", "PROC_RIO217", },
    { "SPI8_CE0"  , 0              , "SCL6"         , "AAUD_RIGHT"   , "VBUS_EN3"     , "SYS_RIO218", "PROC_RIO218", },
    { "SPI8_CE1"  , "SPI7_CE0"     , 0              , "PCIE_CLKREQ_N", "VBUS_OC3"     , "SYS_RIO219", "PROC_RIO219", },
};

static void rp1_gpio_get_bank(int num, int *bank, int *offset)
{
    *bank = *offset = 0;
    if (num >= RP1_NUM_GPIOS)
    {
        assert(0);
        return;
    }

    if (num < rp1_bank_base[1])
        *bank = 0;
    else if (num < rp1_bank_base[2])
        *bank = 1;
    else
        *bank = 2;

   *offset = num - rp1_bank_base[*bank];
}

static uint32_t rp1_gpio_ctrl_read(volatile uint32_t *base, int bank, int offset)
{
    return rp1_gpio_read32(base, gpio_state.io[bank], RP1_GPIO_IO_REG_CTRL_OFFSET(offset));
}

static void rp1_gpio_ctrl_write(volatile uint32_t *base, int bank, int offset,
                                uint32_t value)
{
    rp1_gpio_write32(base, gpio_state.io[bank], RP1_GPIO_IO_REG_CTRL_OFFSET(offset), value);
}

static uint32_t rp1_gpio_pads_read(volatile uint32_t *base, int bank, int offset)
{
    return rp1_gpio_read32(base, gpio_state.pads[bank], RP1_GPIO_PADS_REG_OFFSET(offset));
}

static void rp1_gpio_pads_write(volatile uint32_t *base, int bank, int offset,
                                uint32_t value)
{
    rp1_gpio_write32(base, gpio_state.pads[bank], RP1_GPIO_PADS_REG_OFFSET(offset), value);
}

static uint32_t rp1_gpio_sys_rio_out_read(volatile uint32_t *base, int bank,
                                          int offset)
{
    UNUSED(offset);
    return rp1_gpio_read32(base, gpio_state.sys_rio[bank], RP1_GPIO_SYS_RIO_REG_OUT_OFFSET);
}

static uint32_t rp1_gpio_sys_rio_sync_in_read(volatile uint32_t *base, int bank,
                                              int offset)
{
    UNUSED(offset);
    return rp1_gpio_read32(base, gpio_state.sys_rio[bank],
                           RP1_GPIO_SYS_RIO_REG_SYNC_IN_OFFSET);
}

static void rp1_gpio_sys_rio_out_set(volatile uint32_t *base, int bank, int offset)
{
    rp1_gpio_write32(base, gpio_state.sys_rio[bank],
                     RP1_GPIO_SYS_RIO_REG_OUT_OFFSET + RP1_SET_OFFSET, 1U << offset);
}

static void rp1_gpio_sys_rio_out_clr(volatile uint32_t *base, int bank, int offset)
{
    rp1_gpio_write32(base, gpio_state.sys_rio[bank],
                     RP1_GPIO_SYS_RIO_REG_OUT_OFFSET + RP1_CLR_OFFSET, 1U << offset);
}

static uint32_t rp1_gpio_sys_rio_oe_read(volatile uint32_t *base, int bank)
{
    return rp1_gpio_read32(base, gpio_state.sys_rio[bank],
                           RP1_GPIO_SYS_RIO_REG_OE_OFFSET);
}

static void rp1_gpio_sys_rio_oe_clr(volatile uint32_t *base, int bank, int offset)
{
    rp1_gpio_write32(base, gpio_state.sys_rio[bank],
                     RP1_GPIO_SYS_RIO_REG_OE_OFFSET + RP1_CLR_OFFSET,
                     1U << offset);
}

static void rp1_gpio_sys_rio_oe_set(volatile uint32_t *base, int bank, int offset)
{
    rp1_gpio_write32(base, gpio_state.sys_rio[bank],
                     RP1_GPIO_SYS_RIO_REG_OE_OFFSET + RP1_SET_OFFSET,
                     1U << offset);
}

static void rp1_gpio_set_dir(void *priv, uint32_t gpio, GPIO_DIR_T dir)
{
    volatile uint32_t *base = priv;
    int bank, offset;

    rp1_gpio_get_bank(gpio, &bank, &offset);

    if (dir == DIR_INPUT)
        rp1_gpio_sys_rio_oe_clr(base, bank, offset);
    else if (dir == DIR_OUTPUT)
        rp1_gpio_sys_rio_oe_set(base, bank, offset);
    else
        assert(0);
}

static GPIO_DIR_T rp1_gpio_get_dir(void *priv, unsigned gpio)
{
    volatile uint32_t *base = priv;
    int bank, offset;
    GPIO_DIR_T dir;
    uint32_t reg;

    rp1_gpio_get_bank(gpio, &bank, &offset);
    reg = rp1_gpio_sys_rio_oe_read(base, bank);

    dir = (reg & (1U << offset)) ? DIR_OUTPUT : DIR_INPUT;

    return dir;
}

static GPIO_FSEL_T rp1_gpio_get_fsel(void *priv, unsigned gpio)
{
    volatile uint32_t *base = priv;
    int bank, offset;
    uint32_t reg;
    GPIO_FSEL_T fsel;
    RP1_FSEL_T rsel;

    rp1_gpio_get_bank(gpio, &bank, &offset);
    reg = rp1_gpio_ctrl_read(base, bank, offset);
    rsel = ((reg & RP1_GPIO_CTRL_FSEL_MASK) >> RP1_GPIO_CTRL_FSEL_LSB);
    if (rsel == RP1_FSEL_SYS_RIO)
        fsel = GPIO_FSEL_GPIO;
    else if (rsel == RP1_FSEL_NULL)
        fsel = GPIO_FSEL_NONE;
    else if (rsel < RP1_FSEL_COUNT)
        fsel = (GPIO_FSEL_T)rsel;
    else
        fsel = GPIO_FSEL_MAX;

    return fsel;
}

static void rp1_gpio_set_fsel(void *priv, unsigned gpio, const GPIO_FSEL_T func)
{
    volatile uint32_t *base = priv;
    int bank, offset;
    uint32_t ctrl_reg;
    uint32_t pad_reg;
    uint32_t old_pad_reg;
    RP1_FSEL_T rsel;

    if (func < (GPIO_FSEL_T)RP1_FSEL_COUNT)
        rsel = (RP1_FSEL_T)func;
    else if (func == GPIO_FSEL_INPUT ||
             func == GPIO_FSEL_OUTPUT ||
             func == GPIO_FSEL_GPIO)
        rsel = RP1_FSEL_SYS_RIO;
    else if (func == GPIO_FSEL_NONE)
        rsel = RP1_FSEL_NULL;
    else
        return;

    rp1_gpio_get_bank(gpio, &bank, &offset);
    if (func == GPIO_FSEL_INPUT)
        rp1_gpio_set_dir(priv, gpio, DIR_INPUT);
    else if (func == GPIO_FSEL_OUTPUT)
        rp1_gpio_set_dir(priv, gpio, DIR_OUTPUT);

    ctrl_reg = rp1_gpio_ctrl_read(base, bank, offset) & ~RP1_GPIO_CTRL_FSEL_MASK;
    ctrl_reg |= rsel << RP1_GPIO_CTRL_FSEL_LSB;
    rp1_gpio_ctrl_write(base, bank, offset, ctrl_reg);

    pad_reg = rp1_gpio_pads_read(base, bank, offset);
    old_pad_reg = pad_reg;
    if (rsel == RP1_FSEL_NULL)
    {
        // Disable input
        pad_reg &= ~RP1_PADS_IE_SET;
    }
    else
    {
        // Enable input
        pad_reg |= RP1_PADS_IE_SET;
    }

    if (rsel != RP1_FSEL_NULL)
    {
        // Enable peripheral func output
        pad_reg &= ~RP1_PADS_OD_SET;
    }
    else
    {
        // Disable peripheral func output
        pad_reg |= RP1_PADS_OD_SET;
    }

    if (pad_reg != old_pad_reg)
        rp1_gpio_pads_write(base, bank, offset, pad_reg);
}

static int rp1_gpio_get_level(void *priv, unsigned gpio)
{
    volatile uint32_t *base = priv;
    int bank, offset;
    uint32_t pad_reg;
    uint32_t reg;
    int level;

    rp1_gpio_get_bank(gpio, &bank, &offset);
    pad_reg = rp1_gpio_pads_read(base, bank, offset);
    if (!(pad_reg & RP1_PADS_IE_SET))
	return -1;
    reg = rp1_gpio_sys_rio_sync_in_read(base, bank, offset);
    level = (reg & (1U << offset)) ? 1 : 0;

    return level;
}

static void rp1_gpio_set_drive(void *priv, unsigned gpio, GPIO_DRIVE_T drv)
{
    volatile uint32_t *base = priv;
    int bank, offset;

    rp1_gpio_get_bank(gpio, &bank, &offset);
    if (drv == DRIVE_HIGH)
        rp1_gpio_sys_rio_out_set(base, bank, offset);
    else if (drv == DRIVE_LOW)
        rp1_gpio_sys_rio_out_clr(base, bank, offset);
}

static void rp1_gpio_set_pull(void *priv, unsigned gpio, GPIO_PULL_T pull)
{
    volatile uint32_t *base = priv;
    uint32_t reg;
    int bank, offset;

    rp1_gpio_get_bank(gpio, &bank, &offset);
    reg = rp1_gpio_pads_read(base, bank, offset);
    reg &= ~(RP1_PADS_PDE_SET | RP1_PADS_PUE_SET);
    if (pull == PULL_UP)
        reg |= RP1_PADS_PUE_SET;
    else if (pull == PULL_DOWN)
        reg |= RP1_PADS_PDE_SET;
    rp1_gpio_pads_write(base, bank, offset, reg);
}

static GPIO_PULL_T rp1_gpio_get_pull(void *priv, unsigned gpio)
{
    volatile uint32_t *base = priv;
    uint32_t reg;
    GPIO_PULL_T pull = PULL_NONE;
    int bank, offset;

    rp1_gpio_get_bank(gpio, &bank, &offset);
    reg = rp1_gpio_pads_read(base, bank, offset);
    if (reg & RP1_PADS_PUE_SET)
        pull = PULL_UP;
    else if (reg & RP1_PADS_PDE_SET)
        pull = PULL_DOWN;

    return pull;
}

static GPIO_DRIVE_T rp1_gpio_get_drive(void *priv, unsigned gpio)
{
    volatile uint32_t *base = priv;
    uint32_t reg;
    int bank, offset;

    rp1_gpio_get_bank(gpio, &bank, &offset);
    reg = rp1_gpio_sys_rio_out_read(base, bank, offset);
    return (reg & (1U << offset)) ? DRIVE_HIGH : DRIVE_LOW;
}

static const char *rp1_gpio_get_name(void *priv, unsigned gpio)
{
    static char name_buf[16];
    UNUSED(priv);

    if (gpio >= RP1_NUM_GPIOS)
        return NULL;

    sprintf(name_buf, "GPIO%d", gpio);
    return name_buf;
}

static const char *rp1_gpio_get_fsel_name(void *priv, unsigned gpio, GPIO_FSEL_T fsel)
{
    const char *name = NULL;
    UNUSED(priv);
    switch (fsel)
    {
    case GPIO_FSEL_GPIO:
        name = "gpio";
        break;
    case GPIO_FSEL_INPUT:
        name = "input";
        break;
    case GPIO_FSEL_OUTPUT:
        name = "output";
        break;
    case GPIO_FSEL_NONE:
        name = "none";
        break;
    case GPIO_FSEL_FUNC0:
    case GPIO_FSEL_FUNC1:
    case GPIO_FSEL_FUNC2:
    case GPIO_FSEL_FUNC3:
    case GPIO_FSEL_FUNC4:
    case GPIO_FSEL_FUNC5:
    case GPIO_FSEL_FUNC6:
    case GPIO_FSEL_FUNC7:
    case GPIO_FSEL_FUNC8:
        if (gpio < RP1_NUM_GPIOS)
        {
            name = rp1_gpio_fsel_names[gpio][fsel - GPIO_FSEL_FUNC0];
            if (!name)
                name = "-";
        }
        break;
    default:
        return NULL;
    }
    return name;
}

static void *rp1_gpio_create_instance(const GPIO_CHIP_T *chip,
                                      const char *dtnode)
{
    UNUSED(dtnode);
    return (void *)chip;
}

static int rp1_gpio_count(void *priv)
{
    UNUSED(priv);
    return RP1_NUM_GPIOS;
}

static void *rp1_gpio_probe_instance(void *priv, volatile uint32_t *base)
{
    UNUSED(priv);
    return (void *)base;
}

static const GPIO_CHIP_INTERFACE_T rp1_gpio_interface =
{
    .gpio_create_instance = rp1_gpio_create_instance,
    .gpio_count = rp1_gpio_count,
    .gpio_probe_instance = rp1_gpio_probe_instance,
    .gpio_get_fsel = rp1_gpio_get_fsel,
    .gpio_set_fsel = rp1_gpio_set_fsel,
    .gpio_set_drive = rp1_gpio_set_drive,
    .gpio_set_dir = rp1_gpio_set_dir,
    .gpio_get_dir = rp1_gpio_get_dir,
    .gpio_get_level = rp1_gpio_get_level,
    .gpio_get_drive = rp1_gpio_get_drive,
    .gpio_get_pull = rp1_gpio_get_pull,
    .gpio_set_pull = rp1_gpio_set_pull,
    .gpio_get_name = rp1_gpio_get_name,
    .gpio_get_fsel_name = rp1_gpio_get_fsel_name,
};

DECLARE_GPIO_CHIP(rp1, "raspberrypi,rp1-gpio",
                  &rp1_gpio_interface, 0x30000, 0);
