#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "gpiochip.h"
#include "util.h"

#define ARRAY_SIZE(_a) (sizeof(_a)/sizeof(_a[0]))

/* 2712 definitions */

#define BCM2712_GIO_DATA       0x04
#define BCM2712_GIO_IODIR      0x08

#define BCM2712_PAD_PULL_OFF   0
#define BCM2712_PAD_PULL_DOWN  1
#define BCM2712_PAD_PULL_UP    2

#define BCM2712_MAX_INSTANCES  2
#define BCM2712_FSEL_COUNT     9

#define FLAGS_AON              1
#define FLAGS_C0               2
#define FLAGS_D0               4
#define FLAGS_GPIO             8
#define FLAGS_PINCTRL          16

struct bcm2712_inst
{
    volatile uint32_t *gpio_base;
    volatile uint32_t *pinmux_base;
    unsigned pad_offset;
    uint32_t *bank_widths;
    unsigned flags;
    unsigned num_gpios;
    unsigned num_banks;
};

static unsigned num_instances;
static struct bcm2712_inst bcm2712_instances[BCM2712_MAX_INSTANCES] = { 0 };
static unsigned shared_flags;

static const char *bcm2712_c0_gpio_alt_names[][BCM2712_FSEL_COUNT - 1] =
{
    { "BSC_M3_SDA"             , "VC_SDA0"          , "GPCLK0"           , "ENET0_LINK"        , "VC_PWM1_0"             , "VC_SPI0_CE1_N"         , "IR_IN"          , }, // 0
    { "BSC_M3_SCL"             , "VC_SCL0"          , "GPCLK1"           , "ENET0_ACTIVITY"    , "VC_PWM1_1"             , "SR_EDM_SENSE"          , "VC_SPI0_CE0_N"  , "VC_TXD3"       , }, // 1
    { "PDM_CLK"                , "I2S_CLK0_IN"      , "GPCLK2"           , "VC_SPI4_CE1_N"     , "PKT_CLK0"              , "VC_SPI0_MISO"          , "VC_RXD3"        , }, // 2
    { "PDM_DATA0"              , "I2S_LR0_IN"       , "VC_SPI4_CE0_N"    , "PKT_SYNC0"         , "VC_SPI0_MOSI"          , "VC_CTS3"               , }, // 3
    { "PDM_DATA1"              , "I2S_DATA0_IN"     , "ARM_RTCK"         , "VC_SPI4_MISO"      , "PKT_DATA0"             , "VC_SPI0_SCLK"          , "VC_RTS3"        , }, // 4
    { "PDM_DATA2"              , "VC_SCL3"          , "ARM_TRST"         , "SD_CARD_LED_E"     , "VC_SPI4_MOSI"          , "PKT_CLK1"              , "VC_PCM_CLK"     , "VC_SDA5"       , }, // 5
    { "PDM_DATA3"              , "VC_SDA3"          , "ARM_TCK"          , "SD_CARD_WPROT_E"   , "VC_SPI4_SCLK"          , "PKT_SYNC1"             , "VC_PCM_FS"      , "VC_SCL5"       , }, // 6
    { "I2S_CLK0_OUT"           , "SPDIF_OUT"        , "ARM_TDI"          , "SD_CARD_PRES_E"    , "VC_SDA3"               , "ENET0_RGMII_START_STOP", "VC_PCM_DIN"     , "VC_SPI4_CE1_N" , }, // 7
    { "I2S_LR0_OUT"            , "AUD_FS_CLK0"      , "ARM_TMS"          , "SD_CARD_VOLT_E"    , "VC_SCL3"               , "ENET0_MII_TX_ERR"      , "VC_PCM_DOUT"    , "VC_SPI4_CE0_N" , }, // 8
    { "I2S_DATA0_OUT"          , "AUD_FS_CLK0"      , "ARM_TDO"          , "SD_CARD_PWR0_E"    , "ENET0_MII_RX_ERR"      , "SD_CARD_VOLT_C"        , "VC_SPI4_SCLK"   , }, // 9
    { "BSC_M3_SCL"             , "MTSIF_DATA4_ALT1" , "I2S_CLK0_IN"      , "I2S_CLK0_OUT"      , "VC_SPI5_CE1_N"         , "ENET0_MII_CRS"         , "SD_CARD_PWR0_C" , "VC_SPI4_MOSI"  , }, // 10
    { "BSC_M3_SDA"             , "MTSIF_DATA5_ALT1" , "I2S_LR0_IN"       , "I2S_LR0_OUT"       , "VC_SPI5_CE0_N"         , "ENET0_MII_COL"         , "SD_CARD_PRES_C" , "VC_SPI4_MISO"  , }, // 11
    { "SPI_S_SS0B"             , "MTSIF_DATA6_ALT1" , "I2S_DATA0_IN"     , "I2S_DATA0_OUT"     , "VC_SPI5_MISO"          , "VC_I2CSL_MOSI"         , "SD0_CLK"        , "SD_CARD_VOLT_D", }, // 12
    { "SPI_S_MISO"             , "MTSIF_DATA7_ALT1" , "I2S_DATA1_OUT"    , "USB_VBUS_PRESENT"  , "VC_SPI5_MOSI"          , "VC_I2CSL_CE_N"         , "SD0_CMD"        , "SD_CARD_PWR0_D", }, // 13
    { "SPI_S_MOSI_OR_BSC_S_SDA", "VC_I2CSL_SCL_SCLK", "ENET0_RGMII_RX_OK", "ARM_TCK"           , "VC_SPI5_SCLK"          , "VC_PWM0_0"             , "VC_SDA4"        , "SD_CARD_PRES_D", }, // 14
    { "SPI_S_SCK_OR_BSC_S_SCL" , "VC_I2CSL_SDA_MISO", "VC_SPI3_CE1_N"    , "ARM_TMS"           , "VC_PWM0_1"             , "VC_SCL4"               , "GPCLK0"         , }, // 15
    { "SD_CARD_PRES_B"         , "I2S_CLK0_OUT"     , "VC_SPI3_CE0_N"    , "I2S_CLK0_IN"       , "SD0_DAT0"              , "ENET0_RGMII_MDIO"      , "GPCLK1"         , }, // 16
    { "SD_CARD_WPROT_B"        , "I2S_LR0_OUT"      , "VC_SPI3_MISO"     , "I2S_LR0_IN"        , "EXT_SC_CLK"            , "SD0_DAT1"              , "ENET0_RGMII_MDC", "GPCLK2"        , }, // 17
    { "SD_CARD_LED_B"          , "I2S_DATA0_OUT"    , "VC_SPI3_MOSI"     , "I2S_DATA0_IN"      , "SD0_DAT2"              , "ENET0_RGMII_IRQ"       , "VC_PWM1_0"      , }, // 18
    { "SD_CARD_VOLT_B"         , "USB_PWRFLT"       , "VC_SPI3_SCLK"     , "PKT_DATA1"         , "SPDIF_OUT"             , "SD0_DAT3"              , "IR_IN"          , "VC_PWM1_1"     , }, // 19
    { "SD_CARD_PWR0_B"         , "UUI_TXD"          , "VC_TXD0"          , "ARM_TMS"           , "UART_TXD_2"            , "USB_PWRON"             , "VC_PCM_CLK"     , "VC_TXD4"       , }, // 20
    { "USB_PWRFLT"             , "UUI_RXD"          , "VC_RXD0"          , "ARM_TCK"           , "UART_RXD_2"            , "SD_CARD_VOLT_B"        , "VC_PCM_FS"      , "VC_RXD4"       , }, // 21
    { "USB_PWRON"              , "ENET0_LINK"       , "VC_CTS0"          , "MTSIF_ATS_RST"     , "UART_RTS_2"            , "USB_VBUS_PRESENT"      , "VC_PCM_DIN"     , "VC_SDA5"       , }, // 22
    { "USB_VBUS_PRESENT"       , "ENET0_ACTIVITY"   , "VC_RTS0"          , "MTSIF_ATS_INC"     , "UART_CTS_2"            , "I2S_DATA2_OUT"         , "VC_PCM_DOUT"    , "VC_SCL5"       , }, // 23
    { "MTSIF_ATS_RST"          , "PKT_CLK0"         , "UART_RTS_0"       , "ENET0_RGMII_RX_CLK", "ENET0_RGMII_START_STOP", "VC_SDA4"               , "VC_TXD3"        , }, // 24
    { "MTSIF_ATS_INC"          , "PKT_SYNC0"        , "SC0_CLK"          , "UART_CTS_0"        , "ENET0_RGMII_RX_EN_CTL" , "ENET0_RGMII_RX_OK"     , "VC_SCL4"        , "VC_RXD3"       , }, // 25
    { "MTSIF_DATA1"            , "PKT_DATA0"        , "SC0_IO"           , "UART_TXD_0"        , "ENET0_RGMII_RXD_00"    , "VC_TXD4"               , "VC_SPI5_CE0_N"  , }, // 26
    { "MTSIF_DATA2"            , "PKT_CLK1"         , "SC0_AUX1"         , "UART_RXD_0"        , "ENET0_RGMII_RXD_01"    , "VC_RXD4"               , "VC_SPI5_SCLK"   , }, // 27
    { "MTSIF_CLK"              , "PKT_SYNC1"        , "SC0_AUX2"         , "ENET0_RGMII_RXD_02", "VC_CTS4"               , "VC_SPI5_MOSI"          , }, // 28
    { "MTSIF_DATA0"            , "PKT_DATA1"        , "SC0_PRES"         , "ENET0_RGMII_RXD_03", "VC_RTS4"               , "VC_SPI5_MISO"          , }, // 29
    { "MTSIF_SYNC"             , "PKT_CLK2"         , "SC0_RST"          , "SD2_CLK"           , "ENET0_RGMII_TX_CLK"    , "GPCLK0"                , "VC_PWM0_0"      , }, // 30
    { "MTSIF_DATA3"            , "PKT_SYNC2"        , "SC0_VCC"          , "SD2_CMD"           , "ENET0_RGMII_TX_EN_CTL" , "VC_SPI3_CE1_N"         , "VC_PWM0_1"      , }, // 31
    { "MTSIF_DATA4"            , "PKT_DATA2"        , "SC0_VPP"          , "SD2_DAT0"          , "ENET0_RGMII_TXD_00"    , "VC_SPI3_CE0_N"         , "VC_TXD3"        , }, // 32
    { "MTSIF_DATA5"            , "PKT_CLK3"         , "SD2_DAT1"         , "ENET0_RGMII_TXD_01", "VC_SPI3_SCLK"          , "VC_RXD3"               , }, // 33
    { "MTSIF_DATA6"            , "PKT_SYNC3"        , "EXT_SC_CLK"       , "SD2_DAT2"          , "ENET0_RGMII_TXD_02"    , "VC_SPI3_MOSI"          , "VC_SDA5"        , }, // 34
    { "MTSIF_DATA7"            , "PKT_DATA3"        , "SD2_DAT3"         , "ENET0_RGMII_TXD_03", "VC_SPI3_MISO"          , "VC_SCL5"               , }, // 35
    { "SD0_CLK"                , "MTSIF_ATS_RST"    , "SC0_RST"          , "I2S_DATA1_IN"      , "VC_TXD3"               , "VC_TXD2"               , }, // 36
    { "SD0_CMD"                , "MTSIF_ATS_INC"    , "SC0_VCC"          , "VC_SPI0_CE1_N"     , "I2S_DATA2_IN"          , "VC_RXD3"               , "VC_RXD2"        , }, // 37
    { "SD0_DAT0"               , "MTSIF_DATA4_ALT"  , "SC0_VPP"          , "VC_SPI0_CE0_N"     , "I2S_DATA3_IN"          , "VC_CTS3"               , "VC_RTS2"        , }, // 38
    { "SD0_DAT1"               , "MTSIF_DATA5_ALT"  , "SC0_CLK"          , "VC_SPI0_MISO"      , "VC_RTS3"               , "VC_CTS2"               , }, // 39
    { "SD0_DAT2"               , "MTSIF_DATA6_ALT"  , "SC0_IO"           , "VC_SPI0_MOSI"      , "BSC_M3_SDA"            , }, // 40
    { "SD0_DAT3"               , "MTSIF_DATA7_ALT"  , "SC0_PRES"         , "VC_SPI0_SCLK"      , "BSC_M3_SCL"            , }, // 41
    { "VC_SPI0_CE1_N"          , "MTSIF_CLK_ALT"    , "VC_SDA0"          , "SD_CARD_PRES_A"    , "MTSIF_CLK_ALT1"        , "ARM_TRST"              , "PDM_CLK"        , "SPI_M_SS1B"    , }, // 42
    { "VC_SPI0_CE0_N"          , "MTSIF_SYNC_ALT"   , "VC_SCL0"          , "SD_CARD_PWR0_A"    , "MTSIF_SYNC_ALT1"       , "ARM_RTCK"              , "PDM_DATA0"      , "SPI_M_SS0B"    , }, // 43
    { "VC_SPI0_MISO"           , "MTSIF_DATA0_ALT"  , "ENET0_LINK"       , "SD_CARD_LED_A"     , "MTSIF_DATA0_ALT1"      , "ARM_TDO"               , "PDM_DATA1"      , "SPI_M_MISO"    , }, // 44
    { "VC_SPI0_MOSI"           , "MTSIF_DATA1_ALT"  , "ENET0_ACTIVITY"   , "SD_CARD_VOLT_A"    , "MTSIF_DATA1_ALT1"      , "ARM_TCK"               , "PDM_DATA2"      , "SPI_M_MOSI"    , }, // 45
    { "VC_SPI0_SCLK"           , "MTSIF_DATA2_ALT"  , "SD_CARD_WPROT_A"  , "MTSIF_DATA2_ALT1"  , "ARM_TDI"               , "PDM_DATA3"             , "SPI_M_SCK"      , }, // 46
    { "ENET0_ACTIVITY"         , "MTSIF_DATA3_ALT"  , "I2S_DATA3_OUT"    , "MTSIF_DATA3_ALT1"  , "ARM_TMS"               , }, // 47
    { "SC0_RST"                , "USB_PWRFLT"       , "SPDIF_OUT"        , "MTSIF_ATS_RST"     , }, // 48
    { "SC0_VCC"                , "USB_PWRON"        , "AUD_FS_CLK0"      , "MTSIF_ATS_INC"     , }, // 49
    { "SC0_VPP"                , "USB_VBUS_PRESENT" , "SC0_AUX1"         , }, // 50
    { "SC0_CLK"                , "ENET0_LINK"       , "SC0_AUX2"         , "SR_EDM_SENSE"      , }, // 51
    { "SC0_IO"                 , "ENET0_ACTIVITY"   , "VC_PWM1_1"        , }, // 52
    { "SC0_PRES"               , "ENET0_RGMII_RX_OK", "EXT_SC_CLK"       , }, // 53
};

