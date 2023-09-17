//---------------------------------------------------------------------------
//
//	SCSI Target Emulator PiSCSI
//	for Raspberry Pi (And Banana Pi)
//
//  Copyright (c) 2012-2015 Ben Croston
//  Copyright (C) 2022 akuker
//
//  Large portions of this functionality were derived from c_gpio.c, which
//  is part of the RPI.GPIO library available here:
//     https://github.com/BPI-SINOVOIP/RPi.GPIO/blob/master/source/c_gpio.c
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy of
//  this software and associated documentation files (the "Software"), to deal in
//  the Software without restriction, including without limitation the rights to
//  use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
//  of the Software, and to permit persons to whom the Software is furnished to do
//  so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in all
//  copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//  SOFTWARE.
//
//---------------------------------------------------------------------------

#include <iomanip>
#include <memory>
#include <sstream>
#include <string.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "hal/gpiobus.h"
#include "hal/gpiobus_bananam2p.h"
#include "hal/pi_defs/bpi-gpio.h"
#include "hal/sunxi_utils.h"
#include "hal/systimer.h"
#include "shared/log.h"

#define ARRAY_SIZE(x) (sizeof(x) / (sizeof(x[0])))

bool GPIOBUS_BananaM2p::Init(mode_e mode)
{
    GPIO_FUNCTION_TRACE
    GPIOBUS::Init(mode);
    SysTimer::Init();

    sbc_version = SBC_Version::GetSbcVersion();

    for (auto const gpio_num : SignalTable) {
        if (gpio_num == -1) {
            break;
        }

        int gpio_bank = SunXI::GPIO_BANK(gpio_num);

        if (std::find(gpio_banks.begin(), gpio_banks.end(), gpio_bank) != gpio_banks.end()) {
            LOGTRACE("Duplicate bank: %d", gpio_bank)

        } else {
            LOGDEBUG("New bank: %d", gpio_bank)
            gpio_banks.push_back(gpio_bank);
        }
    }

    if (int result = sunxi_setup(); result != SETUP_OK) {
        return false;
    }

    InitializeGpio();

    MakeTable();

    // SetupSelEvent needs to be called AFTER Initialize GPIO. This function
    // reconfigures the SEL signal.
    if (!SetupSelEvent()) {
        LOGERROR("Failed to setup SELECT poll event")
        return false;
    }
    LOGTRACE("SetupSelEvent OK!")

    // Set drive strength to maximum
    DrvConfig(3);

    return true;
}

void GPIOBUS_BananaM2p::InitializeGpio()
{
    GPIO_FUNCTION_TRACE

    // Set pull up/pull down
#if SIGNAL_CONTROL_MODE == 0
    int pullmode = GPIO_PULLNONE;
#elif SIGNAL_CONTROL_MODE == 1
    int pullmode = GPIO_PULLUP;
#else
    int pullmode = GPIO_PULLDOWN;
#endif

    // Initialize all signals
    for (int i = 0; SignalTable[i] >= 0; i++) {
        int j = SignalTable[i];
        PinConfig(j, GPIO_INPUT);
        PullConfig(j, pullmode);
        PinSetSignal(j, OFF);
    }

    // Set control signals
    PinConfig(BPI_PIN_ACT, GPIO_OUTPUT);
    PinConfig(BPI_PIN_TAD, GPIO_OUTPUT);
    PinConfig(BPI_PIN_IND, GPIO_OUTPUT);
    PinConfig(BPI_PIN_DTD, GPIO_OUTPUT);
    PinSetSignal(BPI_PIN_ACT, OFF);
    PinSetSignal(BPI_PIN_TAD, OFF);
    PinSetSignal(BPI_PIN_IND, OFF);
    PinSetSignal(BPI_PIN_DTD, OFF);

    // Set the ENABLE signal
    // This is used to show that the application is running
    PinConfig(BPI_PIN_ENB, GPIO_OUTPUT);
    PinSetSignal(BPI_PIN_ENB, ON);
}

void GPIOBUS_BananaM2p::Cleanup()
{
    GPIO_FUNCTION_TRACE
#if defined(__x86_64__) || defined(__X86__) || defined(__aarch64__)
    dummy_var--; // Need to do something to prevent Sonar from claiming this should be a const function
    return;
#else

#ifdef USE_SEL_EVENT_ENABLE
    // Release SEL signal interrupt
    close(selevreq.fd);
#endif // USE_SEL_EVENT_ENABLE

    // Set control signals
    PinConfig(BPI_PIN_ACT, GPIO_INPUT);
    PinConfig(BPI_PIN_TAD, GPIO_INPUT);
    PinConfig(BPI_PIN_IND, GPIO_INPUT);
    PinConfig(BPI_PIN_DTD, GPIO_INPUT);
    PinSetSignal(BPI_PIN_ENB, OFF);
    PinSetSignal(BPI_PIN_ACT, OFF);
    PinSetSignal(BPI_PIN_TAD, OFF);
    PinSetSignal(BPI_PIN_IND, OFF);
    PinSetSignal(BPI_PIN_DTD, OFF);

    // Initialize all signals
    for (int i = 0; SignalTable[i] >= 0; i++) {
        int pin = SignalTable[i];
        PinConfig(pin, GPIO_INPUT);
        PullConfig(pin, GPIO_PULLNONE);
        PinSetSignal(pin, OFF);
    }

    // Set drive strength back to Default (Level 1)
    DrvConfig(1);

    munmap((void *)gpio_map, SunXI::BLOCK_SIZE);
    munmap((void *)r_gpio_map, SunXI::BLOCK_SIZE);

#endif
}

