//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI Reloaded
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
#include "hal/systimer.h"
#include "shared/log.h"

extern int wiringPiMode;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-parameter"

// #pragma GCC diagnostic ignored "-Wformat-truncation"

#define GPIO_BANK(pin) ((pin) >> 5)
#define GPIO_NUM(pin) ((pin)&0x1F)

#define GPIO_CFG_INDEX(pin) (((pin)&0x1F) >> 3)
#define GPIO_CFG_OFFSET(pin) ((((pin)&0x1F) & 0x7) << 2)

#define GPIO_PUL_INDEX(pin) (((pin)&0x1F) >> 4)
#define GPIO_PUL_OFFSET(pin) (((pin)&0x0F) << 1)

#define GPIO_DRV_INDEX(pin) (((pin)&0x1F) >> 4)
#define GPIO_DRV_OFFSET(pin) (((pin)&0x0F) << 1)

#define SUNXI_R_GPIO_BASE 0x01F02000
#define SUNXI_R_GPIO_REG_OFFSET 0xC00
#define SUNXI_GPIO_BASE 0x01C20000
#define SUNXI_GPIO_REG_OFFSET 0x800
#define SUNXI_CFG_OFFSET 0x00
#define SUNXI_DATA_OFFSET 0x10
#define SUNXI_PUD_OFFSET 0x1C
#define SUNXI_BANK_SIZE 0x24

#define MAP_SIZE (4096 * 2)
#define MAP_MASK (MAP_SIZE - 1)

#define FSEL_OFFSET 0          // 0x0000
#define SET_OFFSET 7           // 0x001c / 4
#define CLR_OFFSET 10          // 0x0028 / 4
#define PINLEVEL_OFFSET 13     // 0x0034 / 4
#define EVENT_DETECT_OFFSET 16 // 0x0040 / 4
#define RISING_ED_OFFSET 19    // 0x004c / 4
#define FALLING_ED_OFFSET 22   // 0x0058 / 4
#define HIGH_DETECT_OFFSET 25  // 0x0064 / 4
#define LOW_DETECT_OFFSET 28   // 0x0070 / 4
#define PULLUPDN_OFFSET 37     // 0x0094 / 4
#define PULLUPDNCLK_OFFSET 38  // 0x0098 / 4

#define PAGE_SIZE (4 * 1024)
#define BLOCK_SIZE (4 * 1024)

// #define SUNXI_GPIO_BASE   0x01C20000
#define TMR_REGISTER_BASE 0x01C20C00
#define TMR_IRQ_EN_REG 0x0       // T imer IRQ Enable Register
#define TMR_IRQ_STA_REG 0x4      // Timer Status Register
#define TMR0_CTRL_REG 0x10       // Timer 0 Control Register
#define TMR0_INTV_VALUE_REG 0x14 // Timer 0 Interval Value Register
#define TMR0_CUR_VALUE_REG 0x18  // Timer 0 Current Value Register
#define TMR1_CTRL_REG 0x20       // Timer 1 Control Register
#define TMR1_INTV_VALUE_REG 0x24 // Timer 1 Interval Value Register
#define TMR1_CUR_VALUE_REG 0x28  // Timer 1 Current Value Register
#define AVS_CNT_CTL_REG 0x80     // AVS Control Register
#define AVS_CNT0_REG 0x84        // AVS Counter 0 Register
#define AVS_CNT1_REG 0x88        // AVS Counter 1 Register
#define AVS_CNT_DIV_REG 0x8C     // AVS Divisor Register
#define WDOG0_IRQ_EN_REG 0xA0    // Watchdog 0 IRQ Enable Register
#define WDOG0_IRQ_STA_REG 0xA4   // Watchdog 0 Status Register
#define WDOG0_CTRL_REG 0xB0      // Watchdog 0 Control Register
#define WDOG0_CFG_REG 0xB4       // Watchdog 0 Configuration Register
#define WDOG0_MODE_REG 0xB8      // Watchdog 0 Mode Register

std::vector<int> gpio_banks;
#define CYAN "\033[36m"    /* Cyan */
#define WHITE "\033[37m"   /* White */
void GPIOBUS_BananaM2p::dump_all(){

        sunxi_gpio_reg_t *regs = ((sunxi_gpio_reg_t *)pio_map);
        printf(CYAN "--- GPIO BANK 0 CFG: %08X %08X %08X %08X\n", regs->gpio_bank[0].CFG[0], regs->gpio_bank[0].CFG[1], regs->gpio_bank[0].CFG[2], regs->gpio_bank[0].CFG[3]);
        printf("---      Dat: (%08X)  DRV: %08X %08X\n", regs->gpio_bank[0].DAT, regs->gpio_bank[0].DRV[0], regs->gpio_bank[0].DRV[1]);
        printf("---      Pull: %08X %08x\n", regs->gpio_bank[0].PULL[0], regs->gpio_bank[0].PULL[1]);
        
        printf("--- GPIO INT CFG: %08X %08X %08X\n", regs->gpio_int.CFG[0], regs->gpio_int.CFG[1], regs->gpio_int.CFG[2]);
        printf("---      CTL: (%08X)  STA: %08X DEB: %08X\n " WHITE, regs->gpio_int.CTL, regs->gpio_int.STA, regs->gpio_int.DEB);

}

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

        int gpio_bank = GPIO_BANK(gpio_num);

        if (std::find(gpio_banks.begin(), gpio_banks.end(), gpio_bank) != gpio_banks.end()) {
            LOGTRACE("Duplicate bank: %d", gpio_bank)

        } else {
            LOGDEBUG("New bank: %d", gpio_bank);
            gpio_banks.push_back(gpio_bank);
        }
    }

    if (int result = sunxi_setup(); result != SETUP_OK) {
        return false;
    }

    // SaveGpioConfig();

    if (!SetupSelEvent()) {
        LOGERROR("Failed to setup SELECT poll event");
        return false;
    }
    LOGTRACE("SetupSelEvent OK!")
    DrvConfig(3);
    // usleep(5000000);

    InitializeGpio();

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
        PinSetSignal(j, OFF);
        PinConfig(j, GPIO_INPUT);
        PullConfig(j, pullmode);
    }

    // Set control signals
    PinSetSignal(BPI_PIN_ACT, OFF);
    PinSetSignal(BPI_PIN_TAD, OFF);
    PinSetSignal(BPI_PIN_IND, OFF);
    PinSetSignal(BPI_PIN_DTD, OFF);
    PinConfig(BPI_PIN_ACT, GPIO_OUTPUT);
    PinConfig(BPI_PIN_TAD, GPIO_OUTPUT);
    PinConfig(BPI_PIN_IND, GPIO_OUTPUT);
    PinConfig(BPI_PIN_DTD, GPIO_OUTPUT);

    // Set the ENABLE signal
    // This is used to show that the application is running
    PinConfig(BPI_PIN_ENB, GPIO_OUTPUT);
    PinSetSignal(BPI_PIN_ENB, ON);
}