static const char *bcm2712_d0_gpio_alt_names[][BCM2712_FSEL_COUNT - 1] =
{
    { "" }, // 0
    { "VC_SCL0"                , "USB_PWRFLT"       , "GPCLK0"           , "SD_CARD_LED_E"    , "VC_SPI3_CE1_N"          , "SR_EDM_SENSE"          , "VC_SPI0_CE0_N"    , "VC_TXD0"       , }, // 1
    { "VC_SDA0"                , "USB_PWRON"        , "GPCLK1"           , "SD_CARD_WPROT_E"  , "VC_SPI3_CE0_N"          , "CLK_OBSERVE"           , "VC_SPI0_MISO"     , "VC_RXD0"       , }, // 2
    { "VC_SCL3"                , "USB_VBUS_PRESENT" , "GPCLK2"           , "SD_CARD_PRES_E"   , "VC_SPI3_MISO"           , "VC_SPI0_MOSI"          , "VC_CTS0"          , }, // 3
    { "VC_SDA3"                , "VC_PWM1_1"        , "VC_SPI3_CE0_N"    , "SD_CARD_VOLT_E"   , "VC_SPI3_MOSI"           , "VC_SPI0_SCLK"          , "VC_RTS0"          , }, // 4
    { "" }, // 5
    { "" }, // 6
    { "" }, // 7
    { "" }, // 8
    { "" }, // 9
    { "BSC_M3_SCL"             , "VC_PWM1_0"        , "VC_SPI3_CE1_N"    , "SD_CARD_PWR0_E"    , "VC_SPI3_SCLK"          , "GPCLK0"                , }, // 10
    { "BSC_M3_SDA"             , "VC_SPI3_MISO"     , "CLK_OBSERVE"      , "SD_CARD_PRES_C"    , "GPCLK1"                , }, // 11
    { "SPI_S_SS0B"             , "VC_SPI3_MOSI"     , "SD_CARD_PWR0_C"   , "SD_CARD_VOLT_D"    , }, // 12
    { "SPI_S_MISO"             , "VC_SPI3_SCLK"     , "SD_CARD_PRES_C"   , "SD_CARD_PWR0_D"    , }, // 13
    { "SPI_S_MOSI_OR_BSC_S_SDA", "UUI_TXD"          , "ARM_TCK"          , "VC_PWM0_0"         , "VC_SDA0"               , "SD_CARD_PRES_D"        , }, // 14
    { "SPI_S_SCK_OR_BSC_S_SCL" , "UUI_RXD"          , "ARM_TMS"          , "VC_PWM0_1"         , "VC_SCL0"               , "GPCLK0"                , }, // 15
    { "" }, // 16
    { "" }, // 17
    { "SD_CARD_PRES_F"         , "VC_PWM1_0"        , }, // 18
    { "SD_CARD_PWR0_F"         , "USB_PWRFLT"       , "VC_PWM1_1"        , }, // 19
    { "VC_SDA3"                , "UUI_TXD"          , "VC_TXD0"          , "ARM_TMS"           , "VC_TXD2"               , }, // 20
    { "VC_SCL3"                , "UUI_RXD"          , "VC_RXD0"          , "ARM_TCK"           , "VC_RXD2"               , }, // 21
    { "SD_CARD_PRES_F"         , "VC_CTS0"          , "VC_SDA3"          , }, // 22
    { "VC_RTS0"                , "VC_SCL3"          , }, // 23
    { "SD_CARD_PRES_B"         , "VC_SPI0_CE1_N"    , "ARM_TRST"         , "UART_RTS_0"        , "USB_PWRFLT"            , "VC_RTS2"               , "VC_TXD0"        , }, // 24
    { "SD_CARD_WPROT_B"        , "VC_SPI0_CE0_N"    , "ARM_TCK"          , "UART_CTS_0"        , "USB_PWRON"             , "VC_CTS2"               , "VC_RXD0"        , }, // 25
    { "SD_CARD_LED_B"          , "VC_SPI0_MISO"     , "ARM_TDI"          , "UART_TXD_0"        , "USB_VBUS_PRESENT"      , "VC_TXD2"               , "VC_SPI0_CE0_N"  , }, // 26
    { "SD_CARD_VOLT_B"         , "VC_SPI0_MOSI"     , "ARM_TMS"          , "UART_RXD_0"        , "VC_RXD2"               , "VC_SPI0_SCLK"          , }, // 27
    { "SD_CARD_PWR0_B"         , "VC_SPI0_SCLK"     , "ARM_TDO"          , "VC_SDA0"           , "VC_SPI0_MOSI"          , }, // 28
    { "ARM_RTCK"               , "VC_SCL0"          , "VC_SPI0_MISO"     , }, // 29
    { "SD2_CLK"                , "GPCLK0"           , "VC_PWM0_0"        , }, // 30
    { "SD2_CMD"                , "VC_SPI3_CE1_N"    , "VC_PWM0_1"        , }, // 31
    { "SD2_DAT0"               , "VC_SPI3_CE0_N"    , "VC_TXD3"          , }, // 32
    { "SD2_DAT1"               , "VC_SPI3_SCLK"     , "VC_RXD3"          , }, // 33
    { "SD2_DAT2"               , "VC_SPI3_MOSI"     , "VC_SDA5"          , }, // 34
    { "SD2_DAT3"               , "VC_SPI3_MISO"     , "VC_SCL5"          , }, // 35
};

