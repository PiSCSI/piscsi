#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "gpiochip.h"
#include "util.h"

#define ARRAY_SIZE(_a) (sizeof(_a)/sizeof(_a[0]))

#define BCM2835_NUM_GPIOS 54
#define BCM2835_ALT_COUNT 6

#define BCM2711_NUM_GPIOS 54
#define BCM2711_ALT_COUNT 6

/* 2835 register offsets */
#define GPFSEL0      0
#define GPFSEL1      1
#define GPFSEL2      2
#define GPFSEL3      3
#define GPFSEL4      4
#define GPFSEL5      5
#define GPSET0       7
#define GPSET1       8
#define GPCLR0       10
#define GPCLR1       11
#define GPLEV0       13
#define GPLEV1       14
#define GPPUD        37
#define GPPUDCLK0    38
#define GPPUDCLK1    39

/* 2711 has a different mechanism for pin pull-up/down/enable  */
#define GPPUPPDN0    57        /* Pin pull-up/down for pins 15:0  */
#define GPPUPPDN1    58        /* Pin pull-up/down for pins 31:16 */
#define GPPUPPDN2    59        /* Pin pull-up/down for pins 47:32 */
#define GPPUPPDN3    60        /* Pin pull-up/down for pins 57:48 */

static const char *bcm2835_gpio_alt_names[BCM2835_NUM_GPIOS][BCM2835_ALT_COUNT] =
{
    { "SDA0"      , "SA5"        , "PCLK"      , "AVEOUT_VCLK"   , "AVEIN_VCLK" , 0           , },
    { "SCL0"      , "SA4"        , "DE"        , "AVEOUT_DSYNC"  , "AVEIN_DSYNC", 0           , },
    { "SDA1"      , "SA3"        , "LCD_VSYNC" , "AVEOUT_VSYNC"  , "AVEIN_VSYNC", 0           , },
    { "SCL1"      , "SA2"        , "LCD_HSYNC" , "AVEOUT_HSYNC"  , "AVEIN_HSYNC", 0           , },
    { "GPCLK0"    , "SA1"        , "DPI_D0"    , "AVEOUT_VID0"   , "AVEIN_VID0" , "ARM_TDI"   , },
    { "GPCLK1"    , "SA0"        , "DPI_D1"    , "AVEOUT_VID1"   , "AVEIN_VID1" , "ARM_TDO"   , },
    { "GPCLK2"    , "SOE_N_SE"   , "DPI_D2"    , "AVEOUT_VID2"   , "AVEIN_VID2" , "ARM_RTCK"  , },
    { "SPI0_CE1_N", "SWE_N_SRW_N", "DPI_D3"    , "AVEOUT_VID3"   , "AVEIN_VID3" , 0           , },
    { "SPI0_CE0_N", "SD0"        , "DPI_D4"    , "AVEOUT_VID4"   , "AVEIN_VID4" , 0           , },
    { "SPI0_MISO" , "SD1"        , "DPI_D5"    , "AVEOUT_VID5"   , "AVEIN_VID5" , 0           , },
    { "SPI0_MOSI" , "SD2"        , "DPI_D6"    , "AVEOUT_VID6"   , "AVEIN_VID6" , 0           , },
    { "SPI0_SCLK" , "SD3"        , "DPI_D7"    , "AVEOUT_VID7"   , "AVEIN_VID7" , 0           , },
    { "PWM0"      , "SD4"        , "DPI_D8"    , "AVEOUT_VID8"   , "AVEIN_VID8" , "ARM_TMS"   , },
    { "PWM1"      , "SD5"        , "DPI_D9"    , "AVEOUT_VID9"   , "AVEIN_VID9" , "ARM_TCK"   , },
    { "TXD0"      , "SD6"        , "DPI_D10"   , "AVEOUT_VID10"  , "AVEIN_VID10", "TXD1"      , },
    { "RXD0"      , "SD7"        , "DPI_D11"   , "AVEOUT_VID11"  , "AVEIN_VID11", "RXD1"      , },
    { "FL0"       , "SD8"        , "DPI_D12"   , "CTS0"          , "SPI1_CE2_N" , "CTS1"      , },
    { "FL1"       , "SD9"        , "DPI_D13"   , "RTS0"          , "SPI1_CE1_N" , "RTS1"      , },
    { "PCM_CLK"   , "SD10"       , "DPI_D14"   , "I2CSL_SDA_MOSI", "SPI1_CE0_N" , "PWM0"      , },
    { "PCM_FS"    , "SD11"       , "DPI_D15"   , "I2CSL_SCL_SCLK", "SPI1_MISO"  , "PWM1"      , },
    { "PCM_DIN"   , "SD12"       , "DPI_D16"   , "I2CSL_MISO"    , "SPI1_MOSI"  , "GPCLK0"    , },
    { "PCM_DOUT"  , "SD13"       , "DPI_D17"   , "I2CSL_CE_N"    , "SPI1_SCLK"  , "GPCLK1"    , },
    { "SD0_CLK"   , "SD14"       , "DPI_D18"   , "SD1_CLK"       , "ARM_TRST"   , 0           , },
    { "SD0_CMD"   , "SD15"       , "DPI_D19"   , "SD1_CMD"       , "ARM_RTCK"   , 0           , },
    { "SD0_DAT0"  , "SD16"       , "DPI_D20"   , "SD1_DAT0"      , "ARM_TDO"    , 0           , },
    { "SD0_DAT1"  , "SD17"       , "DPI_D21"   , "SD1_DAT1"      , "ARM_TCK"    , 0           , },
    { "SD0_DAT2"  , "TE0"        , "DPI_D22"   , "SD1_DAT2"      , "ARM_TDI"    , 0           , },
    { "SD0_DAT3"  , "TE1"        , "DPI_D23"   , "SD1_DAT3"      , "ARM_TMS"    , 0           , },
    { "SDA0"      , "SA5"        , "PCM_CLK"   , "FL0"           , 0            , 0           , },
    { "SCL0"      , "SA4"        , "PCM_FS"    , "FL1"           , 0            , 0           , },
    { "TE0"       , "SA3"        , "PCM_DIN"   , "CTS0"          , 0            , "CTS1"      , },
    { "FL0"       , "SA2"        , "PCM_DOUT"  , "RTS0"          , 0            , "RTS1"      , },
    { "GPCLK0"    , "SA1"        , "RING_OCLK" , "TXD0"          , 0            , "TXD1"      , },
    { "FL1"       , "SA0"        , "TE1"       , "RXD0"          , 0            , "RXD1"      , },
    { "GPCLK0"    , "SOE_N_SE"   , "TE2"       , "SD1_CLK"       , 0            , 0           , },
    { "SPI0_CE1_N", "SWE_N_SRW_N", 0           , "SD1_CMD"       , 0            , 0           , },
    { "SPI0_CE0_N", "SD0"        , "TXD0"      , "SD1_DAT0"      , 0            , 0           , },
    { "SPI0_MISO" , "SD1"        , "RXD0"      , "SD1_DAT1"      , 0            , 0           , },
    { "SPI0_MOSI" , "SD2"        , "RTS0"      , "SD1_DAT2"      , 0            , 0           , },
    { "SPI0_SCLK" , "SD3"        , "CTS0"      , "SD1_DAT3"      , 0            , 0           , },
    { "PWM0"      , "SD4"        , 0           , "SD1_DAT4"      , "SPI2_MISO"  , "TXD1"      , },
    { "PWM1"      , "SD5"        , "TE0"       , "SD1_DAT5"      , "SPI2_MOSI"  , "RXD1"      , },
    { "GPCLK1"    , "SD6"        , "TE1"       , "SD1_DAT6"      , "SPI2_SCLK"  , "RTS1"      , },
    { "GPCLK2"    , "SD7"        , "TE2"       , "SD1_DAT7"      , "SPI2_CE0_N" , "CTS1"      , },
    { "GPCLK1"    , "SDA0"       , "SDA1"      , "TE0"           , "SPI2_CE1_N" , 0           , },
    { "PWM1"      , "SCL0"       , "SCL1"      , "TE1"           , "SPI2_CE2_N" , 0           , },
    { "SDA0"      , "SDA1"       , "SPI0_CE0_N", 0               , 0            , "SPI2_CE1_N", },
    { "SCL0"      , "SCL1"       , "SPI0_MISO" , 0               , 0            , "SPI2_CE0_N", },
    { "SD0_CLK"   , "FL0"        , "SPI0_MOSI" , "SD1_CLK"       , "ARM_TRST"   , "SPI2_SCLK" , },
    { "SD0_CMD"   , "GPCLK0"     , "SPI0_SCLK" , "SD1_CMD"       , "ARM_RTCK"   , "SPI2_MOSI" , },
    { "SD0_DAT0"  , "GPCLK1"     , "PCM_CLK"   , "SD1_DAT0"      , "ARM_TDO"    , 0           , },
    { "SD0_DAT1"  , "GPCLK2"     , "PCM_FS"    , "SD1_DAT1"      , "ARM_TCK"    , 0           , },
    { "SD0_DAT2"  , "PWM0"       , "PCM_DIN"   , "SD1_DAT2"      , "ARM_TDI"    , 0           , },
    { "SD0_DAT3"  , "PWM1"       , "PCM_DOUT"  , "SD1_DAT3"      , "ARM_TMS"    , 0           , },
};