void GPIOBUS_BananaM2p::Reset()
{
#if defined(__x86_64__) || defined(__X86__) || defined(__aarch64__)
    dummy_var++;
    return;
#else
    int i;
    int j;

    // Turn off active signal
    SetControl(BPI_PIN_ACT, ACT_OFF);

    // Set all signals to off
    for (i = 0;; i++) {
        j = SignalTable[i];
        if (j < 0) {
            break;
        }

        SetSignal(j, OFF);
    }

    if (actmode == mode_e::TARGET) {
        // Target mode

        // Set target signal to input
        SetControl(BPI_PIN_TAD, TAD_IN);
        SetMode(BPI_PIN_BSY, IN);
        SetMode(BPI_PIN_MSG, IN);
        SetMode(BPI_PIN_CD, IN);
        SetMode(BPI_PIN_REQ, IN);
        SetMode(BPI_PIN_IO, IN);

        // Set the initiator signal to input
        SetControl(BPI_PIN_IND, IND_IN);
        SetMode(BPI_PIN_SEL, IN);
        SetMode(BPI_PIN_ATN, IN);
        SetMode(BPI_PIN_ACK, IN);
        SetMode(BPI_PIN_RST, IN);

        // Set data bus signals to input
        SetControl(BPI_PIN_DTD, DTD_IN);
        SetMode(BPI_PIN_DT0, IN);
        SetMode(BPI_PIN_DT1, IN);
        SetMode(BPI_PIN_DT2, IN);
        SetMode(BPI_PIN_DT3, IN);
        SetMode(BPI_PIN_DT4, IN);
        SetMode(BPI_PIN_DT5, IN);
        SetMode(BPI_PIN_DT6, IN);
        SetMode(BPI_PIN_DT7, IN);
        SetMode(BPI_PIN_DP, IN);
    } else {
        // Initiator mode

        // Set target signal to input
        SetControl(BPI_PIN_TAD, TAD_IN);
        SetMode(BPI_PIN_BSY, IN);
        SetMode(BPI_PIN_MSG, IN);
        SetMode(BPI_PIN_CD, IN);
        SetMode(BPI_PIN_REQ, IN);
        SetMode(BPI_PIN_IO, IN);

        // Set the initiator signal to output
        SetControl(BPI_PIN_IND, IND_OUT);
        SetMode(BPI_PIN_SEL, OUT);
        SetMode(BPI_PIN_ATN, OUT);
        SetMode(BPI_PIN_ACK, OUT);
        SetMode(BPI_PIN_RST, OUT);

        // Set the data bus signals to output
        SetControl(BPI_PIN_DTD, DTD_OUT);
        SetMode(BPI_PIN_DT0, OUT);
        SetMode(BPI_PIN_DT1, OUT);
        SetMode(BPI_PIN_DT2, OUT);
        SetMode(BPI_PIN_DT3, OUT);
        SetMode(BPI_PIN_DT4, OUT);
        SetMode(BPI_PIN_DT5, OUT);
        SetMode(BPI_PIN_DT6, OUT);
        SetMode(BPI_PIN_DT7, OUT);
        SetMode(BPI_PIN_DP, OUT);
    }

    // Initialize all signals
    // TODO!! For now, just re-run Acquire
    Acquire();
#endif // ifdef __x86_64__ || __X86__
}