static const char *bcm2712_c0_aon_gpio_alt_names[][BCM2712_FSEL_COUNT - 1] =
{
    { "IR_IN"           , "VC_SPI0_CE1_N"     , "VC_TXD3"          , "VC_SDA3"           , "TE0"          , "VC_SDA0"         , }, // 0
    { "VC_PWM0_0"       , "VC_SPI0_CE0_N"     , "VC_RXD3"          , "VC_SCL3"           , "TE1"          , "AON_PWM0"        , "VC_SCL0"       , "VC_PWM1_0"     , }, // 1
    { "VC_PWM0_1"       , "VC_SPI0_MISO"      , "VC_CTS3"          , "CTL_HDMI_5V"       , "FL0"          , "AON_PWM1"        , "IR_IN"         , "VC_PWM1_1"     , }, // 2
    { "IR_IN"           , "VC_SPI0_MOSI"      , "VC_RTS3"          , "AON_FP_4SEC_RESETB", "FL1"          , "SD_CARD_VOLT_G"  , "AON_GPCLK"     , }, // 3
    { "GPCLK0"          , "VC_SPI0_SCLK"      , "VC_I2CSL_SCL_SCLK", "AON_GPCLK"         , "PM_LED_OUT"   , "AON_PWM0"        , "SD_CARD_PWR0_G", "VC_PWM0_0"     , }, // 4
    { "GPCLK1"          , "IR_IN"             , "VC_I2CSL_SDA_MISO", "CLK_OBSERVE"       , "AON_PWM1"     , "SD_CARD_PRES_G"  , "VC_PWM0_1"     , }, // 5
    { "UART_TXD_1"      , "VC_TXD4"           , "GPCLK2"           , "CTL_HDMI_5V"       , "VC_TXD0"      , "VC_SPI3_CE0_N"   , }, // 6
    { "UART_RXD_1"      , "VC_RXD4"           , "GPCLK0"           , "AON_PWM0"          , "VC_RXD0"      , "VC_SPI3_SCLK"    , }, // 7
    { "UART_RTS_1"      , "VC_RTS4"           , "VC_I2CSL_MOSI"    , "CTL_HDMI_5V"       , "VC_RTS0"      , "VC_SPI3_MOSI"    , }, // 8
    { "UART_CTS_1"      , "VC_CTS4"           , "VC_I2CSL_CE_N"    , "AON_PWM1"          , "VC_CTS0"      , "VC_SPI3_MISO"    , }, // 9
    { "TSIO_CLK_OUT"    , "CTL_HDMI_5V"       , "SC0_AUX1"         , "SPDIF_OUT"         , "VC_SPI5_CE1_N", "USB_PWRFLT"      , "AON_GPCLK"     , "SD_CARD_VOLT_F", }, // 10
    { "TSIO_DATA_IN"    , "UART_RTS_0"        , "SC0_AUX2"         , "AUD_FS_CLK0"       , "VC_SPI5_CE0_N", "USB_VBUS_PRESENT", "VC_RTS2"       , "SD_CARD_PWR0_F", }, // 11
    { "TSIO_DATA_OUT"   , "UART_CTS_0"        , "VC_RTS0"          , "TSIO_VCTRL"        , "VC_SPI5_MISO" , "USB_PWRON"       , "VC_CTS2"       , "SD_CARD_PRES_F", }, // 12
    { "BSC_M1_SDA"      , "UART_TXD_0"        , "VC_TXD0"          , "UUI_TXD"           , "VC_SPI5_MOSI" , "ARM_TMS"         , "VC_TXD2"       , "VC_SDA3"       , }, // 13
    { "BSC_M1_SCL"      , "UART_RXD_0"        , "VC_RXD0"          , "UUI_RXD"           , "VC_SPI5_SCLK" , "ARM_TCK"         , "VC_RXD2"       , "VC_SCL3"       , }, // 14
    { "IR_IN"           , "AON_FP_4SEC_RESETB", "VC_CTS0"          , "PM_LED_OUT"        , "CTL_HDMI_5V"  , "AON_PWM0"        , "AON_GPCLK"     , }, // 15
    { "AON_CPU_STANDBYB", "GPCLK0"            , "PM_LED_OUT"       , "CTL_HDMI_5V"       , "VC_PWM0_0"    , "USB_PWRON"       , "AUD_FS_CLK0"   , }, // 16

    // Pad out the bank to 32 entries
    { "" }, { "" }, { "" }, { "" }, { "" }, { "" }, { "" }, // 17-23
    { "" }, { "" }, { "" }, { "" }, { "" }, { "" }, { "" }, { "" }, // 24-31

    { "HDMI_TX0_BSC_SCL", "HDMI_TX0_AUTO_I2C_SCL", "BSC_M0_SCL", "VC_SCL0", }, // sgpio 0
    { "HDMI_TX0_BSC_SDA", "HDMI_TX0_AUTO_I2C_SDA", "BSC_M0_SDA", "VC_SDA0", }, // sgpio 1
    { "HDMI_TX1_BSC_SCL", "HDMI_TX1_AUTO_I2C_SCL", "BSC_M1_SCL", "VC_SCL4", "CTL_HDMI_5V", }, // sgpio 2
    { "HDMI_TX1_BSC_SDA", "HDMI_TX1_AUTO_I2C_SDA", "BSC_M1_SDA", "VC_SDA4", }, // sgpio 3
    { "AVS_PMU_BSC_SCL", "BSC_M2_SCL", "VC_SCL5", "CTL_HDMI_5V", }, // sgpio 4
    { "AVS_PMU_BSC_SDA", "BSC_M2_SDA", "VC_SDA5", }, // sgpio 5
};

