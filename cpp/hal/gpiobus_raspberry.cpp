//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI Reloaded
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//
//	[ GPIO-SCSI bus ]
//
//---------------------------------------------------------------------------

#include "hal/gpiobus_raspberry.h"
#include "config.h"
#include "hal/board_type.h"
#include "hal/gpiobus.h"
#include "hal/systimer.h"
#include "log.h"
#include "os.h"
#include <map>
#include <string.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>

const std::map<board_type::pi_physical_pin_e, int> GPIOBUS_Raspberry::phys_to_gpio_map = {
    {board_type::pi_physical_pin_e::PI_PHYS_PIN_03, 2},  {board_type::pi_physical_pin_e::PI_PHYS_PIN_05, 3},
    {board_type::pi_physical_pin_e::PI_PHYS_PIN_07, 4},  {board_type::pi_physical_pin_e::PI_PHYS_PIN_08, 14},
    {board_type::pi_physical_pin_e::PI_PHYS_PIN_10, 15}, {board_type::pi_physical_pin_e::PI_PHYS_PIN_11, 17},
    {board_type::pi_physical_pin_e::PI_PHYS_PIN_12, 18}, {board_type::pi_physical_pin_e::PI_PHYS_PIN_13, 27},
    {board_type::pi_physical_pin_e::PI_PHYS_PIN_15, 22}, {board_type::pi_physical_pin_e::PI_PHYS_PIN_16, 23},
    {board_type::pi_physical_pin_e::PI_PHYS_PIN_18, 24}, {board_type::pi_physical_pin_e::PI_PHYS_PIN_19, 10},
    {board_type::pi_physical_pin_e::PI_PHYS_PIN_21, 9},  {board_type::pi_physical_pin_e::PI_PHYS_PIN_22, 25},
    {board_type::pi_physical_pin_e::PI_PHYS_PIN_23, 11}, {board_type::pi_physical_pin_e::PI_PHYS_PIN_24, 8},
    {board_type::pi_physical_pin_e::PI_PHYS_PIN_26, 7},  {board_type::pi_physical_pin_e::PI_PHYS_PIN_27, 0},
    {board_type::pi_physical_pin_e::PI_PHYS_PIN_28, 1},  {board_type::pi_physical_pin_e::PI_PHYS_PIN_29, 5},
    {board_type::pi_physical_pin_e::PI_PHYS_PIN_31, 6},  {board_type::pi_physical_pin_e::PI_PHYS_PIN_32, 12},
    {board_type::pi_physical_pin_e::PI_PHYS_PIN_33, 13}, {board_type::pi_physical_pin_e::PI_PHYS_PIN_35, 19},
    {board_type::pi_physical_pin_e::PI_PHYS_PIN_36, 16}, {board_type::pi_physical_pin_e::PI_PHYS_PIN_37, 26},
    {board_type::pi_physical_pin_e::PI_PHYS_PIN_38, 20}, {board_type::pi_physical_pin_e::PI_PHYS_PIN_40, 21},
};

#ifdef __linux__
//---------------------------------------------------------------------------
//
//	imported from bcm_host.c
//
//---------------------------------------------------------------------------
static uint32_t get_dt_ranges(const char *filename, uint32_t offset)
{
    GPIO_FUNCTION_TRACE
    uint32_t address = ~0;
    if (FILE *fp = fopen(filename, "rb"); fp) {
        fseek(fp, offset, SEEK_SET);
        if (array<BYTE, 4> buf; fread(buf.data(), 1, buf.size(), fp) == buf.size()) {
            address = (int)buf[0] << 24 | (int)buf[1] << 16 | (int)buf[2] << 8 | (int)buf[3] << 0;
        }
        fclose(fp);
    }
    return address;
}

uint32_t bcm_host_get_peripheral_address()
{
    GPIO_FUNCTION_TRACE
    uint32_t address = get_dt_ranges("/proc/device-tree/soc/ranges", 4);
    if (address == 0) {
        address = get_dt_ranges("/proc/device-tree/soc/ranges", 8);
    }
    address = (address == (uint32_t)~0) ? 0x20000000 : address;
    return address;
}
#endif // __linux__

