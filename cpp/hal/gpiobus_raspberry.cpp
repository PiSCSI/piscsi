//---------------------------------------------------------------------------
//
//	SCSI Target Emulator PiSCSI
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//
//	[ GPIO-SCSI bus ]
//
//  Raspberry Pi 4:
//     https://datasheets.raspberrypi.com/bcm2711/bcm2711-peripherals.pdf
//  Raspberry Pi Zero:
//     https://datasheets.raspberrypi.com/bcm2835/bcm2835-peripherals.pdf
//
//---------------------------------------------------------------------------

#include <spdlog/spdlog.h>
#include "hal/gpiobus_raspberry.h"
#include "hal/gpiobus.h"
#include "hal/systimer.h"
#include <map>
#include <cstring>
#ifdef __linux__
#include <sys/epoll.h>
#endif
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>

//---------------------------------------------------------------------------
//
//	imported from bcm_host.c
//
//---------------------------------------------------------------------------
uint32_t GPIOBUS_Raspberry::get_dt_ranges(const char *filename, uint32_t offset)
{
    GPIO_FUNCTION_TRACE
    uint32_t address = ~0;
    if (FILE *fp = fopen(filename, "rb"); fp) {
        fseek(fp, offset, SEEK_SET);
        if (array<uint8_t, 4> buf; fread(buf.data(), 1, buf.size(), fp) == buf.size()) {
            address = (int)buf[0] << 24 | (int)buf[1] << 16 | (int)buf[2] << 8 | (int)buf[3] << 0;
        }
        fclose(fp);
    }
    return address;
}

uint32_t GPIOBUS_Raspberry::bcm_host_get_peripheral_address()
{
    GPIO_FUNCTION_TRACE
#ifdef __linux__
    uint32_t address = get_dt_ranges("/proc/device-tree/soc/ranges", 4);
    if (address == 0) {
        address = get_dt_ranges("/proc/device-tree/soc/ranges", 8);
    }
    address = (address == (uint32_t)~0) ? 0x20000000 : address;
    return address;
#else
    return 0;
#endif
}

bool GPIOBUS_Raspberry::Init(mode_e mode)
{
    GPIOBUS::Init(mode);

#if defined(__x86_64__) || defined(__X86__)
    (void)baseaddr;
    level = new uint32_t();
    return true;
#else
    int i;
#ifdef USE_SEL_EVENT_ENABLE
    epoll_event ev = {};
#endif

    // Get the base address
    baseaddr = (uint32_t)bcm_host_get_peripheral_address();

    // Open /dev/mem
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd == -1) {
        spdlog::error("Error: Unable to open /dev/mem. Are you running as root?");
        return false;
    }

    // Map peripheral region memory
    void *map = mmap(NULL, 0x1000100, PROT_READ | PROT_WRITE, MAP_SHARED, fd, baseaddr);
    if (map == MAP_FAILED) {
        spdlog::error("Error: Unable to map memory: "+ string(strerror(errno)));
        close(fd);
        return false;
    }

    // Determine the type of raspberry pi from the base address
    if (baseaddr == 0xfe000000) {
        rpitype = 4;
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

    // System timer
    SysTimer::Init();

    // Interrupt controller
    irpctl = (uint32_t *)map;
    irpctl += IRPT_OFFSET / sizeof(uint32_t);

    // Quad-A7 control
    qa7regs = (uint32_t *)map;
    qa7regs += QA7_OFFSET / sizeof(uint32_t);

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

    // Set Drive Strength to 16mA
    DrvConfig(7);

    // Set pull up/pull down
#if SIGNAL_CONTROL_MODE == 0
    int pullmode = GPIO_PULLNONE;
#elif SIGNAL_CONTROL_MODE == 1
    int pullmode = GPIO_PULLUP;
#else
    int pullmode = GPIO_PULLDOWN;
#endif

    // Initialize all signals
    for (i = 0; SignalTable[i] >= 0; i++) {
        int j = SignalTable[i];
        PinSetSignal(j, OFF);
        PinConfig(j, GPIO_INPUT);
        PullConfig(j, pullmode);
    }

    // Set control signals
    PinSetSignal(PIN_ACT, OFF);
    PinSetSignal(PIN_TAD, OFF);
    PinSetSignal(PIN_IND, OFF);
    PinSetSignal(PIN_DTD, OFF);
    PinConfig(PIN_ACT, GPIO_OUTPUT);
    PinConfig(PIN_TAD, GPIO_OUTPUT);
    PinConfig(PIN_IND, GPIO_OUTPUT);
    PinConfig(PIN_DTD, GPIO_OUTPUT);

    // Set the ENABLE signal
    // This is used to show that the application is running
    PinSetSignal(PIN_ENB, ENB_OFF);
    PinConfig(PIN_ENB, GPIO_OUTPUT);

    // GPIO Function Select (GPFSEL) registers backup
    gpfsel[0] = gpio[GPIO_FSEL_0];
    gpfsel[1] = gpio[GPIO_FSEL_1];
    gpfsel[2] = gpio[GPIO_FSEL_2];
    gpfsel[3] = gpio[GPIO_FSEL_3];

    // Initialize SEL signal interrupt
#ifdef USE_SEL_EVENT_ENABLE
    // GPIO chip open
    fd = open("/dev/gpiochip0", 0);
    if (fd == -1) {
        spdlog::error("Unable to open /dev/gpiochip0. If PiSCSI is running, please shut it down first.");
        return false;
    }

    // Event request setting
    strcpy(selevreq.consumer_label, "PiSCSI");
    selevreq.lineoffset  = PIN_SEL;
    selevreq.handleflags = GPIOHANDLE_REQUEST_INPUT;
#if SIGNAL_CONTROL_MODE < 2
    selevreq.eventflags  = GPIOEVENT_REQUEST_FALLING_EDGE;
#else
    selevreq.eventflags = GPIOEVENT_REQUEST_RISING_EDGE;
#endif // SIGNAL_CONTROL_MODE

    // Get event request
    if (ioctl(fd, GPIO_GET_LINEEVENT_IOCTL, &selevreq) == -1) {
        spdlog::error("Unable to register event request. If PiSCSI is running, please shut it down first.");
        close(fd);
        return false;
    }

    // Close GPIO chip file handle
    close(fd);

    // epoll initialization
    epfd       = epoll_create(1);
    ev.events  = EPOLLIN | EPOLLPRI;
    ev.data.fd = selevreq.fd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, selevreq.fd, &ev);