static const char *bcm2712_d0_aon_gpio_alt_names[][BCM2712_FSEL_COUNT - 1] =
{
    { "IR_IN"           , "VC_SPI0_CE1_N"     , "VC_TXD0"          , "VC_SDA3"           , "UART_TXD_0"   , "VC_SDA0"         , }, // 0
    { "VC_PWM0_0"       , "VC_SPI0_CE0_N"     , "VC_RXD0"          , "VC_SCL3"           , "UART_RXD_0"   , "AON_PWM0"        , "VC_SCL0"       , "VC_PWM1_0"     , }, // 1
    { "VC_PWM0_1"       , "VC_SPI0_MISO"      , "VC_CTS0"          , "CTL_HDMI_5V"       , "UART_CTS_0"   , "AON_PWM1"        , "IR_IN"         , "VC_PWM1_1"     , }, // 2
    { "IR_IN"           , "VC_SPI0_MOSI"      , "VC_RTS0"          , "UART_RTS_0"        , "SD_CARD_VOLT_G"  , "AON_GPCLK"     , }, // 3
    { "GPCLK0"          , "VC_SPI0_SCLK"      , "PM_LED_OUT"       , "AON_PWM0"          , "SD_CARD_PWR0_G"  , "VC_PWM0_0"     , }, // 4
    { "GPCLK1"          , "IR_IN"             , "AON_PWM1"         , "SD_CARD_PRES_G"    , "VC_PWM0_1"       , }, // 5
    { "UART_TXD_1"      , "VC_TXD2"           , "CTL_HDMI_5V"      , "GPCLK2"            , "VC_SPI3_CE0_N"   , }, // 6
    { "" }, // 7
    { "UART_RTS_1"      , "VC_RTS2"           , "CTL_HDMI_5V"      , "VC_SPI0_CE1_N"     , "VC_SPI3_SCLK"    , }, // 8
    { "UART_CTS_1"      , "VC_CTS2"           , "VC_CTS0"          , "AON_PWM1"          , "VC_SPI0_CE0_N"   , "VC_RTS2"      , "VC_SPI3_MOSI"  , }, // 9
    { "" }, // 10
    { "" }, // 11
    { "UART_RXD_1"      , "VC_RXD2"           , "VC_RTS0"          , "VC_SPI0_MISO"      , "USB_PWRON"       , "VC_CTS2"      , "VC_SPI3_MISO"  , }, // 12
    { "BSC_M1_SDA"      , "VC_TXD0"           , "UUI_TXD"          , "VC_SPI0_MOSI"      , "ARM_TMS"         , "VC_TXD2"      , "VC_SDA3"       , }, // 13
    { "BSC_M1_SCL"      , "AON_GPCLK"         , "VC_RXD0"          , "UUI_RXD"           , "VC_SPI0_SCLK"    , "ARM_TCK"      , "VC_RXD2"       , "VC_SCL3"       , }, // 14

    // Pad out the bank to 32 entries
    { "" }, // 15
    { "" }, { "" }, { "" }, { "" }, { "" }, { "" }, { "" }, { "" }, // 16-23
    { "" }, { "" }, { "" }, { "" }, { "" }, { "" }, { "" }, { "" }, // 24-31

    { "HDMI_TX0_BSC_SCL", "HDMI_TX0_AUTO_I2C_SCL", "BSC_M0_SCL", "VC_SCL0", }, // sgpio 0
    { "HDMI_TX0_BSC_SDA", "HDMI_TX0_AUTO_I2C_SDA", "BSC_M0_SDA", "VC_SDA0", }, // sgpio 1
    { "HDMI_TX1_BSC_SCL", "HDMI_TX1_AUTO_I2C_SCL", "BSC_M1_SCL", "VC_SCL0", "CTL_HDMI_5V", }, // sgpio 2
    { "HDMI_TX1_BSC_SDA", "HDMI_TX1_AUTO_I2C_SDA", "BSC_M1_SDA", "VC_SDA0", }, // sgpio 3
    { "AVS_PMU_BSC_SCL", "BSC_M2_SCL", "VC_SCL3", "CTL_HDMI_5V", }, // sgpio 4
    { "AVS_PMU_BSC_SDA", "BSC_M2_SDA", "VC_SDA3", }, // sgpio 5
};