#ifdef __NetBSD__
// Assume the Raspberry Pi series and estimate the address from CPU
uint32_t bcm_host_get_peripheral_address()
{
    GPIO_FUNCTION_TRACE
    array<char, 1024> buf;
    size_t len = buf.size();
    uint32_t address;

    if (sysctlbyname("hw.model", buf.data(), &len, NULL, 0) || strstr(buf, "ARM1176JZ-S") != buf.data()) {
        // Failed to get CPU model || Not BCM2835
        // use the address of BCM283[67]
        address = 0x3f000000;
    } else {
        // Use BCM2835 address
        address = 0x20000000;
    }
    LOGDEBUG("Peripheral address : 0x%lx\n", address);
    return address;
}
#endif // __NetBSD__

bool GPIOBUS_Raspberry::Init(mode_e mode, board_type::rascsi_board_type_e rascsi_type)
{
    GPIO_FUNCTION_TRACE
    GPIOBUS::Init(mode, rascsi_type);

#if defined(__x86_64__) || defined(__X86__)

    // When we're running on x86, there is no hardware to talk to, so just return.
    return true;
#else
#ifdef USE_SEL_EVENT_ENABLE
    epoll_event ev = {};
#endif

    // Get the base address
    baseaddr = (uint32_t)bcm_host_get_peripheral_address();
    LOGTRACE("%s Base addr: %08X", __PRETTY_FUNCTION__, baseaddr);

    // Open /dev/mem
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd == -1) {
        LOGERROR("Error: Unable to open /dev/mem. Are you running as root?")
        return false;
    }

    LOGTRACE("%s Mapping Memory....", __PRETTY_FUNCTION__);

    // Map peripheral region memory
    void *map = mmap(NULL, 0x1000100, PROT_READ | PROT_WRITE, MAP_SHARED, fd, baseaddr);
    if (map == MAP_FAILED) {
        LOGERROR("Error: Unable to map memory")
        close(fd);
        return false;
    }
    LOGTRACE("%s Done!", __PRETTY_FUNCTION__);

    // Determine the type of raspberry pi from the base address
    if (baseaddr == 0xfe000000) {
        rpitype = 4;
        LOGINFO("%s I'm a pi 4", __PRETTY_FUNCTION__);
    } else if (baseaddr == 0x3f000000) {
        rpitype = 2;
    } else {
        rpitype = 1;
    }

    // GPIO
    gpio = (uint32_t *)map;
    gpio += GPIO_OFFSET / sizeof(uint32_t);
    level = &gpio[GPIO_LEV_0];

    // PADS
    pads = (uint32_t *)map;
    pads += PADS_OFFSET / sizeof(uint32_t);

    // // System timer
    // SysTimer::Init(
    // 	(uint32_t *)map + SYST_OFFSET / sizeof(uint32_t),
    // 	(uint32_t *)map + ARMT_OFFSET / sizeof(uint32_t));
    SysTimer::Init();

    // Interrupt controller
    irpctl = (uint32_t *)map;
    irpctl += IRPT_OFFSET / sizeof(uint32_t);

    // Quad-A7 control
    qa7regs = (uint32_t *)map;
    qa7regs += QA7_OFFSET / sizeof(uint32_t);

    LOGTRACE("%s Mapping GIC Memory....", __PRETTY_FUNCTION__);
    // Map GIC memory
    if (rpitype == 4) {
        map = mmap(NULL, 8192, PROT_READ | PROT_WRITE, MAP_SHARED, fd, ARM_GICD_BASE);
        if (map == MAP_FAILED) {
            close(fd);
            return false;
        }
        gicd = (uint32_t *)map;
        gicc = (uint32_t *)map;
        gicc += (ARM_GICC_BASE - ARM_GICD_BASE) / sizeof(uint32_t);
    } else {
        gicd = NULL;
        gicc = NULL;
    }
    close(fd);

    LOGTRACE("%s Set Drive Strength", __PRETTY_FUNCTION__);
    // Set Drive Strength to 16mA
    DrvConfig(7);

    GPIOBUS::InitializeGpio();
    // // Set pull up/pull down
    // LOGTRACE("%s Set pull up/down....", __PRETTY_FUNCTION__);
    // board_type::gpio_pull_up_down_e pullmode ;
    // if (board->signal_control_mode == 0)
    // {
    //     // #if SIGNAL_CONTROL_MODE == 0
    //     LOGTRACE("%s GPIO_PULLNONE", __PRETTY_FUNCTION__);
    //     pullmode = board_type::gpio_pull_up_down_e::GPIO_PULLNONE;
    // }
    // else if (board->signal_control_mode == 1)
    // {
    //     // #elif SIGNAL_CONTROL_MODE == 1
    //     LOGTRACE("%s GPIO_PULLUP", __PRETTY_FUNCTION__);
    //     pullmode = board_type::gpio_pull_up_down_e ::GPIO_PULLUP;
    // }
    // else
    // {
    //     // #else
    //     LOGTRACE("%s GPIO_PULLDOWN", __PRETTY_FUNCTION__);
    //     pullmode = board_type::gpio_pull_up_down_e ::GPIO_PULLDOWN;
    // }
    // // #endif

    // // Initialize all signals
    // LOGTRACE("%s Initialize all signals....", __PRETTY_FUNCTION__);

    // for (int i = 0; SignalTable[i] != board_type::pi_physical_pin_e::PI_PHYS_PIN_NONE; i++) {
    //     board_type::pi_physical_pin_e j = SignalTable[i];
    //     PinSetSignal(j, board_type::gpio_high_low_e::GPIO_STATE_LOW);
    //     PinConfig(j, board_type::gpio_direction_e::GPIO_INPUT);
    //     PullConfig(j, pullmode);
    // }

    // // Set control signals
    // LOGTRACE("%s Set control signals....", __PRETTY_FUNCTION__);
    // PinSetSignal(board->pin_act, board_type::gpio_high_low_e::GPIO_STATE_LOW);
    // PinSetSignal(board->pin_tad, board_type::gpio_high_low_e::GPIO_STATE_LOW);
    // PinSetSignal(board->pin_ind, board_type::gpio_high_low_e::GPIO_STATE_LOW);
    // PinSetSignal(board->pin_dtd, board_type::gpio_high_low_e::GPIO_STATE_LOW);
    // PinConfig(board->pin_act, board_type::gpio_direction_e::GPIO_OUTPUT);
    // PinConfig(board->pin_tad, board_type::gpio_direction_e::GPIO_OUTPUT);
    // PinConfig(board->pin_ind, board_type::gpio_direction_e::GPIO_OUTPUT);
    // PinConfig(board->pin_dtd, board_type::gpio_direction_e::GPIO_OUTPUT);

    // // Set the ENABLE signal
    // // This is used to show that the application is running
    // PinSetSignal(board->pin_enb, board->EnbOff());
    // PinConfig(board->pin_enb, board_type::gpio_direction_e::GPIO_OUTPUT);

    // GPFSEL backup
    LOGTRACE("%s GPFSEL backup....", __PRETTY_FUNCTION__);

    gpfsel[0] = gpio[GPIO_FSEL_0];
    gpfsel[1] = gpio[GPIO_FSEL_1];
    gpfsel[2] = gpio[GPIO_FSEL_2];
    gpfsel[3] = gpio[GPIO_FSEL_3];

    // Initialize SEL signal interrupt