static const char *bcm2711_gpio_alt_names[BCM2711_NUM_GPIOS][BCM2711_ALT_COUNT] =
{
    { "SDA0"      , "SA5"        , "PCLK"      , "SPI3_CE0_N"    , "TXD2"            , "SDA6"        , },
    { "SCL0"      , "SA4"        , "DE"        , "SPI3_MISO"     , "RXD2"            , "SCL6"        , },
    { "SDA1"      , "SA3"        , "LCD_VSYNC" , "SPI3_MOSI"     , "CTS2"            , "SDA3"        , },
    { "SCL1"      , "SA2"        , "LCD_HSYNC" , "SPI3_SCLK"     , "RTS2"            , "SCL3"        , },
    { "GPCLK0"    , "SA1"        , "DPI_D0"    , "SPI4_CE0_N"    , "TXD3"            , "SDA3"        , },
    { "GPCLK1"    , "SA0"        , "DPI_D1"    , "SPI4_MISO"     , "RXD3"            , "SCL3"        , },
    { "GPCLK2"    , "SOE_N_SE"   , "DPI_D2"    , "SPI4_MOSI"     , "CTS3"            , "SDA4"        , },
    { "SPI0_CE1_N", "SWE_N_SRW_N", "DPI_D3"    , "SPI4_SCLK"     , "RTS3"            , "SCL4"        , },
    { "SPI0_CE0_N", "SD0"        , "DPI_D4"    , "I2CSL_CE_N"    , "TXD4"            , "SDA4"        , },
    { "SPI0_MISO" , "SD1"        , "DPI_D5"    , "I2CSL_SDI_MISO", "RXD4"            , "SCL4"        , },
    { "SPI0_MOSI" , "SD2"        , "DPI_D6"    , "I2CSL_SDA_MOSI", "CTS4"            , "SDA5"        , },
    { "SPI0_SCLK" , "SD3"        , "DPI_D7"    , "I2CSL_SCL_SCLK", "RTS4"            , "SCL5"        , },
    { "PWM0_0"    , "SD4"        , "DPI_D8"    , "SPI5_CE0_N"    , "TXD5"            , "SDA5"        , },
    { "PWM0_1"    , "SD5"        , "DPI_D9"    , "SPI5_MISO"     , "RXD5"            , "SCL5"        , },
    { "TXD0"      , "SD6"        , "DPI_D10"   , "SPI5_MOSI"     , "CTS5"            , "TXD1"        , },
    { "RXD0"      , "SD7"        , "DPI_D11"   , "SPI5_SCLK"     , "RTS5"            , "RXD1"        , },
    { 0           , "SD8"        , "DPI_D12"   , "CTS0"          , "SPI1_CE2_N"      , "CTS1"        , },
    { 0           , "SD9"        , "DPI_D13"   , "RTS0"          , "SPI1_CE1_N"      , "RTS1"        , },
    { "PCM_CLK"   , "SD10"       , "DPI_D14"   , "SPI6_CE0_N"    , "SPI1_CE0_N"      , "PWM0_0"      , },
    { "PCM_FS"    , "SD11"       , "DPI_D15"   , "SPI6_MISO"     , "SPI1_MISO"       , "PWM0_1"      , },
    { "PCM_DIN"   , "SD12"       , "DPI_D16"   , "SPI6_MOSI"     , "SPI1_MOSI"       , "GPCLK0"      , },
    { "PCM_DOUT"  , "SD13"       , "DPI_D17"   , "SPI6_SCLK"     , "SPI1_SCLK"       , "GPCLK1"      , },
    { "SD0_CLK"   , "SD14"       , "DPI_D18"   , "SD1_CLK"       , "ARM_TRST"        , "SDA6"        , },
    { "SD0_CMD"   , "SD15"       , "DPI_D19"   , "SD1_CMD"       , "ARM_RTCK"        , "SCL6"        , },
    { "SD0_DAT0"  , "SD16"       , "DPI_D20"   , "SD1_DAT0"      , "ARM_TDO"         , "SPI3_CE1_N"  , },
    { "SD0_DAT1"  , "SD17"       , "DPI_D21"   , "SD1_DAT1"      , "ARM_TCK"         , "SPI4_CE1_N"  , },
    { "SD0_DAT2"  , 0            , "DPI_D22"   , "SD1_DAT2"      , "ARM_TDI"         , "SPI5_CE1_N"  , },
    { "SD0_DAT3"  , 0            , "DPI_D23"   , "SD1_DAT3"      , "ARM_TMS"         , "SPI6_CE1_N"  , },
    { "SDA0"      , "SA5"        , "PCM_CLK"   , 0               , "MII_A_RX_ERR"    , "RGMII_MDIO"  , },
    { "SCL0"      , "SA4"        , "PCM_FS"    , 0               , "MII_A_TX_ERR"    , "RGMII_MDC"   , },
    { 0           , "SA3"        , "PCM_DIN"   , "CTS0"          , "MII_A_CRS"       , "CTS1"        , },
    { 0           , "SA2"        , "PCM_DOUT"  , "RTS0"          , "MII_A_COL"       , "RTS1"        , },
    { "GPCLK0"    , "SA1"        , 0           , "TXD0"          , "SD_CARD_PRES"    , "TXD1"        , },
    { 0           , "SA0"        , 0           , "RXD0"          , "SD_CARD_WRPROT"  , "RXD1"        , },
    { "GPCLK0"    , "SOE_N_SE"   , 0           , "SD1_CLK"       , "SD_CARD_LED"     , "RGMII_IRQ"   , },
    { "SPI0_CE1_N", "SWE_N_SRW_N", 0           , "SD1_CMD"       , "RGMII_START_STOP", 0             , },
    { "SPI0_CE0_N", "SD0"        , "TXD0"      , "SD1_DAT0"      , "RGMII_RX_OK"     , "MII_A_RX_ERR", },
    { "SPI0_MISO" , "SD1"        , "RXD0"      , "SD1_DAT1"      , "RGMII_MDIO"      , "MII_A_TX_ERR", },
    { "SPI0_MOSI" , "SD2"        , "RTS0"      , "SD1_DAT2"      , "RGMII_MDC"       , "MII_A_CRS"   , },
    { "SPI0_SCLK" , "SD3"        , "CTS0"      , "SD1_DAT3"      , "RGMII_IRQ"       , "MII_A_COL"   , },
    { "PWM1_0"    , "SD4"        , 0           , "SD1_DAT4"      , "SPI0_MISO"       , "TXD1"        , },
    { "PWM1_1"    , "SD5"        , 0           , "SD1_DAT5"      , "SPI0_MOSI"       , "RXD1"        , },
    { "GPCLK1"    , "SD6"        , 0           , "SD1_DAT6"      , "SPI0_SCLK"       , "RTS1"        , },
    { "GPCLK2"    , "SD7"        , 0           , "SD1_DAT7"      , "SPI0_CE0_N"      , "CTS1"        , },
    { "GPCLK1"    , "SDA0"       , "SDA1"      , 0               , "SPI0_CE1_N"      , "SD_CARD_VOLT", },
    { "PWM0_1"    , "SCL0"       , "SCL1"      , 0               , "SPI0_CE2_N"      , "SD_CARD_PWR0", },
    { "SDA0"      , "SDA1"       , "SPI0_CE0_N", 0               , 0                 , "SPI2_CE1_N"  , },
    { "SCL0"      , "SCL1"       , "SPI0_MISO" , 0               , 0                 , "SPI2_CE0_N"  , },
    { "SD0_CLK"   , 0            , "SPI0_MOSI" , "SD1_CLK"       , "ARM_TRST"        , "SPI2_SCLK"   , },
    { "SD0_CMD"   , "GPCLK0"     , "SPI0_SCLK" , "SD1_CMD"       , "ARM_RTCK"        , "SPI2_MOSI"   , },
    { "SD0_DAT0"  , "GPCLK1"     , "PCM_CLK"   , "SD1_DAT0"      , "ARM_TDO"         , "SPI2_MISO"   , },
    { "SD0_DAT1"  , "GPCLK2"     , "PCM_FS"    , "SD1_DAT1"      , "ARM_TCK"         , "SD_CARD_LED" , },
    { "SD0_DAT2"  , "PWM0_0"     , "PCM_DIN"   , "SD1_DAT2"      , "ARM_TDI"         , 0             , },
    { "SD0_DAT3"  , "PWM0_1"     , "PCM_DOUT"  , "SD1_DAT3"      , "ARM_TMS"         , 0             , },
};