static const int bcm2712_gpio_d0_to_c0[] =
{
    -1, 0, 1, 2, 3, -1, -1, -1, -1, -1,
    4, 5, 6, 7, 8, 9, -1, -1, 10, 11,
    12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
    22, 23, 24, 25, 26, 27
};

static const int bcm2712_gpio_aon_d0_to_c0[] =
{
    0, 1, 2, 3, 4, 5, 6, -1, 7, 8,
    -1, -1, 9, 10, 11, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1,
    32, 33, 34, 35, 36, 37
};

static volatile uint32_t *bcm2712_gpio_base(struct bcm2712_inst *inst,
                                            unsigned gpio,
                                            unsigned int *bit)
{
    unsigned bank = gpio / 32;

    gpio %= 32;

    if ((bank >= inst->num_banks) || (gpio >= inst->bank_widths[bank]) ||
        !inst->gpio_base)
        return NULL;

    *bit = gpio;
    return inst->gpio_base + bank * (0x20 / 4);
}

static volatile uint32_t *bcm2712_pinmux_base(struct bcm2712_inst *inst,
                                              unsigned gpio,
                                              unsigned int *bit)
{
    unsigned bank, gpio_offset;

    if (gpio >= inst->num_gpios || !inst->pinmux_base)
        return NULL;

    if (inst->flags & FLAGS_D0)
    {
        if (inst->flags & FLAGS_AON)
            gpio = bcm2712_gpio_aon_d0_to_c0[gpio];
        else
            gpio = bcm2712_gpio_d0_to_c0[gpio];
        if ((int)gpio < 0)
            return NULL;
    }

    bank = gpio / 32;
    gpio_offset = gpio % 32;

    if ((bank >= inst->num_banks) || (gpio_offset >= inst->bank_widths[bank]))
        return NULL;

    if (inst->flags & FLAGS_AON)
    {
        if (bank == 1)
        {
            if (gpio_offset == 4)
            {
                *bit = 0;
                return inst->pinmux_base + 1;
            }
            else if (gpio_offset == 5)
            {
                *bit = 0;
                return inst->pinmux_base + 2;
            }
            else
            {
                *bit = gpio_offset * 4;
                return inst->pinmux_base;
            }
        }
        *bit = (gpio_offset % 8) * 4;
        return inst->pinmux_base + 3 + (gpio_offset / 8);
    }
    *bit = (gpio_offset % 8) * 4;
    return inst->pinmux_base + (bank * 4) + (gpio_offset / 8);
}