void GPIOBUS_BananaM2p::Reset()
{
#if defined(__x86_64__) || defined(__X86__)
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

void GPIOBUS_BananaM2p::Cleanup()
{
    GPIO_FUNCTION_TRACE
#if defined(__x86_64__) || defined(__X86__)
    return;
#else

    SetENB(false);
    SetACT(false);

    // RestoreGpioConfig();

    munmap((void *)gpio_map, BLOCK_SIZE);
    munmap((void *)r_gpio_map, BLOCK_SIZE);

    // Release SEL signal interrupt
#ifdef USE_SEL_EVENT_ENABLE
    close(selevreq.fd);
#endif // USE_SEL_EVENT_ENABLE
#endif
}

void GPIOBUS_BananaM2p::SaveGpioConfig()
{
    GPIO_FUNCTION_TRACE
    saved_gpio_config.resize(14);

    for (auto this_bank : gpio_banks) {
        int bank = this_bank;
        if (this_bank < 11) {
            // std::vector<sunxi_gpio_t> saved_gpio_config;
            memcpy(&(saved_gpio_config[bank]), &((sunxi_gpio_reg_t *)pio_map)->gpio_bank[bank], sizeof(sunxi_gpio_t));
        } else {
            bank -= 11;
            memcpy(&(saved_gpio_config[bank]), &((sunxi_gpio_reg_t *)r_pio_map)->gpio_bank[bank], sizeof(sunxi_gpio_t));
        }
    }
}

void GPIOBUS_BananaM2p::RestoreGpioConfig()
{
    GPIO_FUNCTION_TRACE
    for (auto this_bank : gpio_banks) {
        int bank = this_bank;
        if (this_bank < 11) {
            // std::vector<sunxi_gpio_t> saved_gpio_config;
            memcpy(&((sunxi_gpio_reg_t *)pio_map)->gpio_bank[bank], &(saved_gpio_config[bank]), sizeof(sunxi_gpio_t));
        } else {
            bank -= 11;
            memcpy(&((sunxi_gpio_reg_t *)r_pio_map)->gpio_bank[bank], &(saved_gpio_config[bank]), sizeof(sunxi_gpio_t));
        }
    }
}

bool GPIOBUS_BananaM2p::SetupSelEvent()
{
    GPIO_FUNCTION_TRACE
    int gpio_pin = BPI_PIN_SEL;

    // GPIO chip open
    LOGTRACE("%s GPIO chip open  [%d]", __PRETTY_FUNCTION__, gpio_pin);
    std::string gpio_dev = "/dev/gpiochip0";
    if (GPIO_BANK(gpio_pin) >= 11) {
        gpio_dev = "/dev/gpiochip1";
        LOGWARN("gpiochip1 support isn't implemented yet....");
        LOGWARN("THIS PROBABLY WONT WORK!");
    }

    int gpio_fd = open(gpio_dev.c_str(), 0);
    if (gpio_fd == -1) {
        LOGERROR("Unable to open /dev/gpiochip0. Is RaSCSI already running?")
        return false;
    }

    // Event request setting
    LOGTRACE("%s Event request setting (pin sel: %d)", __PRETTY_FUNCTION__, gpio_pin);
    strcpy(selevreq.consumer_label, "RaSCSI");
    selevreq.lineoffset  = gpio_pin;
    selevreq.handleflags = GPIOHANDLE_REQUEST_INPUT;
#if SIGNAL_CONTROL_MODE < 2
    selevreq.eventflags = GPIOEVENT_REQUEST_FALLING_EDGE;
    LOGTRACE("%s eventflags = GPIOEVENT_REQUEST_FALLING_EDGE", __PRETTY_FUNCTION__);
#else
    selevreq.eventflags = GPIOEVENT_REQUEST_RISING_EDGE;
    LOGTRACE("%s eventflags = GPIOEVENT_REQUEST_RISING_EDGE", __PRETTY_FUNCTION__);
#endif // SIGNAL_CONTROL_MODE

    // Get event request
    if (ioctl(gpio_fd, GPIO_GET_LINEEVENT_IOCTL, &selevreq) == -1) {
        LOGERROR("selevreq.fd = %d %08X", selevreq.fd, (unsigned int)selevreq.fd);
        LOGERROR("Unable to register event request. Is RaSCSI already running?")
        LOGERROR("[%08X] %s", errno, strerror(errno));
        close(gpio_fd);
        return false;
    }

    // Close GPIO chip file handle
    LOGTRACE("%s Close GPIO chip file handle", __PRETTY_FUNCTION__);
    close(gpio_fd);

    // epoll initialization
    LOGTRACE("%s epoll initialization", __PRETTY_FUNCTION__);
    epfd = epoll_create(1);
    if (epfd == -1) {
        LOGERROR("Unable to create the epoll event");
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
    //     // GPIO chip open
    //     LOGTRACE("%s GPIO chip open", __PRETTY_FUNCTION__);
    //     int gpio_fd = open("/dev/gpiochip0", 0);
    //     if (gpio_fd == -1) {
    //         LOGERROR("Unable to open /dev/gpiochip0. Is RaSCSI already running?")
    //         return false;
    //     }

    //     // Event request setting
    //     LOGTRACE("%s Event request setting (pin sel: %d)", __PRETTY_FUNCTION__, phys_to_gpio_map.at(board->pin_sel));
    //     strcpy(selevreq.consumer_label, "RaSCSI");
    //     selevreq.lineoffset  = phys_to_gpio_map->phys_to_gpio_map.at(board->pin_sel);
    //     selevreq.handleflags = GPIOHANDLE_REQUEST_INPUT;
    // #if SIGNAL_CONTROL_MODE < 2
    //     selevreq.eventflags  = GPIOEVENT_REQUEST_FALLING_EDGE;
    //     LOGTRACE("%s eventflags = GPIOEVENT_REQUEST_FALLING_EDGE", __PRETTY_FUNCTION__);
    // #else
    //     selevreq.eventflags = GPIOEVENT_REQUEST_RISING_EDGE;
    //     LOGTRACE("%s eventflags = GPIOEVENT_REQUEST_RISING_EDGE", __PRETTY_FUNCTION__);
    // #endif // SIGNAL_CONTROL_MODE

    //     // Get event request
    //     if (ioctl(gpio_fd, GPIO_GET_LINEEVENT_IOCTL, &selevreq) == -1) {
    //         LOGERROR("selevreq.fd = %d %08X", selevreq.fd, (unsigned int)selevreq.fd);
    //         LOGERROR("Unable to register event request. Is RaSCSI already running?")
    //         LOGERROR("[%08X] %s", errno, strerror(errno));
    //         close(gpio_fd);
    //         return false;
    //     }

    //     // Close GPIO chip file handle
    //     LOGTRACE("%s Close GPIO chip file handle", __PRETTY_FUNCTION__);
    //     close(gpio_fd);

    //     // epoll initialization
    //     LOGTRACE("%s epoll initialization", __PRETTY_FUNCTION__);
    //     epfd = epoll_create(1);
    //     if (epfd == -1) {
    //         LOGERROR("Unable to create the epoll event");
    //         return false;
    //     }
    //     memset(&ev, 0, sizeof(ev));
    //     ev.events  = EPOLLIN | EPOLLPRI;
    //     ev.data.fd = selevreq.fd;
    //     epoll_ctl(epfd, EPOLL_CTL_ADD, selevreq.fd, &ev);
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
    // Set BSY signal
    SetSignal(BPI_PIN_BSY, ast);

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
    SetSignal(BPI_PIN_IO, ast);

    if (actmode == mode_e::TARGET) {
        // Change the data input/output direction by IO signal
        if (ast) {
            SetControl(BPI_PIN_DTD, DTD_OUT);
            SetDAT(0);
            SetMode(BPI_PIN_DT0, OUT);
            SetMode(BPI_PIN_DT1, OUT);
            SetMode(BPI_PIN_DT2, OUT);
            SetMode(BPI_PIN_DT3, OUT);
            SetMode(BPI_PIN_DT4, OUT);
            SetMode(BPI_PIN_DT5, OUT);
            SetMode(BPI_PIN_DT6, OUT);
            SetMode(BPI_PIN_DT7, OUT);
            SetMode(BPI_PIN_DP, OUT);
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
    static uint8_t prev_data = 0;
    // // LOGWARN("%s NOT IMPLEMENTED", __PRETTY_FUNCTION__)
    // LOGTRACE("0:%X 1:%X 2:%X 3:%X 4:%X 5:%X 6:%X 7:%X P:%X", GetSignal(BPI_PIN_DT0),
    //          GetSignal(BPI_PIN_DT1), GetSignal(BPI_PIN_DT2), GetSignal(BPI_PIN_DT3), GetSignal(BPI_PIN_DT4),
    //  GetSignal(BPI_PIN_DT5), GetSignal(BPI_PIN_DT6), GetSignal(BPI_PIN_DT7), GetSignal(BPI_PIN_DP));
    // TODO: This is crazy inefficient...
    uint8_t data = ((GetSignal(BPI_PIN_DT0) ? 0x01 : 0x00) << 0) | ((GetSignal(BPI_PIN_DT1) ? 0x01 : 0x00) << 1) |
                   ((GetSignal(BPI_PIN_DT2) ? 0x01 : 0x00) << 2) | ((GetSignal(BPI_PIN_DT3) ? 0x01 : 0x00) << 3) |
                   ((GetSignal(BPI_PIN_DT4) ? 0x01 : 0x00) << 4) | ((GetSignal(BPI_PIN_DT5) ? 0x01 : 0x00) << 5) |
                   ((GetSignal(BPI_PIN_DT6) ? 0x01 : 0x00) << 6) | ((GetSignal(BPI_PIN_DT7) ? 0x01 : 0x00) << 7);

    // if (prev_data != data) {
    //     LOGINFO("Data %02X", data);
    //     prev_data = data;
    // }

    return (uint8_t)data;
    // return 0;
}

void GPIOBUS_BananaM2p::SetDAT(uint8_t dat)
{
    GPIO_FUNCTION_TRACE
    // TODO: This is crazy inefficient...
    PinSetSignal(BPI_PIN_DT0, (dat & (1 << 0)) == 0 ? true : false);
    PinSetSignal(BPI_PIN_DT1, (dat & (1 << 1)) == 0 ? true : false);
    PinSetSignal(BPI_PIN_DT2, (dat & (1 << 2)) == 0 ? true : false);
    PinSetSignal(BPI_PIN_DT3, (dat & (1 << 3)) == 0 ? true : false);
    PinSetSignal(BPI_PIN_DT4, (dat & (1 << 4)) == 0 ? true : false);
    PinSetSignal(BPI_PIN_DT5, (dat & (1 << 5)) == 0 ? true : false);
    PinSetSignal(BPI_PIN_DT6, (dat & (1 << 6)) == 0 ? true : false);
    PinSetSignal(BPI_PIN_DT7, (dat & (1 << 7)) == 0 ? true : false);
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
    const array<int, 9> pintbl = {BPI_PIN_DT0, BPI_PIN_DT1, BPI_PIN_DT2, BPI_PIN_DT3, BPI_PIN_DT4,
                                  BPI_PIN_DT5, BPI_PIN_DT6, BPI_PIN_DT7, BPI_PIN_DP};

    array<bool, 256> tblParity;

    // Create parity table
    for (uint32_t i = 0; i < 0x100; i++) {
        uint32_t bits   = i;
        uint32_t parity = 0;
        for (int j = 0; j < 8; j++) {
            parity ^= bits & 1;
            bits >>= 1;
        }
        parity       = ~parity;
        tblParity[i] = parity & 1;
    }

#if SIGNAL_CONTROL_MODE == 0
    // Mask and setting data generation
    for (auto &tbl : tblDatMsk) {
        tbl.fill(-1);
    }
    for (auto &tbl : tblDatSet) {
        tbl.fill(0);
    }

    for (uint32_t i = 0; i < 0x100; i++) {
        // Bit string for inspection
        uint32_t bits = i;

        // Get parity
        if (tblParity[i]) {
            bits |= (1 << 8);
        }

        // Bit check
        for (int j = 0; j < 9; j++) {
            // Index and shift amount calculation
            int index = pintbl[j] / 10;
            int shift = (pintbl[j] % 10) * 3;

            // Mask data
            tblDatMsk[index][i] &= ~(0x7 << shift);

            // Setting data
            if (bits & 1) {
                tblDatSet[index][i] |= (1 << shift);
            }

            bits >>= 1;
        }
    }
#else
    for (uint32_t i = 0; i < 0x100; i++) {
        // Bit string for inspection
        uint32_t bits = i;

        // Get parity
        if (tblParity[i]) {
            bits |= (1 << 8);
        }

#if SIGNAL_CONTROL_MODE == 1
        // Negative logic is inverted
        bits = ~bits;
#endif

        // Create GPIO register information
        uint32_t gpclr = 0;
        uint32_t gpset = 0;
        for (int j = 0; j < 9; j++) {
            if (bits & 1) {
                gpset |= (1 << pintbl[j]);
            } else {
                gpclr |= (1 << pintbl[j]);
            }
            bits >>= 1;
        }

        tblDatMsk[i] = gpclr;
        tblDatSet[i] = gpset;
    }
#endif
}

//---------------------------------------------------------------------------
//
//	Control signal setting
//
//---------------------------------------------------------------------------
// void GPIOBUS_BananaM2p::SetControl(int pin, bool ast)
// {
// 	PinSetSignal(pin, ast);
// }

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
void GPIOBUS_BananaM2p::SetMode(int pin, int mode)
{
    GPIO_FUNCTION_TRACE
    int gpio      = pin;
    int direction = mode;
    // int sunxi_gpio_direction = (mode == int::GPIO_INPUT) ? INPUT : OUTPUT;
    // int sunxi_gpio_direction = mode;

    // sunxi_setup_gpio(gpio_num, sunxi_gpio_direction, -1);
    // LOGWARN("%s NOT IMPLEMENTED", __PRETTY_FUNCTION__)
    uint32_t regval = 0;
    int bank        = GPIO_BANK(gpio);       // gpio >> 5
    int index       = GPIO_CFG_INDEX(gpio);  // (gpio & 0x1F) >> 3
    int offset      = GPIO_CFG_OFFSET(gpio); // ((gpio & 0x1F) & 0x7) << 2
    LOGTRACE("%s gpio(%d) bank(%d) index(%d) offset(%d) dir(%s)", __PRETTY_FUNCTION__, gpio, bank, index, offset,
             (GPIO_INPUT == direction) ? "IN" : "OUT")

    sunxi_gpio_t *pio = &((sunxi_gpio_reg_t *)pio_map)->gpio_bank[bank];
    /* DK, for PL and PM */
    if (bank >= 11) {
        bank -= 11;
        pio = &((sunxi_gpio_reg_t *)r_pio_map)->gpio_bank[bank];
    }

    regval = *(&pio->CFG[0] + index);

    // Clear the cfg field
    regval &= ~(0x7 << offset); // 0xf?
    if (GPIO_INPUT == direction) {
        #ifdef USE_SEL_EVENT_ENABLE
        if(gpio == BPI_PIN_SEL){
            regval |= (((uint32_t)gpio_configure_values_e::gpio_interupt) << offset);
        }
        #endif // USE_SEL_EVENT_ENABLE
        // Note: gpio_configure_values_e::gpio_input is 0b00
        *(&pio->CFG[0] + index) = regval;
    } else if (GPIO_OUTPUT == direction) {
        regval |= (((uint32_t)gpio_configure_values_e::gpio_output) << offset);
        *(&pio->CFG[0] + index) = regval;
    } else {
        LOGERROR("line:%dgpio number error\n", __LINE__);
    }
}

bool GPIOBUS_BananaM2p::GetSignal(int pin) const
{
    GPIO_FUNCTION_TRACE
    int gpio_num = pin;
    // int sunxi_gpio_state = sunxi_input_gpio(gpio_num);

    uint32_t regval = 0;
    int bank        = GPIO_BANK(gpio_num); // gpio >> 5
    int num         = GPIO_NUM(gpio_num);  // gpio & 0x1F

    LOGDEBUG("%s gpio(%d) bank(%d) num(%d)", __PRETTY_FUNCTION__, (int)gpio_num, (int)bank, (int)num);

    regval = (signals[bank] >> num) & 0x1;
    return regval == 0;
    // sunxi_gpio_t *pio = &((sunxi_gpio_reg_t *)pio_map)->gpio_bank[bank];
    // /* DK, for PL and PM */
    // if (bank >= 11) {
    //     bank -= 11;
    //     pio = &((sunxi_gpio_reg_t *)r_pio_map)->gpio_bank[bank];
    // }

    // regval = *(&pio->DAT);
    // regval = regval >> num;
    // regval &= 1;

    // int sunxi_gpio_state = regval;

    // LOGDEBUG("%s GPIO %d is set to %d", __PRETTY_FUNCTION__, gpio_num, sunxi_gpio_state);

    // if (sunxi_gpio_state == LOW) {
    //     return true;
    // } else {
    //     return false;
    // }
    // return (digitalRead(pin) != 0);
}

void GPIOBUS_BananaM2p::SetSignal(int pin, bool ast)
{
    GPIO_FUNCTION_TRACE
    int gpio_num = pin;

#if SIGNAL_CONTROL_MODE == 0
    //	  True  : 0V
    //	  False : Open collector output (disconnect from bus)
    int sunxi_gpio_state = (ast == true) ? HIGH : LOW;
#elif SIGNAL_CONTROL_MODE == 1
    //	  True  : 0V   -> (CONVERT) -> 0V
    //	  False : 3.3V -> (CONVERT) -> Open collector output
    LOGWARN("%s:%d THIS LOGIC NEEDS TO BE VALIDATED/TESTED", __PRETTY_FUNCTION__, __LINE__)
    int sunxi_gpio_state = (ast == true) ? HIGH : LOW;
#elif SIGNAL_CONTROL_MODE == 2
    //	  True  : 3.3V -> (CONVERT) -> 0V
    //	  False : 0V   -> (CONVERT) -> Open collector output
    LOGWARN("%s:%d THIS LOGIC NEEDS TO BE VALIDATED/TESTED", __PRETTY_FUNCTION__, __LINE__)
    int sunxi_gpio_state = (ast == true) ? LOW : HIGH;
#endif // SIGNAL_CONTROL_MODE

    LOGTRACE("gpio(%d) sunxi_state(%d)", gpio_num, sunxi_gpio_state)
    // sunxi_output_gpio(gpio_num, sunxi_gpio_state);

    int bank = GPIO_BANK(gpio_num); // gpio >> 5
    int num  = GPIO_NUM(gpio_num);  // gpio & 0x1F

    sunxi_gpio_t *pio = &((sunxi_gpio_reg_t *)pio_map)->gpio_bank[bank];

    /* DK, for PL and PM */
    if (bank >= 11) {
        LOGTRACE("bank > 11");
        bank -= 11;
        pio = &((sunxi_gpio_reg_t *)r_pio_map)->gpio_bank[bank];
    }

    if (sunxi_gpio_state == HIGH)
        *(&pio->DAT) &= ~(1 << num);
    else
        *(&pio->DAT) |= (1 << num);
}

void GPIOBUS_BananaM2p::DisableIRQ()
{
    LOGERROR("%s not tested????", __PRETTY_FUNCTION__)
    *tmr_ctrl = 0b00;
}

void GPIOBUS_BananaM2p::EnableIRQ()
{
    LOGERROR("%s not tested????", __PRETTY_FUNCTION__)
    *tmr_ctrl = 0b11;
}

void GPIOBUS_BananaM2p::PinConfig(int pin, int mode)
{
    GPIO_FUNCTION_TRACE
    int gpio_num = pin;
    // try {
    //     gpio_num = pin;
    // } catch (const std::out_of_range &e) {
    //     LOGERROR("%s %d is not a valid pin", __PRETTY_FUNCTION__, (int)pin);
    // }
    // int sunxi_direction = mode;
    int sunxi_direction = (mode == GPIO_INPUT) ? INPUT : OUTPUT;

    sunxi_setup_gpio(gpio_num, sunxi_direction, -1);

    // if(mode == bool::GPIO_INPUT){
    //     pinMode(pin, INPUT);

    // }else{
    //     pinMode(pin, OUTPUT);
    // }
}

void GPIOBUS_BananaM2p::PullConfig(int pin, int mode)
{
    GPIO_FUNCTION_TRACE
#ifndef __arm__
        (void)
    pin;
    (void)mode;
    return;
#else

    // Note: this will throw an exception if an invalid pin is specified
    int gpio_num           = pin;
    int pull_up_down_state = 0;

    switch (mode) {
    case GPIO_PULLNONE:
        pull_up_down_state = PUD_OFF;
        break;
    case GPIO_PULLUP:
        pull_up_down_state = PUD_UP;
        break;
    case GPIO_PULLDOWN:
        pull_up_down_state = PUD_DOWN;
        break;
    default:
        LOGERROR("%s INVALID PIN MODE", __PRETTY_FUNCTION__);
        return;
    }

    uint32_t regval = 0;
    int bank        = GPIO_BANK(gpio_num);       // gpio >> 5
    int index       = GPIO_PUL_INDEX(gpio_num);  // (gpio & 0x1f) >> 4
    int offset      = GPIO_PUL_OFFSET(gpio_num); // (gpio) & 0x0F) << 1
    LOGTRACE("%s gpio(%d) bank(%d) index(%d) offset(%d)", __PRETTY_FUNCTION__, gpio_num, bank, index, offset)

    sunxi_gpio_t *pio = &((sunxi_gpio_reg_t *)pio_map)->gpio_bank[bank];
    /* DK, for PL and PM */
    if (bank >= 11) {
        bank -= 11;
        pio = &((sunxi_gpio_reg_t *)r_pio_map)->gpio_bank[bank];
    }

    regval = *(&pio->PULL[0] + index);
    regval &= ~(3 << offset);
    regval |= pull_up_down_state << offset;
    *(&pio->PULL[0] + index) = regval;
#endif
}

void GPIOBUS_BananaM2p::PinSetSignal(int pin, bool ast)
{
    GPIO_FUNCTION_TRACE
    int gpio_num = pin;

    int sunxi_gpio_state = (ast == true) ? HIGH : LOW;
    sunxi_output_gpio(gpio_num, sunxi_gpio_state);

    // digitalWrite(pin, ast);
    // LOGERROR("%s not implemented!!", __PRETTY_FUNCTION__)
}

void GPIOBUS_BananaM2p::DrvConfig(uint32_t drive)
{
    GPIO_FUNCTION_TRACE
    // #ifndef __arm__

    //     (void)drive;
    //     return;

    // #else

    for (auto gpio : SignalTable) {
        if (gpio == -1) {
            continue;
        }

        LOGTRACE("Configuring GPIO %d to drive strength %d", gpio, drive);

        uint32_t regval = 0;
        int bank        = GPIO_BANK(gpio);       // gpio >> 5
        int index       = GPIO_DRV_INDEX(gpio);  // (gpio & 0x1F) >> 3
        int offset      = GPIO_DRV_OFFSET(gpio); // ((gpio & 0x1F) & 0x7) << 2
        LOGTRACE("%s gpio(%d) bank(%d) index(%d) offset(%d)", __PRETTY_FUNCTION__, gpio, bank, index, offset)

        sunxi_gpio_t *pio = &((sunxi_gpio_reg_t *)pio_map)->gpio_bank[bank];
        /* DK, for PL and PM */
        if (bank >= 11) {
            bank -= 11;
            pio = &((sunxi_gpio_reg_t *)r_pio_map)->gpio_bank[bank];
        }

        // Get current register value
        regval = *(&pio->DRV[0] + index);
        // Clear the DRV value for that gpio
        regval &= ~(0x7 << offset); // 0xf?
        // Set the new DRV strength
        regval |= (drive & 0b11) << offset;
        // Save back to the register
        *(&pio->DRV[0] + index) = regval;
    }

    // #endif // if __arm__
}

uint32_t GPIOBUS_BananaM2p::Acquire()
{
    // (void)sunxi_capture_all_gpio();
    // uint32_t GPIOBUS_BananaM2p::sunxi_capture_all_gpio()
    // {
    GPIO_FUNCTION_TRACE

    uint32_t value = 0;

    for (auto bank : gpio_banks) {
        sunxi_gpio_t *pio = &((sunxi_gpio_reg_t *)pio_map)->gpio_bank[bank];
        /* DK, for PL and PM */
        if (bank >= 11) {
            pio = &((sunxi_gpio_reg_t *)r_pio_map)->gpio_bank[(bank - 11)];
        }

        uint32_t regval = *(&pio->DAT);
        signals[bank]   = regval;
    }
    // TODO: This should do something someday....
    return 0;
}

#pragma GCC diagnostic pop

uint32_t GPIOBUS_BananaM2p::sunxi_readl(volatile uint32_t *addr)
{
    GPIO_FUNCTION_TRACE
#ifndef __arm__
        (void)
    addr;
    return 0;
#else
    printf("sunxi_readl\n");
    uint32_t val       = 0;
    uint32_t mmap_base = (uint32_t)addr & (~MAP_MASK);
    uint32_t mmap_seek = ((uint32_t)addr - mmap_base) >> 2;
    val                = *(gpio_map + mmap_seek);
    return val;
#endif
}

void GPIOBUS_BananaM2p::sunxi_writel(volatile uint32_t *addr, uint32_t val)
{
    GPIO_FUNCTION_TRACE
#ifndef __arm__
        (void)
    addr;
    (void)val;
    return;
#else
    printf("sunxi_writel\n");
    uint32_t mmap_base      = (uint32_t)addr & (~MAP_MASK);
    uint32_t mmap_seek      = ((uint32_t)addr - mmap_base) >> 2;
    *(gpio_map + mmap_seek) = val;
#endif
}

int GPIOBUS_BananaM2p::sunxi_setup(void)
{
    GPIO_FUNCTION_TRACE
#ifndef __arm__
    return SETUP_MMAP_FAIL;
#else
    int mem_fd;
    uint8_t *gpio_mem;
    // uint8_t *r_gpio_mem;
    // uint32_t peri_base;
    // uint32_t gpio_base;
    // unsigned char buf[4];
    // FILE *fp;
    // char buffer[1024];
    // char hardware[1024];
    // int found = 0;

    // mmap the GPIO memory registers
    if ((mem_fd = open("/dev/mem", O_RDWR | O_SYNC)) < 0) {
        LOGERROR("Error: Unable to open /dev/mem. Are you running as root?")
        LOGDEBUG("errno: [%08X] %s", errno, strerror(errno));
        return SETUP_DEVMEM_FAIL;
    }

    if ((gpio_mem = (uint8_t *)malloc(BLOCK_SIZE + (PAGE_SIZE - 1))) == NULL) {
        LOGERROR("Error: Unable to allocate gpio memory. Are you running as root?")
        LOGDEBUG("errno: [%08X] %s", errno, strerror(errno));
        return SETUP_DEVMEM_FAIL;
    }

    if ((uint32_t)gpio_mem % PAGE_SIZE)
        gpio_mem += PAGE_SIZE - ((uint32_t)gpio_mem % PAGE_SIZE);

    gpio_map = (uint32_t *)mmap((caddr_t)gpio_mem, BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, mem_fd,
                                SUNXI_GPIO_BASE);
    if ((void *)gpio_map == MAP_FAILED) {
        LOGERROR("Error: Unable to map gpio memory. Are you running as root?")
        LOGDEBUG("errno: [%08X] %s", errno, strerror(errno));
        return SETUP_MMAP_FAIL;
    }
    pio_map = gpio_map + (SUNXI_GPIO_REG_OFFSET >> 2);
    LOGTRACE("gpio_mem[%p] gpio_map[%p] pio_map[%p]", gpio_mem, gpio_map, pio_map)
    // R_PIO GPIO LMN
    r_gpio_map =
        (uint32_t *)mmap((caddr_t)0, BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, SUNXI_R_GPIO_BASE);
    if ((void *)r_gpio_map == MAP_FAILED) {
        LOGERROR("Error: Unable to map r_gpio memory. Are you running as root?")
        LOGDEBUG("errno: [%08X] %s", errno, strerror(errno));
        return SETUP_MMAP_FAIL;
    }
    r_pio_map = r_gpio_map + (SUNXI_R_GPIO_REG_OFFSET >> 2);
    LOGTRACE("r_gpio_map[%p] r_pio_map[%p]", r_gpio_map, r_pio_map)

    tmr_ctrl = gpio_map + ((TMR_REGISTER_BASE - SUNXI_GPIO_BASE) >> 2);
    LOGINFO("tmr_ctrl offset: %08X value: %08X", (TMR_REGISTER_BASE - SUNXI_GPIO_BASE), *tmr_ctrl);

    close(mem_fd);
    return SETUP_OK;
#endif
}

void GPIOBUS_BananaM2p::sunxi_setup_gpio(int gpio, int direction, int pud)
{
    GPIO_FUNCTION_TRACE
#ifndef __arm__
        (void)
    gpio;
    (void)direction;
    (void)pud;
    return;
#else
    uint32_t regval = 0;
    int bank        = GPIO_BANK(gpio);       // gpio >> 5
    int index       = GPIO_CFG_INDEX(gpio);  // (gpio & 0x1F) >> 3
    int offset      = GPIO_CFG_OFFSET(gpio); // ((gpio & 0x1F) & 0x7) << 2
    LOGTRACE("%s gpio(%d) bank(%d) index(%d) offset(%d)", __PRETTY_FUNCTION__, gpio, bank, index, offset)

    sunxi_gpio_t *pio = &((sunxi_gpio_reg_t *)pio_map)->gpio_bank[bank];
    /* DK, for PL and PM */
    if (bank >= 11) {
        bank -= 11;
        pio = &((sunxi_gpio_reg_t *)r_pio_map)->gpio_bank[bank];
    }

    if (pud != -1) {
        set_pullupdn(gpio, pud);
    }
    regval = *(&pio->CFG[0] + index);
    regval &= ~(0x7 << offset); // 0xf?
    if (INPUT == direction) {
        *(&pio->CFG[0] + index) = regval;
    } else if (OUTPUT == direction) {
        regval |= (1 << offset);
        *(&pio->CFG[0] + index) = regval;
    } else {
        LOGERROR("line:%dgpio number error\n", __LINE__);
    }
#endif
}

// Contribution by Eric Ptak <trouch@trouch.com>
int GPIOBUS_BananaM2p::sunxi_gpio_function(int gpio)
{
    GPIO_FUNCTION_TRACE
    uint32_t regval = 0;
    int bank        = GPIO_BANK(gpio);       // gpio >> 5
    int index       = GPIO_CFG_INDEX(gpio);  // (gpio & 0x1F) >> 3
    int offset      = GPIO_CFG_OFFSET(gpio); // ((gpio & 0x1F) & 0x7) << 2

    sunxi_gpio_t *pio = &((sunxi_gpio_reg_t *)pio_map)->gpio_bank[bank];
    /* DK, for PL and PM */
    if (bank >= 11) {
        bank -= 11;
        pio = &((sunxi_gpio_reg_t *)r_pio_map)->gpio_bank[bank];
    }

    regval = *(&pio->CFG[0] + index);
    regval >>= offset;
    regval &= 7;
    return regval; // 0=input, 1=output, 4=alt0
}

void GPIOBUS_BananaM2p::sunxi_output_gpio(int gpio, int value)
{
    GPIO_FUNCTION_TRACE
    if (gpio < 0) {
        LOGWARN("Invalid GPIO Num")
        return;
    }
    int bank = GPIO_BANK(gpio); // gpio >> 5
    int num  = GPIO_NUM(gpio);  // gpio & 0x1F

    LOGTRACE("%s gpio(%d) bank(%d) num(%d) value(%d)", __PRETTY_FUNCTION__, gpio, bank, num, value)
    // LOGTRACE("pio_map: %p", pio_map)
    // LOGTRACE("pio_map->gpio_bank: %p", &((sunxi_gpio_reg_t *)pio_map)->gpio_bank[0])
    sunxi_gpio_t *pio = &((sunxi_gpio_reg_t *)pio_map)->gpio_bank[bank];
    // LOGTRACE("pio: %p", pio)
    /* DK, for PL and PM */
    if (bank >= 11) {
        LOGTRACE("bank > 11");
        bank -= 11;
        pio = &((sunxi_gpio_reg_t *)r_pio_map)->gpio_bank[bank];
    }

    if (value == 0)
        *(&pio->DAT) &= ~(1 << num);
    else
        *(&pio->DAT) |= (1 << num);
}

int GPIOBUS_BananaM2p::sunxi_input_gpio(int gpio) const
{
    GPIO_FUNCTION_TRACE
    uint32_t regval = 0;
    int bank        = GPIO_BANK(gpio); // gpio >> 5
    int num         = GPIO_NUM(gpio);  // gpio & 0x1F

    LOGTRACE("%s gpio(%d) bank(%d) num(%d)", __PRETTY_FUNCTION__, gpio, bank, num);
    sunxi_gpio_t *pio = &((sunxi_gpio_reg_t *)pio_map)->gpio_bank[bank];
    /* DK, for PL and PM */
    if (bank >= 11) {
        bank -= 11;
        pio = &((sunxi_gpio_reg_t *)r_pio_map)->gpio_bank[bank];
    }

    regval = *(&pio->DAT);
    regval = regval >> num;
    regval &= 1;
    return regval;
}

int GPIOBUS_BananaM2p::bpi_piGpioLayout(void)
{
    GPIO_FUNCTION_TRACE
    // FILE *bpiFd;
    // char buffer[1024];
    // char hardware[1024];
    // struct BPIBoards *board = nullptr;
    int gpioLayout = -1;

    // if (gpioLayout != -1) // No point checking twice
    //     return gpioLayout;

    // bpi_found = 0; // -1: not init, 0: init but not found, 1: found
    // if ((bpiFd = fopen("/var/lib/bananapi/board.sh", "r")) == NULL) {
    //     return -1;
    // }
    // while (!feof(bpiFd)) {
    //     // TODO: check the output of fgets()
    //     char *ret = fgets(buffer, sizeof(buffer), bpiFd);
    //     (void)ret;
    //     sscanf(buffer, "BOARD=%s", hardware);
    //     // LOGDEBUG("BPI: buffer[%s] hardware[%s]\n", buffer, hardware);

    //     // Search for board:
    //     for (board = bpiboard; board->name != NULL; ++board) {
    //         LOGDEBUG("BPI: name[%s] hardware[%s]\n", board->name, hardware);
    //         if (strcmp(board->name, hardware) == 0) {
    //             // gpioLayout = board->gpioLayout;
    //             gpioLayout = board->model; // BPI: use model to replace gpioLayout
    //             LOGDEBUG("BPI: name[%s] gpioLayout(%d)\n", board->name, gpioLayout);
    //             if (gpioLayout >= 21) {
    //                 bpi_found = 1;
    //                 break;
    //             }
    //         }
    //     }
    //     if (bpi_found == 1) {
    //         break;
    //     }
    // }
    // fclose(bpiFd);
    // LOGDEBUG("BPI: name[%s] gpioLayout(%d)\n", board->name, gpioLayout);
    return gpioLayout;
}

// int GPIOBUS_BananaM2p::bpi_get_rpi_info(rpi_info *info)
// {
//   struct BPIBoards *board=bpiboard;
//   static int  gpioLayout = -1 ;
//   char ram[64];
//   char manufacturer[64];
//   char processor[64];
//   char type[64];

//   gpioLayout = bpi_piGpioLayout () ;
//   printf("BPI: gpioLayout(%d)\n", gpioLayout);
//   if(bpi_found == 1) {
//     board = &bpiboard[gpioLayout];
//     printf("BPI: name[%s] gpioLayout(%d)\n",board->name, gpioLayout);
//     sprintf(ram, "%dMB", piMemorySize [board->mem]);
//     sprintf(type, "%s", piModelNames [board->model]);
//      //add by jackzeng
//      //jude mtk platform
//     if(strcmp(board->name, "bpi-r2") == 0){
//         bpi_found_mtk = 1;
// 	printf("found mtk board\n");
//     }
//     sprintf(manufacturer, "%s", piMakerNames [board->maker]);
//     info->p1_revision = 3;
//     info->type = type;
//     info->ram  = ram;
//     info->manufacturer = manufacturer;
//     if(bpi_found_mtk == 1){
//         info->processor = "MTK";
//     }else{
// 	info->processor = "Allwinner";
//     }

//     strcpy(info->revision, "4001");
// //    pin_to_gpio =  board->physToGpio ;
//     pinToGpio_BP =  board->pinToGpio ;
//     physToGpio_BP = board->physToGpio ;
//     pinTobcm_BP = board->pinTobcm ;
//     //printf("BPI: name[%s] bType(%d) model(%d)\n",board->name, bType, board->model);
//     return 0;
//   }
//   return -1;
// }

void GPIOBUS_BananaM2p::set_pullupdn(int gpio, int pud)
{
    GPIO_FUNCTION_TRACE
    int clk_offset = PULLUPDNCLK_OFFSET + (gpio / 32);
    int shift      = (gpio % 32);

#ifdef BPI
    if (bpi_found == 1) {
        gpio = *(pinTobcm_BP + gpio);
        return sunxi_set_pullupdn(gpio, pud);
    }
#endif
    if (pud == PUD_DOWN)
        *(gpio_map + PULLUPDN_OFFSET) = (*(gpio_map + PULLUPDN_OFFSET) & ~3) | PUD_DOWN;
    else if (pud == PUD_UP)
        *(gpio_map + PULLUPDN_OFFSET) = (*(gpio_map + PULLUPDN_OFFSET) & ~3) | PUD_UP;
    else // pud == PUD_OFF
        *(gpio_map + PULLUPDN_OFFSET) &= ~3;

    short_wait();
    *(gpio_map + clk_offset) = 1 << shift;
    short_wait();
    *(gpio_map + PULLUPDN_OFFSET) &= ~3;
    *(gpio_map + clk_offset) = 0;
}

void GPIOBUS_BananaM2p::short_wait(void)
{
    GPIO_FUNCTION_TRACE
    int i;

    for (i = 0; i < 150; i++) { // wait 150 cycles
        asm volatile("nop");
    }
}

// Extract as specific pin field from a raw data capture
uint32_t GPIOBUS_BananaM2p::GetPinRaw(uint32_t raw_data, uint32_t pin_num)
{
    return ((raw_data >> pin_num) & 1);
}

//---------------------------------------------------------------------------
//
//	Generic Phase Acquisition (Doesn't read GPIO)
//
//---------------------------------------------------------------------------
BUS::phase_t GPIOBUS_BananaM2p::GetPhaseRaw(uint32_t raw_data)
{
    // Selection Phase
    if (GetPinRaw(raw_data, BPI_PIN_SEL)) {
        if (GetPinRaw(raw_data, BPI_PIN_IO)) {
            return BUS::phase_t::reselection;
        } else {
            return BUS::phase_t::selection;
        }
    }

    // Bus busy phase
    if (!GetPinRaw(raw_data, BPI_PIN_BSY)) {
        return BUS::phase_t::busfree;
    }

    // Get target phase from bus signal line
    int mci = GetPinRaw(raw_data, BPI_PIN_MSG) ? 0x04 : 0x00;
    mci |= GetPinRaw(raw_data, BPI_PIN_CD) ? 0x02 : 0x00;
    mci |= GetPinRaw(raw_data, BPI_PIN_IO) ? 0x01 : 0x00;
    return BUS::GetPhase(mci);
}