static GPIO_FSEL_T bcm2835_gpio_get_fsel(void *priv, unsigned gpio)
{
    volatile uint32_t *base = priv;
    /* GPFSEL0-5 with 10 sels per reg, 3 bits per sel (so bits 0:29 used) */
    uint32_t reg = GPFSEL0 + (gpio / 10);
    uint32_t lsb = (gpio % 10) * 3;

    if (gpio < BCM2835_NUM_GPIOS)
    {
        switch ((base[reg] >> lsb) & 7)
        {
        case 0: return GPIO_FSEL_INPUT;
        case 1: return GPIO_FSEL_OUTPUT;
        case 2: return GPIO_FSEL_FUNC5;
        case 3: return GPIO_FSEL_FUNC4;
        case 4: return GPIO_FSEL_FUNC0;
        case 5: return GPIO_FSEL_FUNC1;
        case 6: return GPIO_FSEL_FUNC2;
        case 7: return GPIO_FSEL_FUNC3;
        }
    }

    return GPIO_FSEL_MAX;
}

static void bcm2835_gpio_set_fsel(void *priv, unsigned gpio, const GPIO_FSEL_T func)
{
    volatile uint32_t *base = priv;
    /* GPFSEL0-5 with 10 sels per reg, 3 bits per sel (so bits 0:29 used) */
    uint32_t reg = GPFSEL0 + (gpio / 10);
    uint32_t lsb = (gpio % 10) * 3;
    int fsel;

    switch (func)
    {
    case GPIO_FSEL_INPUT: fsel = 0; break;
    case GPIO_FSEL_OUTPUT: fsel = 1; break;
    case GPIO_FSEL_FUNC0: fsel = 4; break;
    case GPIO_FSEL_FUNC1: fsel = 5; break;
    case GPIO_FSEL_FUNC2: fsel = 6; break;
    case GPIO_FSEL_FUNC3: fsel = 7; break;
    case GPIO_FSEL_FUNC4: fsel = 3; break;
    case GPIO_FSEL_FUNC5: fsel = 2; break;
    default:
        return;
    }

    if (gpio < BCM2835_NUM_GPIOS)
        base[reg] = (base[reg] & ~(0x7 << lsb)) | (fsel << lsb);
}