static volatile uint32_t *bcm2712_pad_base(struct bcm2712_inst *inst,
                                           unsigned gpio,
                                           unsigned int *bit)
{
    unsigned bank, gpio_offset;

    if (gpio >= inst->num_gpios || !inst->pinmux_base)
        return NULL;

    if (inst->flags & FLAGS_D0)
    {
        if (inst->flags & FLAGS_AON)
            gpio = bcm2712_gpio_aon_d0_to_c0[gpio];
        else
            gpio = bcm2712_gpio_d0_to_c0[gpio];
        if ((int)gpio < 0)
            return NULL;
    }

    bank = gpio / 32;
    gpio_offset = gpio % 32;

    if ((bank >= inst->num_banks) || (gpio_offset >= inst->bank_widths[bank]))
        return NULL;

    if ((inst->flags & FLAGS_AON) && (bank > 0))
    {
        /* There is no SGPIO pad control (that I know of) */
        return NULL;
    }

    gpio = gpio_offset + inst->pad_offset;
    *bit = (gpio % 15) * 2;
    return inst->pinmux_base + (gpio / 15);
}

static int bcm2712_gpio_get_level(void *priv, unsigned gpio)
{
    struct bcm2712_inst *inst = priv;
    unsigned int bit;
    volatile uint32_t *gpio_base = bcm2712_gpio_base(inst, gpio, &bit);

    if (!gpio_base)
        return -1;

    return !!(gpio_base[BCM2712_GIO_DATA / 4] & (1 << bit));
}

static void bcm2712_gpio_set_drive(void *priv, unsigned gpio, GPIO_DRIVE_T drv)
{
    struct bcm2712_inst *inst = priv;
    unsigned int bit;
    volatile uint32_t *gpio_base = bcm2712_gpio_base(inst, gpio, &bit);
    uint32_t gpio_val;

    if (!gpio_base)
        return;

    gpio_val = gpio_base[BCM2712_GIO_DATA / 4];
    gpio_val = (gpio_val & ~(1U << bit)) | (drv << bit);
    gpio_base[BCM2712_GIO_DATA / 4] = gpio_val;
}

static GPIO_DRIVE_T bcm2712_gpio_get_drive(void *priv, unsigned gpio)
{
    struct bcm2712_inst *inst = priv;
    unsigned int bit;
    volatile uint32_t *gpio_base = bcm2712_gpio_base(inst, gpio, &bit);
    uint32_t gpio_val;

    if (!gpio_base)
        return DRIVE_MAX;

    gpio_val = gpio_base[BCM2712_GIO_DATA / 4];
    return (gpio_val & (1U << bit)) ? DRIVE_HIGH : DRIVE_LOW;
}

static void bcm2712_gpio_set_dir(void *priv, unsigned gpio, GPIO_DIR_T dir)
{
    struct bcm2712_inst *inst = priv;
    unsigned int bit;
    volatile uint32_t *gpio_base = bcm2712_gpio_base(inst, gpio, &bit);
    uint32_t gpio_val;

    if (!gpio_base)
        return;

    gpio_val = gpio_base[BCM2712_GIO_IODIR / 4];
    gpio_val &= ~(1U << bit);
    gpio_val |= ((dir == DIR_INPUT) << bit);
    gpio_base[BCM2712_GIO_IODIR / 4] = gpio_val;
}

static GPIO_DIR_T bcm2712_gpio_get_dir(void *priv, unsigned gpio)
{
    struct bcm2712_inst *inst = priv;
    unsigned int bit;
    volatile uint32_t *gpio_base = bcm2712_gpio_base(inst, gpio, &bit);
    uint32_t gpio_val;

    if (!gpio_base)
        return DIR_MAX;

    gpio_val = gpio_base[BCM2712_GIO_IODIR / 4];
    return (gpio_val & (1U << bit)) ? DIR_INPUT : DIR_OUTPUT;
}

static GPIO_FSEL_T bcm2712_pinctrl_get_fsel(void *priv, unsigned gpio)
{
    struct bcm2712_inst *inst = priv;
    unsigned int pinmux_bit;
    volatile uint32_t *pinmux_base = bcm2712_pinmux_base(inst, gpio, &pinmux_bit);
    int fsel;

    if (!pinmux_base)
        return -1;

    fsel = ((*pinmux_base >> pinmux_bit) & 0xf);

    if (fsel == 0)
        return GPIO_FSEL_GPIO;
    else if (fsel < BCM2712_FSEL_COUNT)
        return GPIO_FSEL_FUNC1 + (fsel - 1);
    else if (fsel == 0xf) // Choose one value as a considered NONE
        return GPIO_FSEL_NONE;

    /* Unknown FSEL */
    return -1;
}

static void bcm2712_pinctrl_set_fsel(void *priv, unsigned gpio, const GPIO_FSEL_T func)
{
    struct bcm2712_inst *inst = priv;
    unsigned int pinmux_bit;
    volatile uint32_t *pinmux_base = bcm2712_pinmux_base(inst, gpio, &pinmux_bit);
    uint32_t pinmux_val;
    int fsel;

    if (!pinmux_base)
        return;

    if (func == GPIO_FSEL_INPUT || func == GPIO_FSEL_OUTPUT || func == GPIO_FSEL_GPIO)
    {
        // Set direction before switching
        // N.B. We explicitly interpret a request for FUNC_A0/GPIO as "last/current GPIO dir"
        fsel = 0;
        if (func == GPIO_FSEL_INPUT)
            bcm2712_gpio_set_dir(priv, gpio, DIR_INPUT);
        else if (func == GPIO_FSEL_OUTPUT)
            bcm2712_gpio_set_dir(priv, gpio, DIR_OUTPUT);
    }
    else if (func >= GPIO_FSEL_FUNC0 && func <= GPIO_FSEL_FUNC8)
    {
        fsel = func - GPIO_FSEL_FUNC0;
    }
    else
    {
        return;
    }

    pinmux_val = *pinmux_base;
    pinmux_val &= ~(0xf << pinmux_bit);
    pinmux_val |= (fsel << pinmux_bit);
    *pinmux_base = pinmux_val;
}

static GPIO_PULL_T bcm2712_pinctrl_get_pull(void *priv, unsigned gpio)
{
    struct bcm2712_inst *inst = priv;
    unsigned int bit;
    volatile uint32_t *pad_base = bcm2712_pad_base(inst, gpio, &bit);
    uint32_t pad_val;

    if (!pad_base)
        return PULL_MAX;

    pad_val = (*pad_base >> bit) & 0x3;
    switch (pad_val)
    {
    case BCM2712_PAD_PULL_OFF:
        return PULL_NONE;
    case BCM2712_PAD_PULL_DOWN:
        return PULL_DOWN;
    case BCM2712_PAD_PULL_UP:
        return PULL_UP;
    default:
        return PULL_MAX; /* This is an error */
    }
}