#else
    // Edge detection setting
#if SIGNAL_CONTROL_MODE == 2
    gpio[GPIO_AREN_0] = 1 << PIN_SEL;
#else
    gpio[GPIO_AFEN_0] = 1 << PIN_SEL;
#endif // SIGNAL_CONTROL_MODE

    // Clear event - GPIO Pin Event Detect Status
    gpio[GPIO_EDS_0] = 1 << PIN_SEL;

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
    // Show the user that this app is running
    SetControl(PIN_ENB, ENB_ON);

    return true;
#endif // ifdef __x86_64__ || __X86__
}

void GPIOBUS_Raspberry::Cleanup()
{
#if defined(__x86_64__) || defined(__X86__)
    return;
#else
    // Release SEL signal interrupt
#ifdef USE_SEL_EVENT_ENABLE
    close(selevreq.fd);
#endif // USE_SEL_EVENT_ENABLE

    // Set control signals
    PinSetSignal(PIN_ENB, OFF);
    PinSetSignal(PIN_ACT, OFF);
    PinSetSignal(PIN_TAD, OFF);
    PinSetSignal(PIN_IND, OFF);
    PinSetSignal(PIN_DTD, OFF);
    PinConfig(PIN_ACT, GPIO_INPUT);
    PinConfig(PIN_TAD, GPIO_INPUT);
    PinConfig(PIN_IND, GPIO_INPUT);
    PinConfig(PIN_DTD, GPIO_INPUT);

    // Initialize all signals
    for (int i = 0; SignalTable[i] >= 0; i++) {
        int pin = SignalTable[i];
        PinSetSignal(pin, OFF);
        PinConfig(pin, GPIO_INPUT);
        PullConfig(pin, GPIO_PULLNONE);
    }

    // Set drive strength back to 8mA
    DrvConfig(3);
#endif // ifdef __x86_64__ || __X86__
}