#ifdef USE_SEL_EVENT_ENABLE
    // GPIO chip open
    LOGTRACE("%s GPIO chip open", __PRETTY_FUNCTION__);
    int gpio_fd = open("/dev/gpiochip0", 0);
    if (gpio_fd == -1) {
        LOGERROR("Unable to open /dev/gpiochip0. Is RaSCSI already running?")
        return false;
    }

    // Event request setting
    LOGTRACE("%s Event request setting (pin sel: %d)", __PRETTY_FUNCTION__, phys_to_gpio_map.at(board->pin_sel));
    strcpy(selevreq.consumer_label, "RaSCSI");
    selevreq.lineoffset  = phys_to_gpio_map.at(board->pin_sel);
    selevreq.handleflags = GPIOHANDLE_REQUEST_INPUT;
#if SIGNAL_CONTROL_MODE < 2
    selevreq.eventflags  = GPIOEVENT_REQUEST_FALLING_EDGE;
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
    memset(&ev, 0, sizeof(ev));
    ev.events  = EPOLLIN | EPOLLPRI;
    ev.data.fd = selevreq.fd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, selevreq.fd, &ev);
#else
    // Edge detection setting
    if (board->signal_control_mode == 2) {
        // #if SIGNAL_CONTROL_MODE == 2
        gpio[GPIO_AREN_0] = 1 << phys_to_gpio_map.at(board->pin_sel);
    } else {
        // #else
        gpio[GPIO_AFEN_0] = 1 << phys_to_gpio_map.at(board->pin_sel);
        // #endif // SIGNAL_CONTROL_MODE
    }
    // Clear event
    gpio[GPIO_EDS_0] = 1 << phys_to_gpio_map.at(board->pin_sel);

    // Register interrupt handler
    setIrqFuncAddress(IrqHandler);

    // GPIO interrupt setting
    if (rpitype == 4) {
        // GIC Invalid
        gicd[GICD_CTLR] = 0;

        // Route all interupts to core 0
        for (i = 0; i < 8; i++) {
            gicd[GICD_ICENABLER0 + i] = 0xffffffff;
            gicd[GICD_ICPENDR0 + i]   = 0xffffffff;
            gicd[GICD_ICACTIVER0 + i] = 0xffffffff;
        }
        for (i = 0; i < 64; i++) {
            gicd[GICD_IPRIORITYR0 + i] = 0xa0a0a0a0;
            gicd[GICD_ITARGETSR0 + i]  = 0x01010101;
        }

        // Set all interrupts as level triggers
        for (i = 0; i < 16; i++) {
            gicd[GICD_ICFGR0 + i] = 0;
        }

        // GIC Invalid
        gicd[GICD_CTLR] = 1;

        // Enable CPU interface for core 0
        gicc[GICC_PMR]  = 0xf0;
        gicc[GICC_CTLR] = 1;

        // Enable interrupts
        gicd[GICD_ISENABLER0 + (GIC_GPIO_IRQ / 32)] = 1 << (GIC_GPIO_IRQ % 32);
    } else {
        // Enable interrupts
        irpctl[IRPT_ENB_IRQ_2] = (1 << (GPIO_IRQ % 32));
    }