static void bcm2712_pinctrl_set_pull(void *priv, unsigned gpio, GPIO_PULL_T pull)
{
    struct bcm2712_inst *inst = priv;
    unsigned int bit = 0;
    volatile uint32_t *pad_base = bcm2712_pad_base(inst, gpio, &bit);
    uint32_t padval;
    int val;

    if (!pad_base)
        return;

    switch (pull)
    {
    case PULL_NONE:
        val = BCM2712_PAD_PULL_OFF;
        break;
    case PULL_DOWN:
        val = BCM2712_PAD_PULL_DOWN;
        break;
    case PULL_UP:
        val = BCM2712_PAD_PULL_UP;
        break;
    default:
        assert(0);
        return;
    }

    padval = *pad_base;
    padval &= ~(3 << bit);
    padval |= (val << bit);

    *pad_base = padval;
}

static void *bcm2712_gpio_create_instance(const GPIO_CHIP_T *chip,
                                          const char *dtnode)
{
    struct bcm2712_inst *inst = NULL;
    uint32_t *widths;
    unsigned num_banks, inst_gpios;
    unsigned flags = FLAGS_GPIO | chip->data;
    unsigned bank;
    unsigned i;

    widths = dt_read_cells(dtnode, "brcm,gpio-bank-widths", &num_banks);
    if (!widths)
        return NULL;

    inst_gpios = 0;
    for (bank = 0; bank < num_banks; bank++)
    {
        inst_gpios = ROUND_UP(inst_gpios, 32) + widths[bank];
    }

    flags |= shared_flags;
    if (widths[0] < 32)
    {
        flags |= FLAGS_AON;
        if (widths[0] == 15)
            flags |= FLAGS_D0;
        else
            flags |= FLAGS_C0;
    }
    else if (!(flags & (FLAGS_C0 | FLAGS_D0)))
    {
        size_t names_len;
        char *names = dt_read_prop(dtnode, "gpio-line-names", &names_len);
        if (!names[0])
            flags |= FLAGS_D0;
        dt_free(names);
    }

    shared_flags |= (flags & (FLAGS_C0 | FLAGS_D0));

    /* look for a corresponding pinctrl instance */
    for (i = 0; i < num_instances; i++)
    {
        struct bcm2712_inst *pinctrl_inst = &bcm2712_instances[i];
        pinctrl_inst->flags |= shared_flags;
        if (!((pinctrl_inst->flags ^ flags) & FLAGS_AON))
        {
            if (pinctrl_inst->flags & FLAGS_GPIO)
            {
                assert(!"duplicate gpio nodes?");
                return NULL;
            }
            inst = pinctrl_inst;
            break;
        }
    }

    if (!inst)
    {
        if (num_instances == BCM2712_MAX_INSTANCES)
            return NULL;

        inst = &bcm2712_instances[num_instances++];
    }

    inst->num_gpios = inst_gpios;
    inst->num_banks = num_banks;
    inst->bank_widths = widths;
    inst->flags |= flags;

    return (void *)inst;
}

static int bcm2712_gpio_count(void *priv)
{
    struct bcm2712_inst *inst = priv;

    return inst->num_gpios;
}

static void *bcm2712_gpio_probe_instance(void *priv, volatile uint32_t *base)
{
    struct bcm2712_inst *inst = priv;

    inst->gpio_base = base;

    return inst;
}

static void *bcm2712_pinctrl_create_instance(const GPIO_CHIP_T *chip,
                                             const char *dtnode)
{
    struct bcm2712_inst *inst = NULL;
    unsigned flags = FLAGS_PINCTRL | chip->data;
    unsigned reg_cells, reg_size;
    uint32_t *reg;
    unsigned i;

    if (dtnode)
    {
        reg = dt_read_cells(dtnode, "reg", &reg_cells);
        if (!reg || reg_cells < 2)
            return NULL;

        reg_size = (reg_cells > 1) ? reg[reg_cells - 1] : 0;
        dt_free(reg);

        switch (reg_size)
        {
        case 0x1c:
            assert((flags & FLAGS_AON) && (flags & FLAGS_D0));
            break;
        case 0x20:
            assert(((flags & FLAGS_AON) && !(flags & FLAGS_D0)) ||
                   (!(flags & FLAGS_AON) && (flags & FLAGS_D0)));
            break;
        case 0x30:
            assert(!(flags & FLAGS_AON) && !(flags & FLAGS_D0));
            break;
        default:
            assert(0);
        }
    }

    shared_flags |= (flags & (FLAGS_C0 | FLAGS_D0));

    /* look for a corresponding gpio instance */
    for (i = 0; i < num_instances; i++)
    {
        struct bcm2712_inst *gpio_inst = &bcm2712_instances[i];
        gpio_inst->flags |= shared_flags;
        if (!((gpio_inst->flags ^ flags) & FLAGS_AON))
        {
            if (gpio_inst->flags & FLAGS_PINCTRL)
            {
                assert(!"duplicate pinctrl nodes?");
                return NULL;
            }
            inst = gpio_inst;
            break;
        }
    }

    if (!inst)
    {
        if (num_instances == BCM2712_MAX_INSTANCES)
            return NULL;

        inst = &bcm2712_instances[num_instances++];
    }

    inst->flags |= flags;

    return (void *)inst;
}

static int bcm2712_pinctrl_count(void *priv)
{
    struct bcm2712_inst *inst = priv;

    if (inst->flags & FLAGS_GPIO)
        return 0;  /* Don't occupy any GPIO space */

    if (!inst->num_gpios)
    {
        switch (inst->flags & (FLAGS_AON | FLAGS_C0 | FLAGS_D0))
        {
        case 0:
        case FLAGS_C0:
            inst->num_gpios = 54;
            break;
        case FLAGS_D0:
            inst->num_gpios = 36;
            break;
        case FLAGS_AON:
        case FLAGS_AON | FLAGS_D0:
        case FLAGS_AON | FLAGS_C0:
            inst->num_gpios = 38;
            break;
        default:
            break;
        }
    }
    return inst->num_gpios;
}