void GPIOBUS_Raspberry::Reset()
{
#if defined(__x86_64__) || defined(__X86__)
    return;
#else
    int i;
    int j;

    // Turn off active signal
    SetControl(PIN_ACT, ACT_OFF);

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
        SetControl(PIN_TAD, TAD_IN);
        SetMode(PIN_BSY, IN);
        SetMode(PIN_MSG, IN);
        SetMode(PIN_CD, IN);
        SetMode(PIN_REQ, IN);
        SetMode(PIN_IO, IN);

        // Set the initiator signal to input
        SetControl(PIN_IND, IND_IN);
        SetMode(PIN_SEL, IN);
        SetMode(PIN_ATN, IN);
        SetMode(PIN_ACK, IN);
        SetMode(PIN_RST, IN);

        // Set data bus signals to input
        SetControl(PIN_DTD, DTD_IN);
        SetMode(PIN_DT0, IN);
        SetMode(PIN_DT1, IN);
        SetMode(PIN_DT2, IN);
        SetMode(PIN_DT3, IN);
        SetMode(PIN_DT4, IN);
        SetMode(PIN_DT5, IN);
        SetMode(PIN_DT6, IN);
        SetMode(PIN_DT7, IN);
        SetMode(PIN_DP, IN);
    } else {
        // Initiator mode

        // Set target signal to input
        SetControl(PIN_TAD, TAD_IN);
        SetMode(PIN_BSY, IN);
        SetMode(PIN_MSG, IN);
        SetMode(PIN_CD, IN);
        SetMode(PIN_REQ, IN);
        SetMode(PIN_IO, IN);

        // Set the initiator signal to output
        SetControl(PIN_IND, IND_OUT);
        SetMode(PIN_SEL, OUT);
        SetMode(PIN_ATN, OUT);
        SetMode(PIN_ACK, OUT);
        SetMode(PIN_RST, OUT);

        // Set the data bus signals to output
        SetControl(PIN_DTD, DTD_OUT);
        SetMode(PIN_DT0, OUT);
        SetMode(PIN_DT1, OUT);
        SetMode(PIN_DT2, OUT);
        SetMode(PIN_DT3, OUT);
        SetMode(PIN_DT4, OUT);
        SetMode(PIN_DT5, OUT);
        SetMode(PIN_DT6, OUT);
        SetMode(PIN_DT7, OUT);
        SetMode(PIN_DP, OUT);
    }

    // Initialize all signals
    signals          = 0;
#endif // ifdef __x86_64__ || __X86__
}

void GPIOBUS_Raspberry::SetENB(bool ast)
{
    PinSetSignal(PIN_ENB, ast ? ENB_ON : ENB_OFF);
}

bool GPIOBUS_Raspberry::GetBSY() const
{
    return GetSignal(PIN_BSY);
}

void GPIOBUS_Raspberry::SetBSY(bool ast)
{
    // Set BSY signal
    SetSignal(PIN_BSY, ast);

    if (ast) {
        // Turn on ACTIVE signal
        SetControl(PIN_ACT, ACT_ON);

        // Set Target signal to output
        SetControl(PIN_TAD, TAD_OUT);

    	SetMode(PIN_BSY, OUT);
    	SetMode(PIN_MSG, OUT);
    	SetMode(PIN_CD, OUT);
    	SetMode(PIN_REQ, OUT);
    	SetMode(PIN_IO, OUT);
    } else {
        // Turn off the ACTIVE signal
        SetControl(PIN_ACT, ACT_OFF);

        // Set the target signal to input
    	SetControl(PIN_TAD, TAD_IN);

    	SetMode(PIN_BSY, IN);
    	SetMode(PIN_MSG, IN);
    	SetMode(PIN_CD, IN);
    	SetMode(PIN_REQ, IN);
    	SetMode(PIN_IO, IN);
    }
}

bool GPIOBUS_Raspberry::GetSEL() const
{
    return GetSignal(PIN_SEL);
}

void GPIOBUS_Raspberry::SetSEL(bool ast)
{
    if (actmode == mode_e::INITIATOR && ast) {
        // Turn on ACTIVE signal
        SetControl(PIN_ACT, ACT_ON);
    }

    // Set SEL signal
    SetSignal(PIN_SEL, ast);
}

bool GPIOBUS_Raspberry::GetATN() const
{
    return GetSignal(PIN_ATN);
}

void GPIOBUS_Raspberry::SetATN(bool ast)
{
    SetSignal(PIN_ATN, ast);
}

bool GPIOBUS_Raspberry::GetACK() const
{
    return GetSignal(PIN_ACK);
}

void GPIOBUS_Raspberry::SetACK(bool ast)
{
    SetSignal(PIN_ACK, ast);
}

bool GPIOBUS_Raspberry::GetACT() const
{
    return GetSignal(PIN_ACT);
}

void GPIOBUS_Raspberry::SetACT(bool ast)
{
    SetSignal(PIN_ACT, ast);
}

bool GPIOBUS_Raspberry::GetRST() const
{
    return GetSignal(PIN_RST);
}

void GPIOBUS_Raspberry::SetRST(bool ast)
{
    SetSignal(PIN_RST, ast);
}

bool GPIOBUS_Raspberry::GetMSG() const
{
    return GetSignal(PIN_MSG);
}