static GPIO_DIR_T bcm2835_gpio_get_dir(void *priv, unsigned gpio)
{
    GPIO_FSEL_T fsel = bcm2835_gpio_get_fsel(priv, gpio);
    if (fsel == GPIO_FSEL_INPUT)
        return DIR_INPUT;
    else if (fsel == GPIO_FSEL_OUTPUT)
        return DIR_OUTPUT;
    else
        return DIR_MAX;
}

static void bcm2835_gpio_set_dir(void *priv, unsigned gpio, GPIO_DIR_T dir)
{
    GPIO_FSEL_T fsel;
    if (dir == DIR_INPUT)
        fsel = GPIO_FSEL_INPUT;
    else if (dir == DIR_OUTPUT)
        fsel = GPIO_FSEL_OUTPUT;
    else
        return;

    bcm2835_gpio_set_fsel(priv, gpio, fsel);
}

static int bcm2835_gpio_get_level(void *priv, unsigned gpio)
{
    volatile uint32_t *base = priv;

    if (gpio >= BCM2835_NUM_GPIOS)
        return -1;

    return (base[GPLEV0 + (gpio / 32)] >> (gpio % 32)) & 1;
}

GPIO_DRIVE_T bcm2835_gpio_get_drive(void *priv, unsigned gpio)
{
    /* This is a write-only mechanism */
    UNUSED(priv);
    UNUSED(gpio);
    return DRIVE_MAX;
}