static void *bcm2712_pinctrl_probe_instance(void *priv, volatile uint32_t *base)
{
    struct bcm2712_inst *inst = priv;
    unsigned pad_offset;

    inst->pinmux_base = base;
    switch (inst->flags & (FLAGS_D0 | FLAGS_C0 | FLAGS_AON))
    {
    case FLAGS_C0:
    default:
        pad_offset = 112;
        break;
    case FLAGS_D0:
        pad_offset = 65;
        break;
    case FLAGS_AON:
    case FLAGS_C0 | FLAGS_AON:
        pad_offset = 100;
        break;
    case FLAGS_D0 | FLAGS_AON:
        pad_offset = 84;
        break;
    }

    inst->pad_offset = pad_offset;

    return inst;
}

static const char *bcm2712_pinctrl_get_fsel_name(void *priv, unsigned gpio, GPIO_FSEL_T fsel)
{
    struct bcm2712_inst *inst = priv;
    const char *name = NULL;

    switch (fsel)
    {
    case GPIO_FSEL_GPIO:
    case GPIO_FSEL_FUNC0:
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
    case GPIO_FSEL_FUNC1:
    case GPIO_FSEL_FUNC2:
    case GPIO_FSEL_FUNC3:
    case GPIO_FSEL_FUNC4:
    case GPIO_FSEL_FUNC5:
    case GPIO_FSEL_FUNC6:
    case GPIO_FSEL_FUNC7:
    case GPIO_FSEL_FUNC8:
        if (gpio < inst->num_gpios)
        {
            switch (inst->flags & (FLAGS_AON | FLAGS_C0 | FLAGS_D0))
            {
            case FLAGS_C0 | FLAGS_AON:
            case FLAGS_AON:
                name = bcm2712_c0_aon_gpio_alt_names[gpio][fsel - 1];
                break;
            case FLAGS_C0:
            case 0:
                name = bcm2712_c0_gpio_alt_names[gpio][fsel - 1];
                break;
            case FLAGS_D0 | FLAGS_AON:
                name = bcm2712_d0_aon_gpio_alt_names[gpio][fsel - 1];
                break;
            case FLAGS_D0:
                name = bcm2712_d0_gpio_alt_names[gpio][fsel - 1];
                break;
            }
            if (!name)
                name = "-";
        }
        break;
    default:
        break;
    }
    return name;
}

static const char *bcm2712_gpio_get_name(void *priv, unsigned gpio)
{
    struct bcm2712_inst *inst = priv;
    const char *fsel_name;
    static char name_buf[16];
    unsigned gpio_offset;
    unsigned bank;

    fsel_name = bcm2712_pinctrl_get_fsel_name(priv, gpio, GPIO_FSEL_FUNC1);
    if (!fsel_name || !fsel_name[0])
        return NULL;

    bank = gpio / 32;
    gpio_offset = gpio % 32;

    if ((inst->flags & FLAGS_GPIO) &&
        ((bank >= inst->num_banks) ||
         (gpio_offset >= inst->bank_widths[bank])))
        return NULL;

    if (inst->flags & FLAGS_AON)
    {
        if (bank == 1)
            sprintf(name_buf, "AON_SGPIO%d", gpio_offset);
        else
            sprintf(name_buf, "AON_GPIO%d", gpio_offset);
    }
    else
    {
        sprintf(name_buf, "GPIO%d", gpio);
    }
    return name_buf;
}

static const GPIO_CHIP_INTERFACE_T bcm2712_gpio_interface =
{
    .gpio_create_instance = bcm2712_gpio_create_instance,
    .gpio_count = bcm2712_gpio_count,
    .gpio_probe_instance = bcm2712_gpio_probe_instance,
    .gpio_get_fsel = bcm2712_pinctrl_get_fsel,
    .gpio_set_fsel = bcm2712_pinctrl_set_fsel,
    .gpio_set_drive = bcm2712_gpio_set_drive,
    .gpio_set_dir = bcm2712_gpio_set_dir,
    .gpio_get_dir = bcm2712_gpio_get_dir,
    .gpio_get_level = bcm2712_gpio_get_level,
    .gpio_get_drive = bcm2712_gpio_get_drive,
    .gpio_get_pull = bcm2712_pinctrl_get_pull,
    .gpio_set_pull = bcm2712_pinctrl_set_pull,
    .gpio_get_name = bcm2712_gpio_get_name,
    .gpio_get_fsel_name = bcm2712_pinctrl_get_fsel_name,
};

DECLARE_GPIO_CHIP(brcmstb, "brcm,brcmstb-gpio",
                  &bcm2712_gpio_interface, 0x40, 0);

static const GPIO_CHIP_INTERFACE_T bcm2712_pinctrl_interface =
{
    .gpio_create_instance = bcm2712_pinctrl_create_instance,
    .gpio_count = bcm2712_pinctrl_count,
    .gpio_probe_instance = bcm2712_pinctrl_probe_instance,
    .gpio_get_fsel = bcm2712_pinctrl_get_fsel,
    .gpio_set_fsel = bcm2712_pinctrl_set_fsel,
    .gpio_set_drive = bcm2712_gpio_set_drive,
    .gpio_set_dir = bcm2712_gpio_set_dir,
    .gpio_get_dir = bcm2712_gpio_get_dir,
    .gpio_get_level = bcm2712_gpio_get_level,
    .gpio_get_drive = bcm2712_gpio_get_drive,
    .gpio_get_pull = bcm2712_pinctrl_get_pull,
    .gpio_set_pull = bcm2712_pinctrl_set_pull,
    .gpio_get_name = bcm2712_gpio_get_name,
    .gpio_get_fsel_name = bcm2712_pinctrl_get_fsel_name,
};

DECLARE_GPIO_CHIP(bcm2712, "brcm,bcm2712-pinctrl",
                  &bcm2712_pinctrl_interface, 0x30, 0);
DECLARE_GPIO_CHIP(bcm2712_aon, "brcm,bcm2712-aon-pinctrl",
                  &bcm2712_pinctrl_interface, 0x20, FLAGS_AON);

DECLARE_GPIO_CHIP(bcm2712c0, "brcm,bcm2712c0-pinctrl",
                  &bcm2712_pinctrl_interface, 0x30, FLAGS_C0);
DECLARE_GPIO_CHIP(bcm2712c0_aon, "brcm,bcm2712c0-aon-pinctrl",
                  &bcm2712_pinctrl_interface, 0x20, FLAGS_C0 | FLAGS_AON);

DECLARE_GPIO_CHIP(bcm2712d0, "brcm,bcm2712d0-pinctrl",
                  &bcm2712_pinctrl_interface, 0x20, FLAGS_D0);
DECLARE_GPIO_CHIP(bcm2712d0_aon, "brcm,bcm2712d0-aon-pinctrl",
                  &bcm2712_pinctrl_interface, 0x1c, FLAGS_D0 | FLAGS_AON);