void GPIOBUS_Raspberry::SetMSG(bool ast)
{
    SetSignal(PIN_MSG, ast);
}

bool GPIOBUS_Raspberry::GetCD() const
{
    return GetSignal(PIN_CD);
}

void GPIOBUS_Raspberry::SetCD(bool ast)
{
    SetSignal(PIN_CD, ast);
}

bool GPIOBUS_Raspberry::GetIO()
{
    bool ast = GetSignal(PIN_IO);

    if (actmode == mode_e::INITIATOR) {
        // Change the data input/output direction by IO signal
        if (ast) {
            SetControl(PIN_DTD, DTD_IN);
            SetMode(PIN_DT0, IN);
            SetMode(PIN_DT1, IN);
            SetMode(PIN_DT2, IN);
            SetMode(PIN_DT3, IN);
            SetMode(PIN_DT4, IN);
            SetMode(PIN_DT5, IN);
            SetMode(PIN_DT6, IN);
            SetMode(PIN_DT7, IN);
            SetMode(PIN_DP, IN);
        } else {
            SetControl(PIN_DTD, DTD_OUT);
            SetMode(PIN_DT0, OUT);
            SetMode(PIN_DT1, OUT);
            SetMode(PIN_DT2, OUT);
            SetMode(PIN_DT3, OUT);
            SetMode(PIN_DT4, OUT);
            SetMode(PIN_DT5, OUT);
            SetMode(PIN_DT6, OUT);
            SetMode(PIN_DT7, OUT);
            SetMode(PIN_DP, OUT);
        }
    }

    return ast;
}

void GPIOBUS_Raspberry::SetIO(bool ast)
{
    SetSignal(PIN_IO, ast);

    if (actmode == mode_e::TARGET) {
        // Change the data input/output direction by IO signal
        if (ast) {
            SetControl(PIN_DTD, DTD_OUT);
            SetDAT(0);
            SetMode(PIN_DT0, OUT);
            SetMode(PIN_DT1, OUT);
            SetMode(PIN_DT2, OUT);
            SetMode(PIN_DT3, OUT);
            SetMode(PIN_DT4, OUT);
            SetMode(PIN_DT5, OUT);
            SetMode(PIN_DT6, OUT);
            SetMode(PIN_DT7, OUT);
            SetMode(PIN_DP, OUT);
        } else {
            SetControl(PIN_DTD, DTD_IN);
            SetMode(PIN_DT0, IN);
            SetMode(PIN_DT1, IN);
            SetMode(PIN_DT2, IN);
            SetMode(PIN_DT3, IN);
            SetMode(PIN_DT4, IN);
            SetMode(PIN_DT5, IN);
            SetMode(PIN_DT6, IN);
            SetMode(PIN_DT7, IN);
            SetMode(PIN_DP, IN);
        }
    }
}

bool GPIOBUS_Raspberry::GetREQ() const
{
    return GetSignal(PIN_REQ);
}

void GPIOBUS_Raspberry::SetREQ(bool ast)
{
    SetSignal(PIN_REQ, ast);
}

//---------------------------------------------------------------------------
//
// Get data signals
//
//---------------------------------------------------------------------------
uint8_t GPIOBUS_Raspberry::GetDAT()
{
    uint32_t data = Acquire();
    data          = ((data >> (PIN_DT0 - 0)) & (1 << 0)) | ((data >> (PIN_DT1 - 1)) & (1 << 1)) |
           ((data >> (PIN_DT2 - 2)) & (1 << 2)) | ((data >> (PIN_DT3 - 3)) & (1 << 3)) |
           ((data >> (PIN_DT4 - 4)) & (1 << 4)) | ((data >> (PIN_DT5 - 5)) & (1 << 5)) |
           ((data >> (PIN_DT6 - 6)) & (1 << 6)) | ((data >> (PIN_DT7 - 7)) & (1 << 7));

    return (uint8_t)data;
}