bool GPIOBUS_BananaM2p::SetupSelEvent()
{
#if defined(__x86_64__) || defined(__X86__) || defined(__aarch64__)
    dummy_var += 2; // Need to do something to prevent Sonar from claiming this should be a const function
    return false;
#else
    GPIO_FUNCTION_TRACE
    int gpio_pin = BPI_PIN_SEL;

    // GPIO chip open
    LOGTRACE("%s GPIO chip open  [%d]", __PRETTY_FUNCTION__, gpio_pin)
    std::string gpio_dev = "/dev/gpiochip0";
    if (SunXI::GPIO_BANK(gpio_pin) >= 11) {
        gpio_dev = "/dev/gpiochip1";
        LOGWARN("gpiochip1 support isn't implemented yet....")
        LOGWARN("THIS PROBABLY WONT WORK!")
    }

    int gpio_fd = open(gpio_dev.c_str(), 0);
    if (gpio_fd == -1) {
        LOGERROR("Unable to open /dev/gpiochip0. Is PiSCSI or RaSCSI already running?")
        return false;
    }

    // Event request setting
    LOGTRACE("%s Event request setting (pin sel: %d)", __PRETTY_FUNCTION__, gpio_pin)
    strncpy(selevreq.consumer_label, "PiSCSI", ARRAY_SIZE(selevreq.consumer_label));
    selevreq.lineoffset  = gpio_pin;
    selevreq.handleflags = GPIOHANDLE_REQUEST_INPUT;
#if SIGNAL_CONTROL_MODE < 2
    selevreq.eventflags  = GPIOEVENT_REQUEST_FALLING_EDGE;
    LOGTRACE("%s eventflags = GPIOEVENT_REQUEST_FALLING_EDGE", __PRETTY_FUNCTION__)
#else
    selevreq.eventflags = GPIOEVENT_REQUEST_RISING_EDGE;
    LOGTRACE("%s eventflags = GPIOEVENT_REQUEST_RISING_EDGE", __PRETTY_FUNCTION__)
#endif // SIGNAL_CONTROL_MODE

    // Get event request
    if (ioctl(gpio_fd, GPIO_GET_LINEEVENT_IOCTL, &selevreq) == -1) {
        LOGERROR("selevreq.fd = %d %08X", selevreq.fd, (unsigned int)selevreq.fd)
        LOGERROR("Unable to register event request. Is PiSCSI or RaSCSI already running?")
        LOGERROR("[%08X] %s", errno, strerror(errno))
        close(gpio_fd);
        return false;
    }

    // Close GPIO chip file handle
    LOGTRACE("%s Close GPIO chip file handle", __PRETTY_FUNCTION__)
    close(gpio_fd);

    // epoll initialization
    LOGTRACE("%s epoll initialization", __PRETTY_FUNCTION__)
    epfd = epoll_create(1);
    if (epfd == -1) {
        LOGERROR("Unable to create the epoll event")
        return false;
    }
    epoll_event ev = {};
    memset(&ev, 0, sizeof(ev));
    ev.events  = EPOLLIN | EPOLLPRI;
    ev.data.fd = selevreq.fd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, selevreq.fd, &ev) < 0) {
        return false;
    }

    return true;
#endif
}

void GPIOBUS_BananaM2p::SetENB(bool ast)
{
    PinSetSignal(BPI_PIN_ENB, ast ? ENB_ON : ENB_OFF);
}

bool GPIOBUS_BananaM2p::GetBSY() const
{
    return GetSignal(BPI_PIN_BSY);
}

void GPIOBUS_BananaM2p::SetBSY(bool ast)
{
    if (actmode == mode_e::TARGET) {
        if (ast) {
            // Turn on ACTIVE signal
            SetControl(BPI_PIN_ACT, ACT_ON);

            // Set Target signal to output
            SetControl(BPI_PIN_TAD, TAD_OUT);

            SetMode(BPI_PIN_BSY, OUT);
            SetMode(BPI_PIN_MSG, OUT);
            SetMode(BPI_PIN_CD, OUT);
            SetMode(BPI_PIN_REQ, OUT);
            SetMode(BPI_PIN_IO, OUT);

            // Set BSY signal
            SetSignal(BPI_PIN_BSY, ast);

        } else {
            // Turn off the ACTIVE signal
            SetControl(BPI_PIN_ACT, ACT_OFF);

            // Set the target signal to input
            SetControl(BPI_PIN_TAD, TAD_IN);

            SetMode(BPI_PIN_BSY, IN);
            SetMode(BPI_PIN_MSG, IN);
            SetMode(BPI_PIN_CD, IN);
            SetMode(BPI_PIN_REQ, IN);
            SetMode(BPI_PIN_IO, IN);
        }
    } else {
        // Set BSY signal
        SetSignal(BPI_PIN_BSY, ast);
    }
}

bool GPIOBUS_BananaM2p::GetSEL() const
{
    return GetSignal(BPI_PIN_SEL);
}

void GPIOBUS_BananaM2p::SetSEL(bool ast)
{
    if (actmode == mode_e::INITIATOR && ast) {
        // Turn on ACTIVE signal
        SetControl(BPI_PIN_ACT, ACT_ON);
    }

    // Set SEL signal
    SetSignal(BPI_PIN_SEL, ast);
}

bool GPIOBUS_BananaM2p::GetATN() const
{
    return GetSignal(BPI_PIN_ATN);
}

void GPIOBUS_BananaM2p::SetATN(bool ast)
{
    SetSignal(BPI_PIN_ATN, ast);
}

bool GPIOBUS_BananaM2p::GetACK() const
{
    return GetSignal(BPI_PIN_ACK);
}

void GPIOBUS_BananaM2p::SetACK(bool ast)
{
    SetSignal(BPI_PIN_ACK, ast);
}

bool GPIOBUS_BananaM2p::GetACT() const
{
    return GetSignal(BPI_PIN_ACT);
}

void GPIOBUS_BananaM2p::SetACT(bool ast)
{
    SetSignal(BPI_PIN_ACT, ast);
}

bool GPIOBUS_BananaM2p::GetRST() const
{
    return GetSignal(BPI_PIN_RST);
}

void GPIOBUS_BananaM2p::SetRST(bool ast)
{
    SetSignal(BPI_PIN_RST, ast);
}

bool GPIOBUS_BananaM2p::GetMSG() const
{
    return GetSignal(BPI_PIN_MSG);
}

void GPIOBUS_BananaM2p::SetMSG(bool ast)
{
    SetSignal(BPI_PIN_MSG, ast);
}

bool GPIOBUS_BananaM2p::GetCD() const
{
    return GetSignal(BPI_PIN_CD);
}

