//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022 akuker
//
//---------------------------------------------------------------------------

#include "hal/gpiobus_raspberry.h"
#include "mocks.h"
#include <cstdlib>
#include "test/test_shared.h"

class SetableGpiobusRaspberry : public GPIOBUS_Raspberry
{
  public:
    void TestSetGpios(uint32_t value)
    {
        *level = ~value;
    }
    void TestSetGpioPin(int pin, bool value)
    {
        // Level is inverted logic
        if (!value) {
            *level = *level | (1 << pin);
        } else {
            *level = *level & ~(1 << pin);
        }
    }
    SetableGpiobusRaspberry()
    {
        level = new uint32_t(); // NOSONAR: This is a pointer to a register on the real hardware
    }
};

extern "C" {
uint32_t get_dt_ranges(const char *filename, uint32_t offset);
uint32_t bcm_host_get_peripheral_address();
}

TEST(GpiobusRaspberry, GetDtRanges)
{
    const string soc_ranges_file = "/proc/device-tree/soc/ranges";

    vector<uint8_t> data;
    // If bytes 4-7 are non-zero, get_peripheral address should return those bytes
    data = vector<uint8_t>(
        {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF});
    CreateTempFileWithData(soc_ranges_file, data);
    EXPECT_EQ(0x44556677, GPIOBUS_Raspberry::bcm_host_get_peripheral_address());
    DeleteTempFile("/proc/device-tree/soc/ranges");

    // If bytes 4-7 are zero, get_peripheral address should return bytes 8-11
    data = vector<uint8_t>(
        {0x00, 0x11, 0x22, 0x33, 0x00, 0x00, 0x00, 0x00, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF});
    CreateTempFileWithData(soc_ranges_file, data);
    EXPECT_EQ(0x8899AABB, GPIOBUS_Raspberry::bcm_host_get_peripheral_address());
    DeleteTempFile("/proc/device-tree/soc/ranges");

    // If bytes 4-7 are zero, and 8-11 are 0xFF, get_peripheral address should return a default address of 0x20000000
    data = vector<uint8_t>(
        {0x00, 0x11, 0x22, 0x33, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xCC, 0xDD, 0xEE, 0xFF});
    CreateTempFileWithData(soc_ranges_file, data);
    EXPECT_EQ(0x20000000, GPIOBUS_Raspberry::bcm_host_get_peripheral_address());
    DeleteTempFile("/proc/device-tree/soc/ranges");

    remove_all(test_data_temp_path);
}

TEST(GpiobusRaspberry, GetDat)
{
    SetableGpiobusRaspberry bus;

    bus.TestSetGpios(0x00);
    EXPECT_EQ(0, bus.GetDAT());

    bus.TestSetGpioPin(PIN_DT0, true);
    EXPECT_EQ(0x01, bus.GetDAT());

    bus.TestSetGpioPin(PIN_DT1, true);
    EXPECT_EQ(0x03, bus.GetDAT());

    bus.TestSetGpioPin(PIN_DT2, true);
    EXPECT_EQ(0x07, bus.GetDAT());

    bus.TestSetGpioPin(PIN_DT3, true);
    EXPECT_EQ(0x0F, bus.GetDAT());

    bus.TestSetGpioPin(PIN_DT4, true);
    EXPECT_EQ(0x1F, bus.GetDAT());

    bus.TestSetGpioPin(PIN_DT5, true);
    EXPECT_EQ(0x3F, bus.GetDAT());

    bus.TestSetGpioPin(PIN_DT6, true);
    EXPECT_EQ(0x7F, bus.GetDAT());

    bus.TestSetGpioPin(PIN_DT7, true);
    EXPECT_EQ(0xFF, bus.GetDAT());

    bus.TestSetGpios(0xFFFFFFFF);
    EXPECT_EQ(0xFF, bus.GetDAT());
}