//---------------------------------------------------------------------------
//
//	Set data signals
//
//---------------------------------------------------------------------------
void GPIOBUS_Raspberry::SetDAT(uint8_t dat)
{
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
//	Signal table
//
//---------------------------------------------------------------------------
const array<int, 19> GPIOBUS_Raspberry::SignalTable = {PIN_DT0, PIN_DT1, PIN_DT2, PIN_DT3, PIN_DT4, PIN_DT5, PIN_DT6,
                                                       PIN_DT7, PIN_DP,  PIN_SEL, PIN_ATN, PIN_RST, PIN_ACK, PIN_BSY,
                                                       PIN_MSG, PIN_CD,  PIN_IO,  PIN_REQ, -1};

//---------------------------------------------------------------------------
//
//	Create work table
//
//---------------------------------------------------------------------------
void GPIOBUS_Raspberry::MakeTable(void)
{
    const array<int, 9> pintbl = {PIN_DT0, PIN_DT1, PIN_DT2, PIN_DT3, PIN_DT4, PIN_DT5, PIN_DT6, PIN_DT7, PIN_DP};

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
void GPIOBUS_Raspberry::SetControl(int pin, bool ast)
{
    PinSetSignal(pin, ast);
}

//---------------------------------------------------------------------------
//
//	Input/output mode setting
//
// Set direction fo pin (IN / OUT)
//   Used with: TAD, BSY, MSG, CD, REQ, O, SEL, IND, ATN, ACK, RST, DT*
//
//---------------------------------------------------------------------------
void GPIOBUS_Raspberry::SetMode(int pin, int mode)
{
#if SIGNAL_CONTROL_MODE == 0
    if (mode == OUT) {
        return;
    }
#endif // SIGNAL_CONTROL_MODE

    int index     = pin / 10;
    int shift     = (pin % 10) * 3;
    uint32_t data = gpfsel[index];
    data &= ~(0x7 << shift);
    if (mode == OUT) {
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
bool GPIOBUS_Raspberry::GetSignal(int pin) const
{
    return (signals >> pin) & 1;
}

//---------------------------------------------------------------------------
//
//	Set output signal value
//
//  Sets the output value. Used with:
//     PIN_ENB, ACT, TAD, IND, DTD, BSY, SignalTable
//
//---------------------------------------------------------------------------
void GPIOBUS_Raspberry::SetSignal(int pin, bool ast)
{
#if SIGNAL_CONTROL_MODE == 0
    int index     = pin / 10;
    int shift     = (pin % 10) * 3;
    uint32_t data = gpfsel[index];
    if (ast) {
        data |= (1 << shift);
    } else {
        data &= ~(0x7 << shift);
    }
    gpio[index]   = data;
    gpfsel[index] = data;
#elif SIGNAL_CONTROL_MODE == 1
    if (ast) {
        gpio[GPIO_CLR_0] = 0x1 << pin;
    } else {
        gpio[GPIO_SET_0] = 0x1 << pin;
    }
#elif SIGNAL_CONTROL_MODE == 2
    if (ast) {
        gpio[GPIO_SET_0] = 0x1 << pin;
    } else {
        gpio[GPIO_CLR_0] = 0x1 << pin;
    }
#endif // SIGNAL_CONTROL_MODE
}

void GPIOBUS_Raspberry::DisableIRQ()
{
#ifdef __linux__
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
    (void)0;
#endif
}

void GPIOBUS_Raspberry::EnableIRQ()
{
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
// Used in Init() for ACT, TAD, IND, DTD, ENB to set direction (GPIO_OUTPUT vs GPIO_INPUT)
// Also used on SignalTable
// Only used in Init and Cleanup. Reset uses SetMode
//---------------------------------------------------------------------------
void GPIOBUS_Raspberry::PinConfig(int pin, int mode)
{
    // Check for invalid pin
    if (pin < 0) {
        return;
    }

    int index     = pin / 10;
    uint32_t mask = ~(0x7 << ((pin % 10) * 3));
    gpio[index]   = (gpio[index] & mask) | ((mode & 0x7) << ((pin % 10) * 3));
}

//---------------------------------------------------------------------------
//
//	Pin pull-up/pull-down setting
//
//---------------------------------------------------------------------------
void GPIOBUS_Raspberry::PullConfig(int pin, int mode)
{
    uint32_t pull;

    // Check for invalid pin
    if (pin < 0) {
        return;
    }

    if (rpitype == 4) {
        switch (mode) {
        case GPIO_PULLNONE:
            pull = 0;
            break;
        case GPIO_PULLUP:
            pull = 1;
            break;
        case GPIO_PULLDOWN:
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
        gpio[GPIO_PUD] = mode & 0x3;
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
void GPIOBUS_Raspberry::PinSetSignal(int pin, bool ast)
{
    // Check for invalid pin
    if (pin < 0) {
        return;
    }

    if (ast) {
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

//---------------------------------------------------------------------------
//
//	Bus signal acquisition
//
//---------------------------------------------------------------------------
uint32_t GPIOBUS_Raspberry::Acquire()
{
    signals = *level;

#if SIGNAL_CONTROL_MODE < 2
    // Invert if negative logic (internal processing is unified to positive logic)
    signals = ~signals;
#endif // SIGNAL_CONTROL_MODE

    return signals;
}