void GPIOBUS_BananaM2p::SetCD(bool ast)
{
    SetSignal(BPI_PIN_CD, ast);
}

bool GPIOBUS_BananaM2p::GetIO()
{
    bool ast = GetSignal(BPI_PIN_IO);

    if (actmode == mode_e::INITIATOR) {
        // Change the data input/output direction by IO signal
        if (ast) {
            SetControl(BPI_PIN_DTD, DTD_IN);
            SetMode(BPI_PIN_DT0, IN);
            SetMode(BPI_PIN_DT1, IN);
            SetMode(BPI_PIN_DT2, IN);
            SetMode(BPI_PIN_DT3, IN);
            SetMode(BPI_PIN_DT4, IN);
            SetMode(BPI_PIN_DT5, IN);
            SetMode(BPI_PIN_DT6, IN);
            SetMode(BPI_PIN_DT7, IN);
            SetMode(BPI_PIN_DP, IN);
        } else {
            SetControl(BPI_PIN_DTD, DTD_OUT);
            SetMode(BPI_PIN_DT0, OUT);
            SetMode(BPI_PIN_DT1, OUT);
            SetMode(BPI_PIN_DT2, OUT);
            SetMode(BPI_PIN_DT3, OUT);
            SetMode(BPI_PIN_DT4, OUT);
            SetMode(BPI_PIN_DT5, OUT);
            SetMode(BPI_PIN_DT6, OUT);
            SetMode(BPI_PIN_DT7, OUT);
            SetMode(BPI_PIN_DP, OUT);
        }
    }

    return ast;
}

void GPIOBUS_BananaM2p::SetIO(bool ast)
{
    if (actmode == mode_e::TARGET) {
        // Change the data input/output direction by IO signal
        if (ast) {
            SetControl(BPI_PIN_DTD, DTD_OUT);
            SetMode(BPI_PIN_DT0, OUT);
            SetMode(BPI_PIN_DT1, OUT);
            SetMode(BPI_PIN_DT2, OUT);
            SetMode(BPI_PIN_DT3, OUT);
            SetMode(BPI_PIN_DT4, OUT);
            SetMode(BPI_PIN_DT5, OUT);
            SetMode(BPI_PIN_DT6, OUT);
            SetMode(BPI_PIN_DT7, OUT);
            SetMode(BPI_PIN_DP, OUT);

            SetDAT(0);
            SetSignal(BPI_PIN_IO, ast);

        } else {
            SetControl(BPI_PIN_DTD, DTD_IN);
            SetMode(BPI_PIN_DT0, IN);
            SetMode(BPI_PIN_DT1, IN);
            SetMode(BPI_PIN_DT2, IN);
            SetMode(BPI_PIN_DT3, IN);
            SetMode(BPI_PIN_DT4, IN);
            SetMode(BPI_PIN_DT5, IN);
            SetMode(BPI_PIN_DT6, IN);
            SetMode(BPI_PIN_DT7, IN);
            SetMode(BPI_PIN_DP, IN);
        }
    } else {
        SetSignal(BPI_PIN_IO, ast);
    }
}

bool GPIOBUS_BananaM2p::GetREQ() const
{
    return GetSignal(BPI_PIN_REQ);
}

void GPIOBUS_BananaM2p::SetREQ(bool ast)
{
    SetSignal(BPI_PIN_REQ, ast);
}

uint8_t GPIOBUS_BananaM2p::GetDAT()
{
    GPIO_FUNCTION_TRACE

    Acquire();
    uint32_t data =
        ((GetSignal(BPI_PIN_DT0) ? 0x01 : 0x00) << 0) | ((GetSignal(BPI_PIN_DT1) ? 0x01 : 0x00) << 1) |
        ((GetSignal(BPI_PIN_DT2) ? 0x01 : 0x00) << 2) | ((GetSignal(BPI_PIN_DT3) ? 0x01 : 0x00) << 3) |
        ((GetSignal(BPI_PIN_DT4) ? 0x01 : 0x00) << 4) | ((GetSignal(BPI_PIN_DT5) ? 0x01 : 0x00) << 5) |
        ((GetSignal(BPI_PIN_DT6) ? 0x01 : 0x00) << 6) |
        ((GetSignal(BPI_PIN_DT7) ? 0x01 : 0x00) << 7); // NOSONAR: GCC 10 doesn't support shift operations on std::byte

    return (uint8_t)(data & 0xFF);
}

void GPIOBUS_BananaM2p::SetDAT(uint8_t dat)
{
    GPIO_FUNCTION_TRACE

    array<uint32_t, 12> gpio_reg_values = {0};

    for (size_t gpio_num = 0; gpio_num < pintbl.size(); gpio_num++) {
        bool value;
        if (gpio_num < 8) {
            // data bits
            value = !(dat & (1 << gpio_num)); // NOSONAR: GCC 10 doesn't support shift operations on std::byte
        } else {
            // parity bit
            value = (__builtin_parity(dat) == 1);
        }

        if (value) {
            uint32_t this_gpio = pintbl[gpio_num];
            int bank           = SunXI::GPIO_BANK(this_gpio);
            int offset         = SunXI::GPIO_NUM(this_gpio);
            gpio_reg_values[bank] |= (1 << offset);
        }
    }

    sunxi_set_all_gpios(gpio_data_masks, gpio_reg_values);
}