#endif // USE_SEL_EVENT_ENABLE

    // Create work table

    MakeTable();

    // Finally, enable ENABLE
    LOGTRACE("%s Finally, enable ENABLE....", __PRETTY_FUNCTION__);
    // Show the user that this app is running
    SetControl(board->pin_enb, board->EnbOn());

    return true;
#endif // ifdef __x86_64__ || __X86__
}

void GPIOBUS_Raspberry::Cleanup()
{
    GPIO_FUNCTION_TRACE
#if defined(__x86_64__) || defined(__X86__)
    return;
#else
    // Release SEL signal interrupt
#ifdef USE_SEL_EVENT_ENABLE
    close(selevreq.fd);
#endif // USE_SEL_EVENT_ENABLE

    // Set control signals
    PinSetSignal(board->pin_enb, board_type::gpio_high_low_e::GPIO_STATE_LOW);
    PinSetSignal(board->pin_act, board_type::gpio_high_low_e::GPIO_STATE_LOW);
    PinSetSignal(board->pin_tad, board_type::gpio_high_low_e::GPIO_STATE_LOW);
    PinSetSignal(board->pin_ind, board_type::gpio_high_low_e::GPIO_STATE_LOW);
    PinSetSignal(board->pin_dtd, board_type::gpio_high_low_e::GPIO_STATE_LOW);
    PinConfig(board->pin_act, board_type::gpio_direction_e::GPIO_INPUT);
    PinConfig(board->pin_tad, board_type::gpio_direction_e::GPIO_INPUT);
    PinConfig(board->pin_ind, board_type::gpio_direction_e::GPIO_INPUT);
    PinConfig(board->pin_dtd, board_type::gpio_direction_e::GPIO_INPUT);

    // Initialize all signals
    for (int i = 0; SignalTable[i] != board_type::pi_physical_pin_e::PI_PHYS_PIN_NONE; i++) {
        board_type::pi_physical_pin_e pin = SignalTable[i];
        PinSetSignal(pin, board_type::gpio_high_low_e::GPIO_STATE_LOW);
        PinConfig(pin, board_type::gpio_direction_e::GPIO_INPUT);
        PullConfig(pin, board_type::gpio_pull_up_down_e::GPIO_PULLNONE);
    }

    // Set drive strength back to 8mA
    DrvConfig(3);
#endif // ifdef __x86_64__ || __X86__
}