TEST(GpiobusRaspberry, GetBSY)
{
    SetableGpiobusRaspberry bus;

    bus.TestSetGpios(0x00);
    bus.TestSetGpioPin(PIN_BSY, true);
    bus.Acquire();
    EXPECT_EQ(true, bus.GetBSY());
    bus.TestSetGpioPin(PIN_BSY, false);
    bus.Acquire();
    EXPECT_EQ(false, bus.GetBSY());
}

TEST(GpiobusRaspberry, GetSEL)
{
    SetableGpiobusRaspberry bus;

    bus.TestSetGpios(0x00);
    bus.TestSetGpioPin(PIN_SEL, true);
    bus.Acquire();
    EXPECT_EQ(true, bus.GetSEL());
    bus.TestSetGpioPin(PIN_SEL, false);
    bus.Acquire();
    EXPECT_EQ(false, bus.GetSEL());
}

TEST(GpiobusRaspberry, GetATN)
{
    SetableGpiobusRaspberry bus;

    bus.TestSetGpios(0x00);
    bus.TestSetGpioPin(PIN_ATN, true);
    bus.Acquire();
    EXPECT_EQ(true, bus.GetATN());
    bus.TestSetGpioPin(PIN_ATN, false);
    bus.Acquire();
    EXPECT_EQ(false, bus.GetATN());
}

TEST(GpiobusRaspberry, GetACK)
{
    SetableGpiobusRaspberry bus;

    bus.TestSetGpios(0x00);
    bus.TestSetGpioPin(PIN_ACK, true);
    bus.Acquire();
    EXPECT_EQ(true, bus.GetACK());
    bus.TestSetGpioPin(PIN_ACK, false);
    bus.Acquire();
    EXPECT_EQ(false, bus.GetACK());
}

TEST(GpiobusRaspberry, GetRST)
{
    SetableGpiobusRaspberry bus;

    bus.TestSetGpios(0x00);
    bus.TestSetGpioPin(PIN_RST, true);
    bus.Acquire();
    EXPECT_EQ(true, bus.GetRST());
    bus.TestSetGpioPin(PIN_RST, false);
    bus.Acquire();
    EXPECT_EQ(false, bus.GetRST());
}

TEST(GpiobusRaspberry, GetMSG)
{
    SetableGpiobusRaspberry bus;

    bus.TestSetGpios(0x00);
    bus.TestSetGpioPin(PIN_MSG, true);
    bus.Acquire();
    EXPECT_EQ(true, bus.GetMSG());
    bus.TestSetGpioPin(PIN_MSG, false);
    bus.Acquire();
    EXPECT_EQ(false, bus.GetMSG());
}

TEST(GpiobusRaspberry, GetCD)
{
    SetableGpiobusRaspberry bus;

    bus.TestSetGpios(0x00);
    bus.TestSetGpioPin(PIN_CD, true);
    bus.Acquire();
    EXPECT_EQ(true, bus.GetCD());
    bus.TestSetGpioPin(PIN_CD, false);
    bus.Acquire();
    EXPECT_EQ(false, bus.GetCD());
}

TEST(GpiobusRaspberry, GetIO)
{
    SetableGpiobusRaspberry bus;

    bus.TestSetGpios(0x00);
    bus.TestSetGpioPin(PIN_IO, true);
    bus.Acquire();
    EXPECT_EQ(true, bus.GetIO());
    bus.TestSetGpioPin(PIN_IO, false);
    bus.Acquire();
    EXPECT_EQ(false, bus.GetIO());
}

TEST(GpiobusRaspberry, GetREQ)
{
    SetableGpiobusRaspberry bus;

    bus.TestSetGpios(0x00);
    bus.TestSetGpioPin(PIN_REQ, true);
    bus.Acquire();
    EXPECT_EQ(true, bus.GetREQ());
    bus.TestSetGpioPin(PIN_REQ, false);
    bus.Acquire();
    EXPECT_EQ(false, bus.GetREQ());
}