//---------------------------------------------------------------------------
//
//	Signal table
//
//---------------------------------------------------------------------------
const array<int, 19> GPIOBUS_BananaM2p::SignalTable = {BPI_PIN_DT0, BPI_PIN_DT1, BPI_PIN_DT2, BPI_PIN_DT3, BPI_PIN_DT4,
                                                       BPI_PIN_DT5, BPI_PIN_DT6, BPI_PIN_DT7, BPI_PIN_DP,  BPI_PIN_SEL,
                                                       BPI_PIN_ATN, BPI_PIN_RST, BPI_PIN_ACK, BPI_PIN_BSY, BPI_PIN_MSG,
                                                       BPI_PIN_CD,  BPI_PIN_IO,  BPI_PIN_REQ, -1};

//---------------------------------------------------------------------------
//
//	Create work table
//
//---------------------------------------------------------------------------
void GPIOBUS_BananaM2p::MakeTable(void)
{
    for (auto this_gpio : pintbl) {
        int bank   = SunXI::GPIO_BANK(this_gpio);
        int offset = (SunXI::GPIO_NUM(this_gpio));

        gpio_data_masks[bank] |= (1 << offset);
    }
}

bool GPIOBUS_BananaM2p::GetDP() const
{
    return GetSignal(BPI_PIN_DP);
}

void GPIOBUS_BananaM2p::SetControl(int pin, bool ast)
{
    GPIO_FUNCTION_TRACE
    PinSetSignal(pin, ast);
}

// Set direction
int GPIOBUS_BananaM2p::GetMode(int pin)
{
    GPIO_FUNCTION_TRACE

    uint32_t regval = 0;
    int bank        = SunXI::GPIO_BANK(pin);
    int index       = SunXI::GPIO_CFG_INDEX(pin);
    int offset      = SunXI::GPIO_CFG_OFFSET(pin);

    volatile SunXI::sunxi_gpio_t *pio = &(pio_map->gpio_bank[bank]);
    /* DK, for PL and PM */
    if (bank >= 11) {
        bank -= 11;
        pio = &(r_pio_map->gpio_bank[bank]);
    }

    regval = pio->CFG[0 + index];

    // Extract the CFG field
    regval &= (0x7 << offset); // 0xf?
    // Shift it down to the LSB
    regval >>= offset;
    return (int)regval;
}

// Set direction
void GPIOBUS_BananaM2p::SetMode(int pin, int mode)
{
    GPIO_FUNCTION_TRACE
    int direction = mode;

    uint32_t regval = 0;
    int bank        = SunXI::GPIO_BANK(pin);       // gpio >> 5
    int index       = SunXI::GPIO_CFG_INDEX(pin);  // (gpio & 0x1F) >> 3
    int offset      = SunXI::GPIO_CFG_OFFSET(pin); // ((gpio & 0x1F) & 0x7) << 2
    LOGTRACE("%s gpio(%d) bank(%d) index(%d) offset(%d) dir(%s)", __PRETTY_FUNCTION__, pin, bank, index, offset,
             (GPIO_INPUT == direction)    ? "IN"
             : (GPIO_IRQ_IN == direction) ? "IRQ"
                                          : "OUT")

    volatile SunXI::sunxi_gpio_t *pio = &(pio_map->gpio_bank[bank]);
    /* DK, for PL and PM */
    if (bank >= 11) {
        bank -= 11;
        pio = &(r_pio_map->gpio_bank[bank]);
    }

    regval = pio->CFG[0 + index];

    // Clear the cfg field
    regval &= ~(0x7 << offset); // 0xf?
    if (GPIO_INPUT == direction) {
        regval |= (((uint32_t)SunXI::gpio_configure_values_e::gpio_input) << offset);
    } else if (GPIO_OUTPUT == direction) {
        regval |= (((uint32_t)SunXI::gpio_configure_values_e::gpio_output) << offset);
    } else if (GPIO_IRQ_IN == direction) {
        regval |= (((uint32_t)SunXI::gpio_configure_values_e::gpio_interupt) << offset);
    } else {
        LOGERROR("line:%d gpio number error %d", __LINE__, pin)
    }
    pio->CFG[0 + index] = regval;
}

bool GPIOBUS_BananaM2p::GetSignal(int pin) const
{
    GPIO_FUNCTION_TRACE
    int gpio_num = pin;

    uint32_t regval = 0;
    int bank        = SunXI::GPIO_BANK(gpio_num); // gpio >> 5
    int num         = SunXI::GPIO_NUM(gpio_num);  // gpio & 0x1F

    regval = (signals[bank] >> num) & 0x1;
    return regval != 0;
}