//---------------------------------------------------------------------------
//
// Get data signals
//
//---------------------------------------------------------------------------
BYTE GPIOBUS_Raspberry::GetDAT()
{
    GPIO_FUNCTION_TRACE
    uint32_t data = Acquire();
    data          = ((data >> (phys_to_gpio_map.at(board->pin_dt0) - 0)) & (1 << 0)) |
           ((data >> (phys_to_gpio_map.at(board->pin_dt1) - 1)) & (1 << 1)) |
           ((data >> (phys_to_gpio_map.at(board->pin_dt2) - 2)) & (1 << 2)) |
           ((data >> (phys_to_gpio_map.at(board->pin_dt3) - 3)) & (1 << 3)) |
           ((data >> (phys_to_gpio_map.at(board->pin_dt4) - 4)) & (1 << 4)) |
           ((data >> (phys_to_gpio_map.at(board->pin_dt5) - 5)) & (1 << 5)) |
           ((data >> (phys_to_gpio_map.at(board->pin_dt6) - 6)) & (1 << 6)) |
           ((data >> (phys_to_gpio_map.at(board->pin_dt7) - 7)) & (1 << 7));
    return (BYTE)data;
}

//---------------------------------------------------------------------------
//
//	Set data signals
//
//---------------------------------------------------------------------------
void GPIOBUS_Raspberry::SetDAT(BYTE dat)
{
    GPIO_FUNCTION_TRACE
    // Write to port
#if SIGNAL_CONTROL_MODE == 0
    uint32_t fsel = gpfsel[0];
    fsel &= tblDatMsk[0][dat];
    fsel |= tblDatSet[0][dat];
    if (fsel != gpfsel[0]) {
        gpfsel[0]         = fsel;
        gpio[GPIO_FSEL_0] = fsel;
    }

    fsel = gpfsel[1];
    fsel &= tblDatMsk[1][dat];
    fsel |= tblDatSet[1][dat];
    if (fsel != gpfsel[1]) {
        gpfsel[1]         = fsel;
        gpio[GPIO_FSEL_1] = fsel;
    }

    fsel = gpfsel[2];
    fsel &= tblDatMsk[2][dat];
    fsel |= tblDatSet[2][dat];
    if (fsel != gpfsel[2]) {
        gpfsel[2]         = fsel;
        gpio[GPIO_FSEL_2] = fsel;
    }
#else
    gpio[GPIO_CLR_0] = tblDatMsk[dat];
    gpio[GPIO_SET_0] = tblDatSet[dat];
#endif // SIGNAL_CONTROL_MODE
}