static void bcm2835_gpio_set_drive(void *priv, unsigned gpio, GPIO_DRIVE_T drv)
{
    volatile uint32_t *base = priv;

    if (gpio < BCM2835_NUM_GPIOS && drv <= DRIVE_HIGH)
        base[(drv ? GPSET0 : GPCLR0) + (gpio / 32)] = (1 << (gpio % 32));
}

static GPIO_PULL_T bcm2835_gpio_get_pull(void *priv, unsigned gpio)
{
    /* This is a write-only mechanism */
    UNUSED(priv);
    UNUSED(gpio);
    return PULL_MAX;
}

static void bcm2835_gpio_set_pull(void *priv, unsigned gpio, GPIO_PULL_T pull)
{
    volatile uint32_t *base = priv;
    int clkreg = GPPUDCLK0 + (gpio / 32);
    int clkbit = 1 << (gpio % 32);

    if (gpio >= BCM2835_NUM_GPIOS || pull < PULL_NONE || pull > PULL_UP)
        return;

    base[GPPUD] = pull;
    usleep(10);
    base[clkreg] = clkbit;
    usleep(10);
    base[GPPUD] = 0;
    usleep(10);
    base[clkreg] = 0;
    usleep(10);
}

static const char *bcm2835_gpio_get_name(void *priv, unsigned gpio)
{
    static char name_buf[16];
    UNUSED(priv);
    sprintf(name_buf, "GPIO%d", gpio);
    return name_buf;
}