void GPIOBUS_BananaM2p::SetSignal(int pin, bool ast)
{
    GPIO_FUNCTION_TRACE
    int gpio_num = pin;

#if SIGNAL_CONTROL_MODE == 0
    //	  True  : 0V
    //	  False : Open collector output (disconnect from bus)
    int sunxi_gpio_state = (ast == true) ? SunXI::HIGH : SunXI::LOW;
#elif SIGNAL_CONTROL_MODE == 1
    //	  True  : 0V   -> (CONVERT) -> 0V
    //	  False : 3.3V -> (CONVERT) -> Open collector output
    LOGWARN("%s:%d THIS LOGIC NEEDS TO BE VALIDATED/TESTED", __PRETTY_FUNCTION__, __LINE__)
    int sunxi_gpio_state = (ast == true) ? SunXI::HIGH : SunXI::LOW;
#elif SIGNAL_CONTROL_MODE == 2
    //	  True  : 3.3V -> (CONVERT) -> 0V
    //	  False : 0V   -> (CONVERT) -> Open collector output
    LOGWARN("%s:%d THIS LOGIC NEEDS TO BE VALIDATED/TESTED", __PRETTY_FUNCTION__, __LINE__)
    int sunxi_gpio_state = (ast == true) ? SunXI::LOW : SunXI::HIGH;
#endif // SIGNAL_CONTROL_MODE

    LOGTRACE("gpio(%d) sunxi_state(%d)", gpio_num, sunxi_gpio_state)

    int bank = SunXI::GPIO_BANK(gpio_num); // gpio >> 5
    int num  = SunXI::GPIO_NUM(gpio_num);  // gpio & 0x1F

    volatile SunXI::sunxi_gpio_t *pio = &(pio_map->gpio_bank[bank]);

    /* DK, for PL and PM */
    if (bank >= 11) {
        LOGTRACE("bank > 11")
        bank -= 11;
        pio = &(r_pio_map->gpio_bank[bank]);
    }

    if (sunxi_gpio_state == SunXI::HIGH)
    	pio->DAT = pio->DAT & ~(1 << num);
    else
    	pio->DAT = pio->DAT | (1 << num);
}

void GPIOBUS_BananaM2p::DisableIRQ()
{
    *tmr_ctrl = 0b00;
}

void GPIOBUS_BananaM2p::EnableIRQ()
{
    *tmr_ctrl = 0b11;
}

void GPIOBUS_BananaM2p::PinConfig(int pin, int mode)
{
    GPIO_FUNCTION_TRACE
    int gpio_num        = pin;
    int sunxi_direction = (mode == GPIO_INPUT) ? SunXI::INPUT : SunXI::OUTPUT;

    sunxi_setup_gpio(gpio_num, sunxi_direction, -1);
}

void GPIOBUS_BananaM2p::PullConfig(int pin, int mode)
{
    GPIO_FUNCTION_TRACE
#if defined(__x86_64__) || defined(__X86__) || defined(__aarch64__)
    dummy_var++; // Need to do something to prevent Sonar from claiming this should be a const function
    (void)pin;
    (void)mode;
    return;
#else

    // Note: this will throw an exception if an invalid pin is specified
    int gpio_num           = pin;
    int pull_up_down_state = 0;

    switch (mode) {
    case GPIO_PULLNONE:
        pull_up_down_state = SunXI::PUD_OFF;
        break;
    case GPIO_PULLUP:
        pull_up_down_state = SunXI::PUD_UP;
        break;
    case GPIO_PULLDOWN:
        pull_up_down_state = SunXI::PUD_DOWN;
        break;
    default:
        LOGERROR("%s INVALID PIN MODE", __PRETTY_FUNCTION__);
        return;
    }

    uint32_t regval = 0;
    int bank        = SunXI::GPIO_BANK(gpio_num);       // gpio >> 5
    int index       = SunXI::GPIO_PUL_INDEX(gpio_num);  // (gpio & 0x1f) >> 4
    int offset      = SunXI::GPIO_PUL_OFFSET(gpio_num); // (gpio) & 0x0F) << 1
    LOGTRACE("%s gpio(%d) bank(%d) index(%d) offset(%d)", __PRETTY_FUNCTION__, gpio_num, bank, index, offset)

    volatile SunXI::sunxi_gpio_t *pio = &(pio_map->gpio_bank[bank]);
    /* DK, for PL and PM */
    if (bank >= 11) {
        bank -= 11;
        pio = &(r_pio_map->gpio_bank[bank]);
    }

    regval = *(&pio->PULL[0] + index);
    regval &= ~(3 << offset);
    regval |= pull_up_down_state << offset;
    pio->PULL[0 + index] = regval;
#endif
}

void GPIOBUS_BananaM2p::PinSetSignal(int pin, bool ast)
{
    GPIO_FUNCTION_TRACE
    int gpio_num = pin;

    int sunxi_gpio_state = (ast == true) ? SunXI::HIGH : SunXI::LOW;
    sunxi_output_gpio(gpio_num, sunxi_gpio_state);
}