//---------------------------------------------------------------------------
//
//	Create work table
//
//---------------------------------------------------------------------------
void GPIOBUS_Raspberry::MakeTable(void)
{
    GPIO_FUNCTION_TRACE

    const array<int, 9> pintbl = {
        phys_to_gpio_map.at(board->pin_dt0), phys_to_gpio_map.at(board->pin_dt1), phys_to_gpio_map.at(board->pin_dt2),
        phys_to_gpio_map.at(board->pin_dt3), phys_to_gpio_map.at(board->pin_dt4), phys_to_gpio_map.at(board->pin_dt5),
        phys_to_gpio_map.at(board->pin_dt6), phys_to_gpio_map.at(board->pin_dt7), phys_to_gpio_map.at(board->pin_dp)};

    //  PIN_DT0, PIN_DT1, PIN_DT2, PIN_DT3, PIN_DT4, PIN_DT5, PIN_DT6, PIN_DT7, PIN_DP};

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
void GPIOBUS_Raspberry::SetControl(board_type::pi_physical_pin_e pin, board_type::gpio_high_low_e ast)
{
    LOGTRACE("%s hwpin: %d", __PRETTY_FUNCTION__, (int)pin);
    PinSetSignal(pin, ast);
}

//---------------------------------------------------------------------------
//
//	Input/output mode setting
//
//---------------------------------------------------------------------------
void GPIOBUS_Raspberry::SetMode(board_type::pi_physical_pin_e hw_pin, board_type::gpio_direction_e mode)
{
    LOGTRACE("%s hwpin: %d", __PRETTY_FUNCTION__, (int)hw_pin);
    int pin = phys_to_gpio_map.at(hw_pin);

#if SIGNAL_CONTROL_MODE == 0
    if (mode == board_type::gpio_direction_e::GPIO_OUTPUT) {
        return;
    }
#endif // SIGNAL_CONTROL_MODE

    int index     = pin / 10;
    int shift     = (pin % 10) * 3;
    uint32_t data = gpfsel[index];
    data &= ~(0x7 << shift);
    if (mode == board_type::gpio_direction_e::GPIO_OUTPUT) {
        data |= (1 << shift);
    }
    gpio[index]   = data;
    gpfsel[index] = data;
}

//---------------------------------------------------------------------------
//
//	Get input signal value
//
//---------------------------------------------------------------------------
bool GPIOBUS_Raspberry::GetSignal(board_type::pi_physical_pin_e hw_pin) const
{
    LOGTRACE("%s hwpin: %d", __PRETTY_FUNCTION__, (int)hw_pin);

    int pin = phys_to_gpio_map.at(hw_pin);
    return (signals >> pin) & 1;
}

//---------------------------------------------------------------------------
//
//	Set output signal value
//
//---------------------------------------------------------------------------
void GPIOBUS_Raspberry::SetSignal(board_type::pi_physical_pin_e hw_pin, board_type::gpio_high_low_e ast)
{
    LOGTRACE("%s hwpin: %d", __PRETTY_FUNCTION__, (int)hw_pin);

    int pin = phys_to_gpio_map.at(hw_pin);

    if (board->signal_control_mode == 0) {
        // #if SIGNAL_CONTROL_MODE == 0
        int index     = pin / 10;
        int shift     = (pin % 10) * 3;
        uint32_t data = gpfsel[index];
        if (ast == board_type::gpio_high_low_e::GPIO_STATE_HIGH) {
            data |= (1 << shift);
        } else {
            data &= ~(0x7 << shift);
        }
        gpio[index]   = data;
        gpfsel[index] = data;
    } else if (board->signal_control_mode == 1) {
        //#elif SIGNAL_CONTROL_MODE == 1
        if (ast == board_type::gpio_high_low_e::GPIO_STATE_HIGH) {
            gpio[GPIO_CLR_0] = 0x1 << pin;
        } else {
            gpio[GPIO_SET_0] = 0x1 << pin;
        }
    } else if (board->signal_control_mode == 2) {
        // #elif SIGNAL_CONTROL_MODE == 2
        if (ast == board_type::gpio_high_low_e::GPIO_STATE_HIGH) {
            gpio[GPIO_SET_0] = 0x1 << pin;
        } else {
            gpio[GPIO_CLR_0] = 0x1 << pin;
        }
    }
    // #endif // SIGNAL_CONTROL_MODE
}

//---------------------------------------------------------------------------
//
//	Wait for signal change
//
// TODO: maybe this should be moved to SCSI_Bus?
//---------------------------------------------------------------------------
bool GPIOBUS_Raspberry::WaitSignal(board_type::pi_physical_pin_e hw_pin, board_type::gpio_high_low_e ast)
{
    int pin = phys_to_gpio_map.at(hw_pin);
    // Get current time
    uint32_t now = SysTimer::GetTimerLow();

    // Calculate timeout (3000ms)
    uint32_t timeout = 3000 * 1000;

    // end immediately if the signal has changed
    do {
        // Immediately upon receiving a reset
        Acquire();
        if (GetRST()) {
            return false;
        }

        // Check for the signal edge
        if (((signals >> pin) ^ ~((int)board->gpio_state_to_bool(ast))) & 1) {
            return true;
        }
    } while ((SysTimer::GetTimerLow() - now) < timeout);

    // We timed out waiting for the signal
    return false;
}

void GPIOBUS_Raspberry::DisableIRQ()
{
    GPIO_FUNCTION_TRACE
#ifdef __linux
    if (rpitype == 4) {
        // RPI4 is disabled by GICC
        giccpmr        = gicc[GICC_PMR];
        gicc[GICC_PMR] = 0;
    } else if (rpitype == 2) {
        // RPI2,3 disable core timer IRQ
        tintcore          = sched_getcpu() + QA7_CORE0_TINTC;
        tintctl           = qa7regs[tintcore];
        qa7regs[tintcore] = 0;
    } else {
        // Stop system timer interrupt with interrupt controller
        irptenb                = irpctl[IRPT_ENB_IRQ_1];
        irpctl[IRPT_DIS_IRQ_1] = irptenb & 0xf;
    }
#else
        (void)
    0;
#endif
}

void GPIOBUS_Raspberry::EnableIRQ()
{
    GPIO_FUNCTION_TRACE
    if (rpitype == 4) {
        // RPI4 enables interrupts via the GICC
        gicc[GICC_PMR] = giccpmr;
    } else if (rpitype == 2) {
        // RPI2,3 re-enable core timer IRQ
        qa7regs[tintcore] = tintctl;
    } else {
        // Restart the system timer interrupt with the interrupt controller
        irpctl[IRPT_ENB_IRQ_1] = irptenb & 0xf;
    }
}

//---------------------------------------------------------------------------
//
//	Pin direction setting (input/output)
//
//---------------------------------------------------------------------------
void GPIOBUS_Raspberry::PinConfig(board_type::pi_physical_pin_e hw_pin, board_type::gpio_direction_e mode)
{
    LOGTRACE("%s hwpin: %d", __PRETTY_FUNCTION__, (int)hw_pin);

    int pin = phys_to_gpio_map.at(hw_pin);

    // Check for invalid pin
    if (pin < 0) {
        return;
    }

    int index     = pin / 10;
    uint32_t mask = ~(0x7 << ((pin % 10) * 3));
    gpio[index]   = (gpio[index] & mask) | (((uint8_t)mode & 0x7) << ((pin % 10) * 3));
}

//---------------------------------------------------------------------------
//
//	Pin pull-up/pull-down setting
//
//---------------------------------------------------------------------------
void GPIOBUS_Raspberry::PullConfig(board_type::pi_physical_pin_e hw_pin, board_type::gpio_pull_up_down_e mode)
{
    uint32_t pull;

    int pin = phys_to_gpio_map.at(hw_pin);

    // Check for invalid pin
    if (pin < 0) {
        return;
    }

    if (rpitype == 4) {
        LOGTRACE("%s (I'm a Pi 4) hwpin: %d gpio: %d", __PRETTY_FUNCTION__, (int)hw_pin, pin);
        switch (mode) {
        case board_type::gpio_pull_up_down_e::GPIO_PULLNONE:
            pull = 0;
            break;
        case board_type::gpio_pull_up_down_e::GPIO_PULLUP:
            pull = 1;
            break;
        case board_type::gpio_pull_up_down_e::GPIO_PULLDOWN:
            pull = 2;
            break;
        default:
            return;
        }

        pin &= 0x1f;
        int shift     = (pin & 0xf) << 1;
        uint32_t bits = gpio[GPIO_PUPPDN0 + (pin >> 4)];
        bits &= ~(3 << shift);
        bits |= (pull << shift);
        gpio[GPIO_PUPPDN0 + (pin >> 4)] = bits;
    } else {
        pin &= 0x1f;
        gpio[GPIO_PUD] = (uint32_t)mode & 0x3;
        SysTimer::SleepUsec(2);
        gpio[GPIO_CLK_0] = 0x1 << pin;
        SysTimer::SleepUsec(2);
        gpio[GPIO_PUD]   = 0;
        gpio[GPIO_CLK_0] = 0;
    }
}

//---------------------------------------------------------------------------
//
//	Set output pin
//
//---------------------------------------------------------------------------
void GPIOBUS_Raspberry::PinSetSignal(board_type::pi_physical_pin_e hw_pin, board_type::gpio_high_low_e ast)
{
    LOGTRACE("%s hwpin: %d", __PRETTY_FUNCTION__, (int)hw_pin);

    int pin = phys_to_gpio_map.at(hw_pin);

    // Check for invalid pin
    if (pin < 0) {
        return;
    }

    if (ast == board_type::gpio_high_low_e::GPIO_STATE_HIGH) {
        gpio[GPIO_SET_0] = 0x1 << pin;
    } else {
        gpio[GPIO_CLR_0] = 0x1 << pin;
    }
}

//---------------------------------------------------------------------------
//
//	Set the signal drive strength
//
//---------------------------------------------------------------------------
void GPIOBUS_Raspberry::DrvConfig(uint32_t drive)
{
    uint32_t data  = pads[PAD_0_27];
    pads[PAD_0_27] = (0xFFFFFFF8 & data) | drive | 0x5a000000;
}

uint32_t GPIOBUS_Raspberry::Acquire()
{
    GPIO_FUNCTION_TRACE;
#if defined(__x86_64__) || defined(__X86__)
    // Only used for development/debugging purposes. Isn't really applicable
    // to any real-world RaSCSI application
    return 0;
#else
    signals = *level;

    if (board->signal_control_mode < 2) {
        // #if SIGNAL_CONTROL_MODE < 2
        // Invert if negative logic (internal processing is unified to positive logic)
        signals = ~signals;
        // #endif // SIGNAL_CONTROL_MODE
    }
    return signals;
#endif // ifdef __x86_64__ || __X86__
}