static const char *bcm2835_gpio_get_fsel_name(void *priv, unsigned gpio, GPIO_FSEL_T fsel)
{
    const char *name = NULL;
    UNUSED(priv);
    switch (fsel)
    {
    case GPIO_FSEL_INPUT:
        name = "input";
        break;
    case GPIO_FSEL_OUTPUT:
        name = "output";
        break;
    case GPIO_FSEL_FUNC0:
    case GPIO_FSEL_FUNC1:
    case GPIO_FSEL_FUNC2:
    case GPIO_FSEL_FUNC3:
    case GPIO_FSEL_FUNC4:
    case GPIO_FSEL_FUNC5:
        if (gpio < BCM2835_NUM_GPIOS)
        {
            name = bcm2835_gpio_alt_names[gpio][fsel];
            if (!name)
                name = "-";
        }
        break;
    default:
        break;
    }
    return name;
}

static GPIO_PULL_T bcm2711_gpio_get_pull(void *priv, unsigned gpio)
{
    volatile uint32_t *base = priv;
    int reg = GPPUPPDN0 + (gpio / 16);
    int lsb = (gpio % 16) * 2;

    if (gpio < BCM2711_NUM_GPIOS)
    {
        switch ((base[reg] >> lsb) & 3)
        {
        case 0: return PULL_NONE;
        case 1: return PULL_UP;
        case 2: return PULL_DOWN;
        }
    }

    return PULL_MAX;
}