void GPIOBUS_BananaM2p::DrvConfig(uint32_t drive)
{
    GPIO_FUNCTION_TRACE

    for (auto pin : SignalTable) {
        if (pin == -1) {
            continue;
        }

        LOGTRACE("Configuring GPIO %d to drive strength %d", pin, drive)

        uint32_t regval = 0;
        int bank        = SunXI::GPIO_BANK(pin);       // gpio >> 5
        int index       = SunXI::GPIO_DRV_INDEX(pin);  // (gpio & 0x1F) >> 3
        int offset      = SunXI::GPIO_DRV_OFFSET(pin); // ((gpio & 0x1F) & 0x7) << 2
        LOGTRACE("%s gpio(%d) bank(%d) index(%d) offset(%d)", __PRETTY_FUNCTION__, pin, bank, index, offset)

        volatile SunXI::sunxi_gpio_t *pio = &(pio_map->gpio_bank[bank]);
        /* DK, for PL and PM */
        if (bank >= 11) {
            bank -= 11;
            pio = &(r_pio_map->gpio_bank[bank]);
        }

        // Get current register value
        regval = pio->DRV[0 + index];
        // Clear the DRV value for that gpio
        regval &= ~(0x7 << offset); // 0xf?
        // Set the new DRV strength
        regval |= (drive & 0b11) << offset;
        // Save back to the register
        pio->DRV[0 + index] = regval;
    }

    // #endif // if __arm__
}

uint32_t GPIOBUS_BananaM2p::Acquire()
{
    GPIO_FUNCTION_TRACE

    for (auto bank : gpio_banks) {
        volatile SunXI::sunxi_gpio_t *pio = &(pio_map->gpio_bank[bank]);
        /* DK, for PL and PM */
        if (bank >= 11) {
            pio = &(r_pio_map->gpio_bank[bank - 11]);
        }

        uint32_t regval = pio->DAT;

#if SIGNAL_CONTROL_MODE < 2
        // Invert if negative logic (internal processing is unified to positive logic)
        regval = ~regval;
#endif // SIGNAL_CONTROL_MODE
        signals[bank] = regval;
    }
    // TODO: This should do something someday....
    return 0;
}

int GPIOBUS_BananaM2p::sunxi_setup(void)
{
    GPIO_FUNCTION_TRACE
#if defined(__x86_64__) || defined(__X86__) || defined(__aarch64__)
    dummy_var++; // Need to do something to prevent Sonar from claiming this should be a const function
    return SunXI::SETUP_MMAP_FAIL;
#else
    int mem_fd;
    uint8_t *gpio_mem;

    // mmap the GPIO memory registers
    if ((mem_fd = open("/dev/mem", O_RDWR | O_SYNC)) < 0) {
        LOGERROR("Error: Unable to open /dev/mem. Are you running as root?")
        LOGDEBUG("errno: [%08X] %s", errno, strerror(errno));
        return SunXI::SETUP_DEVMEM_FAIL;
    }

    if ((gpio_mem = (uint8_t *)malloc(SunXI::BLOCK_SIZE + (SunXI::PAGE_SIZE - 1))) == NULL) {
        LOGERROR("Error: Unable to allocate gpio memory. Are you running as root?")
        LOGDEBUG("errno: [%08X] %s", errno, strerror(errno));
        return SunXI::SETUP_DEVMEM_FAIL;
    }

    if ((uint32_t)gpio_mem % SunXI::PAGE_SIZE)
        gpio_mem += SunXI::PAGE_SIZE - ((uint32_t)gpio_mem % SunXI::PAGE_SIZE);

    gpio_map = (uint32_t *)mmap((caddr_t)gpio_mem, SunXI::BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED,
                                mem_fd, SunXI::SUNXI_GPIO_BASE);
    if ((void *)gpio_map == MAP_FAILED) {
        LOGERROR("Error: Unable to map gpio memory. Are you running as root?")
        LOGDEBUG("errno: [%08X] %s", errno, strerror(errno));
        return SunXI::SETUP_MMAP_FAIL;
    }
    pio_map = (volatile SunXI::sunxi_gpio_reg *)(gpio_map + (SunXI::SUNXI_GPIO_REG_OFFSET >> 2));
    LOGTRACE("gpio_mem[%p] gpio_map[%p] pio_map[%p]", gpio_mem, gpio_map, pio_map)
    // R_PIO GPIO LMN
    r_gpio_map = (uint32_t *)mmap((caddr_t)0, SunXI::BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd,
                                  SunXI::SUNXI_R_GPIO_BASE);
    if ((void *)r_gpio_map == MAP_FAILED) {
        LOGERROR("Error: Unable to map r_gpio memory. Are you running as root?")
        LOGDEBUG("errno: [%08X] %s", errno, strerror(errno));
        return SunXI::SETUP_MMAP_FAIL;
    }
    r_pio_map = (volatile SunXI::sunxi_gpio_reg *)(r_gpio_map + (SunXI::SUNXI_R_GPIO_REG_OFFSET >> 2));
    LOGTRACE("r_gpio_map[%p] r_pio_map[%p]", r_gpio_map, r_pio_map)

    tmr_ctrl = gpio_map + ((SunXI::TMR_REGISTER_BASE - SunXI::SUNXI_GPIO_BASE) >> 2);
    // LOGINFO("tmr_ctrl offset: %08X value: %08X", (TMR_REGISTER_BASE - SUNXI_GPIO_BASE), *tmr_ctrl);

    close(mem_fd);
    return SETUP_OK;
#endif
}

void GPIOBUS_BananaM2p::sunxi_setup_gpio(int pin, int direction, int pud)
{
    GPIO_FUNCTION_TRACE
#if defined(__x86_64__) || defined(__X86__) || defined(__aarch64__)
    dummy_var++; // Need to do something to prevent Sonar from claiming this should be a const function
    (void)pin;
    (void)direction;
    (void)pud;
    return;
#else
    uint32_t regval = 0;
    int bank        = SunXI::GPIO_BANK(pin);       // gpio >> 5
    int index       = SunXI::GPIO_CFG_INDEX(pin);  // (gpio & 0x1F) >> 3
    int offset      = SunXI::GPIO_CFG_OFFSET(pin); // ((gpio & 0x1F) & 0x7) << 2
    LOGTRACE("%s gpio(%d) bank(%d) index(%d) offset(%d)", __PRETTY_FUNCTION__, pin, bank, index, offset)

    volatile SunXI::sunxi_gpio_t *pio = &(pio_map->gpio_bank[bank]);
    /* DK, for PL and PM */
    if (bank >= 11) {
        bank -= 11;
        pio = &(r_pio_map->gpio_bank[bank]);
    }

    if (pud != -1) {
        set_pullupdn(pin, pud);
    }
    regval = *(&pio->CFG[0 + index]);
    regval &= ~(0x7 << offset); // 0xf?
    if (SunXI::INPUT == direction) {
        pio->CFG[0 + index] = regval;
    } else if (SunXI::OUTPUT == direction) {
        regval |= (1 << offset);
        pio->CFG[0 + index] = regval;
    } else {
        LOGERROR("line:%d gpio number error %d", __LINE__, pin)
    }
#endif
}

void GPIOBUS_BananaM2p::sunxi_output_gpio(int pin, int value)
{
    GPIO_FUNCTION_TRACE
    if (pin < 0) {
        LOGWARN("Invalid GPIO Num")
        return;
    }
    int bank = SunXI::GPIO_BANK(pin); // gpio >> 5
    int num  = SunXI::GPIO_NUM(pin);  // gpio & 0x1F

    LOGTRACE("%s gpio(%d) bank(%d) num(%d) value(%d)", __PRETTY_FUNCTION__, pin, bank, num, value)
    volatile SunXI::sunxi_gpio_t *pio = &(pio_map->gpio_bank[bank]);

    /* DK, for PL and PM */
    if (bank >= 11) {
        bank -= 11;
        pio = &(r_pio_map->gpio_bank[bank]);
    }

    if (value == 0)
    	pio->DAT = pio->DAT & ~(1 << num);
    else
    	pio->DAT = pio->DAT | (1 << num);
}

void GPIOBUS_BananaM2p::sunxi_set_all_gpios(array<uint32_t, 12> &mask, array<uint32_t, 12> &value)
{
    GPIO_FUNCTION_TRACE
    for (size_t bank = 0; bank < mask.size(); bank++) {
        if (mask[bank] == 0) {
            continue;
        }

        volatile SunXI::sunxi_gpio_t *pio;
        if (bank < 11) {
            pio = &(pio_map->gpio_bank[bank]);
        } else {
            pio = &(r_pio_map->gpio_bank[bank - 11]);
        }

        uint32_t reg_val = pio->DAT;
        reg_val &= ~mask[bank];
        reg_val |= value[bank];
        pio->DAT = reg_val;
    }
}

int GPIOBUS_BananaM2p::sunxi_input_gpio(int pin) const
{
    GPIO_FUNCTION_TRACE
    uint32_t regval = 0;
    int bank        = SunXI::GPIO_BANK(pin); // gpio >> 5
    int num         = SunXI::GPIO_NUM(pin);  // gpio & 0x1F

    LOGTRACE("%s gpio(%d) bank(%d) num(%d)", __PRETTY_FUNCTION__, pin, bank, num)
    volatile SunXI::sunxi_gpio_t *pio = &(pio_map->gpio_bank[bank]);
    /* DK, for PL and PM */
    if (bank >= 11) {
        bank -= 11;
        pio = &(r_pio_map->gpio_bank[bank]);
    }

    regval = pio->DAT;
    regval = regval >> num;
    regval &= 1;
    return regval;
}

void GPIOBUS_BananaM2p::set_pullupdn(int pin, int pud)
{
    GPIO_FUNCTION_TRACE
    int clk_offset = SunXI::PULLUPDNCLK_OFFSET + (pin / 32);
    int shift      = (pin % 32);

#ifdef BPI
    if (bpi_found == 1) {
        gpio = *(pinTobcm_BP + pin);
        return sunxi_set_pullupdn(pin, pud);
    }
#endif
    if (pud == SunXI::PUD_DOWN)
        *(gpio_map + SunXI::PULLUPDN_OFFSET) = (*(gpio_map + SunXI::PULLUPDN_OFFSET) & ~3) | SunXI::PUD_DOWN;
    else if (pud == SunXI::PUD_UP)
        *(gpio_map + SunXI::PULLUPDN_OFFSET) = (*(gpio_map + SunXI::PULLUPDN_OFFSET) & ~3) | SunXI::PUD_UP;
    else // pud == PUD_OFF
        *(gpio_map + SunXI::PULLUPDN_OFFSET) &= ~3;

    SunXI::short_wait();
    *(gpio_map + clk_offset) = 1 << shift;
    SunXI::short_wait();
    *(gpio_map + SunXI::PULLUPDN_OFFSET) &= ~3;
    *(gpio_map + clk_offset) = 0;
}