static void bcm2711_gpio_set_pull(void *priv, unsigned gpio, GPIO_PULL_T pull)
{
    volatile uint32_t *base = priv;
    int reg = GPPUPPDN0 + (gpio / 16);
    int lsb = (gpio % 16) * 2;
    int pull_val;

    if (gpio >= BCM2711_NUM_GPIOS)
        return;

    switch (pull)
    {
    case PULL_NONE:
        pull_val = 0;
        break;
    case PULL_UP:
        pull_val = 1;
        break;
    case PULL_DOWN:
        pull_val = 2;
        break;
    default:
        return;
    }

    base[reg] = (base[reg] & ~(3 << lsb)) | (pull_val << lsb);
}

static const char *bcm2711_gpio_get_fsel_name(void *priv, unsigned gpio, GPIO_FSEL_T fsel)
{
    const char *name = NULL;
    UNUSED(priv);
    switch (fsel)
    {
    case GPIO_FSEL_INPUT:
        name = "input";
        break;
    case GPIO_FSEL_OUTPUT:
        name = "output";
        break;
    case GPIO_FSEL_FUNC0:
    case GPIO_FSEL_FUNC1:
    case GPIO_FSEL_FUNC2:
    case GPIO_FSEL_FUNC3:
    case GPIO_FSEL_FUNC4:
    case GPIO_FSEL_FUNC5:
        if (gpio < BCM2711_NUM_GPIOS)
        {
            name = bcm2711_gpio_alt_names[gpio][fsel];
            if (!name)
                name = "-";
        }
        break;
    default:
        break;
    }
    return name;
}

static void *bcm2835_gpio_create_instance(const GPIO_CHIP_T *chip,
                                          const char *dtnode)
{
    UNUSED(dtnode);
    return (void *)chip;
}

static int bcm2835_gpio_count(void *priv)
{
    UNUSED(priv);
    return BCM2835_NUM_GPIOS;
}

static void *bcm2835_gpio_probe_instance(void *priv, volatile uint32_t *base)
{
    UNUSED(priv);
    return (void *)base;
}

static const GPIO_CHIP_INTERFACE_T bcm2835_gpio_interface =
{
    .gpio_create_instance = bcm2835_gpio_create_instance,
    .gpio_count = bcm2835_gpio_count,
    .gpio_probe_instance = bcm2835_gpio_probe_instance,
    .gpio_get_fsel = bcm2835_gpio_get_fsel,
    .gpio_set_fsel = bcm2835_gpio_set_fsel,
    .gpio_set_drive = bcm2835_gpio_set_drive,
    .gpio_set_dir = bcm2835_gpio_set_dir,
    .gpio_get_dir = bcm2835_gpio_get_dir,
    .gpio_get_level = bcm2835_gpio_get_level,
    .gpio_get_drive = bcm2835_gpio_get_drive,
    .gpio_get_pull = bcm2835_gpio_get_pull,
    .gpio_set_pull = bcm2835_gpio_set_pull,
    .gpio_get_name = bcm2835_gpio_get_name,
    .gpio_get_fsel_name = bcm2835_gpio_get_fsel_name,
};

DECLARE_GPIO_CHIP(bcm2835, "brcm,bcm2835-gpio", &bcm2835_gpio_interface,
                  0x30000, 0);

static const GPIO_CHIP_INTERFACE_T bcm2711_gpio_interface =
{
    .gpio_create_instance = bcm2835_gpio_create_instance,
    .gpio_count = bcm2835_gpio_count,
    .gpio_probe_instance = bcm2835_gpio_probe_instance,
    .gpio_get_fsel = bcm2835_gpio_get_fsel,
    .gpio_set_fsel = bcm2835_gpio_set_fsel,
    .gpio_set_drive = bcm2835_gpio_set_drive,
    .gpio_set_dir = bcm2835_gpio_set_dir,
    .gpio_get_dir = bcm2835_gpio_get_dir,
    .gpio_get_level = bcm2835_gpio_get_level,
    .gpio_get_drive = bcm2835_gpio_get_drive,
    .gpio_get_pull = bcm2711_gpio_get_pull,
    .gpio_set_pull = bcm2711_gpio_set_pull,
    .gpio_get_name = bcm2835_gpio_get_name,
    .gpio_get_fsel_name = bcm2711_gpio_get_fsel_name,
};

DECLARE_GPIO_CHIP(bcm2711, "brcm,bcm2711-gpio",
                  &bcm2711_gpio_interface, 0x30000, 0);
