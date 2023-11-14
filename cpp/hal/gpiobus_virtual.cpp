//---------------------------------------------------------------------------
//
//	SCSI Target Emulator PiSCSI
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//
//---------------------------------------------------------------------------

#include "hal/gpiobus_virtual.h"
#include "hal/gpiobus.h"
#include "hal/systimer.h"
#include "hal/log.h"
#include <cstddef>
#include <map>
#include <memory>
#include <string.h>
#ifdef __linux__
#include <sys/epoll.h>
#endif
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>

using namespace std;

bool GPIOBUS_Virtual::Init(mode_e mode)
{
    GPIO_FUNCTION_TRACE
    GPIOBUS::Init(mode);

#ifdef SHARED_MEMORY_GPIO
    // Create a shared memory region that can be accessed as a virtual "SCSI bus"
    //  mutual exclusion semaphore, mutex_sem with an initial value 0.
    if ((mutex_sem = sem_open(SHARED_MEM_MUTEX_NAME.c_str(), O_CREAT, 0660, 0)) == SEM_FAILED) {
        LOGERROR("Unable to open shared memory semaphore %s. Are you running as root?", SHARED_MEM_MUTEX_NAME.c_str());
    }
    // Get shared memory
    if ((fd_shm = shm_open(SHARED_MEM_NAME.c_str(), O_RDWR | O_CREAT | O_EXCL, 0660)) == -1) {
        LOGERROR("Unable to open shared memory %s. Are you running as root?", SHARED_MEM_NAME.c_str());
        sem_close(mutex_sem);
    }
    if (ftruncate(fd_shm, sizeof(uint32_t)) == -1) {
        LOGERROR("Unable to read shared memory");
        sem_close(mutex_sem);
        shm_unlink(SHARED_MEM_NAME.c_str());
        return false;
    }

    signals = static_cast<uint32_t *>(mmap(NULL, sizeof(uint32_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd_shm, 0));
    if (static_cast<void *>(signals) == MAP_FAILED) {
        LOGERROR("Unabled to map shared memory");
        sem_close(mutex_sem);
        shm_unlink(SHARED_MEM_NAME.c_str());
    }
#else
    signals = make_shared<uint32_t>(0);
#endif

    return true;
}

void GPIOBUS_Virtual::Cleanup()
{
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

#ifdef SHARED_MEMORY_GPIO
    munmap(static_cast<void *>(signals), sizeof(uint32_t));
    shm_unlink(SHARED_MEM_NAME.c_str());
    sem_close(mutex_sem);
#endif
}

void GPIOBUS_Virtual::Reset()
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
    signals = 0;
#endif // ifdef __x86_64__ || __X86__
}

void GPIOBUS_Virtual::SetENB(bool ast)
{
    PinSetSignal(PIN_ENB, ast ? ENB_ON : ENB_OFF);
}

bool GPIOBUS_Virtual::GetBSY() const
{
    return GetSignal(PIN_BSY);
}

void GPIOBUS_Virtual::SetBSY(bool ast)
{
    // Set BSY signal
    SetSignal(PIN_BSY, ast);

    if (actmode == mode_e::TARGET) {
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
}

bool GPIOBUS_Virtual::GetSEL() const
{
    return GetSignal(PIN_SEL);
}

void GPIOBUS_Virtual::SetSEL(bool ast)
{
    if (actmode == mode_e::INITIATOR && ast) {
        // Turn on ACTIVE signal
        SetControl(PIN_ACT, ACT_ON);
    }

    // Set SEL signal
    SetSignal(PIN_SEL, ast);
}

bool GPIOBUS_Virtual::GetATN() const
{
    return GetSignal(PIN_ATN);
}

void GPIOBUS_Virtual::SetATN(bool ast)
{
    SetSignal(PIN_ATN, ast);
}

bool GPIOBUS_Virtual::GetACK() const
{
    return GetSignal(PIN_ACK);
}

void GPIOBUS_Virtual::SetACK(bool ast)
{
    SetSignal(PIN_ACK, ast);
}

bool GPIOBUS_Virtual::GetACT() const
{
    return GetSignal(PIN_ACT);
}

void GPIOBUS_Virtual::SetACT(bool ast)
{
    SetSignal(PIN_ACT, ast);
}

bool GPIOBUS_Virtual::GetRST() const
{
    return GetSignal(PIN_RST);
}

void GPIOBUS_Virtual::SetRST(bool ast)
{
    SetSignal(PIN_RST, ast);
}

bool GPIOBUS_Virtual::GetMSG() const
{
    return GetSignal(PIN_MSG);
}

void GPIOBUS_Virtual::SetMSG(bool ast)
{
    SetSignal(PIN_MSG, ast);
}

bool GPIOBUS_Virtual::GetCD() const
{
    return GetSignal(PIN_CD);
}

void GPIOBUS_Virtual::SetCD(bool ast)
{
    SetSignal(PIN_CD, ast);
}

bool GPIOBUS_Virtual::GetIO()
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

void GPIOBUS_Virtual::SetIO(bool ast)
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

bool GPIOBUS_Virtual::GetREQ() const
{
    return GetSignal(PIN_REQ);
}

void GPIOBUS_Virtual::SetREQ(bool ast)
{
    SetSignal(PIN_REQ, ast);
}

//---------------------------------------------------------------------------
//
// Get data signals
//
//---------------------------------------------------------------------------
uint8_t GPIOBUS_Virtual::GetDAT()
{
    GPIO_FUNCTION_TRACE
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
void GPIOBUS_Virtual::SetDAT(uint8_t dat)
{
    GPIO_FUNCTION_TRACE

    auto new_dat = static_cast<byte>(dat);

    PinSetSignal(PIN_DT0, (new_dat & ((byte)1 << 0)) != (byte)0);
    PinSetSignal(PIN_DT1, (new_dat & ((byte)1 << 1)) != (byte)0);
    PinSetSignal(PIN_DT2, (new_dat & ((byte)1 << 2)) != (byte)0);
    PinSetSignal(PIN_DT3, (new_dat & ((byte)1 << 3)) != (byte)0);
    PinSetSignal(PIN_DT4, (new_dat & ((byte)1 << 4)) != (byte)0);
    PinSetSignal(PIN_DT5, (new_dat & ((byte)1 << 5)) != (byte)0);
    PinSetSignal(PIN_DT6, (new_dat & ((byte)1 << 6)) != (byte)0);
    PinSetSignal(PIN_DT7, (new_dat & ((byte)1 << 7)) != (byte)0);
}

//---------------------------------------------------------------------------
//
//	Create work table
//
//---------------------------------------------------------------------------
void GPIOBUS_Virtual::MakeTable(void)
{
    GPIO_FUNCTION_TRACE
}

//---------------------------------------------------------------------------
//
//	Control signal setting
//
//---------------------------------------------------------------------------
void GPIOBUS_Virtual::SetControl(int pin, bool ast)
{
    LOGTRACE("%s hwpin: %d", __PRETTY_FUNCTION__, (int)pin)
    PinSetSignal(pin, ast);
}

//---------------------------------------------------------------------------
//
//	Input/output mode setting
//
//---------------------------------------------------------------------------
void GPIOBUS_Virtual::SetMode(int hw_pin, int mode)
{
    // Doesn't do anything for virtual gpio bus
    (void)hw_pin;
    (void)mode;
}

//---------------------------------------------------------------------------
//
//	Get input signal value
//
//---------------------------------------------------------------------------
bool GPIOBUS_Virtual::GetSignal(int hw_pin) const
{
    GPIO_FUNCTION_TRACE

    uint32_t signal_value = 0;
#ifdef SHARED_MEMORY_GPIO
    if (sem_wait(mutex_sem) == -1) {
        LOGERROR("Unable to lock the shared memory")
        return false;
    }
#endif
    signal_value = *signals;
#ifdef SHARED_MEMORY_GPIO
    if (sem_post(mutex_sem) == -1) {
        LOGERROR("Unable to release the shared memory")
        return false;
    }
#endif
    return (signal_value >> hw_pin) & 1;
}

//---------------------------------------------------------------------------
//
//	Set output signal value
//
//---------------------------------------------------------------------------
void GPIOBUS_Virtual::SetSignal(int hw_pin, bool ast)
{
    GPIO_FUNCTION_TRACE
    PinSetSignal(hw_pin, ast);
}

void GPIOBUS_Virtual::DisableIRQ()
{
    GPIO_FUNCTION_TRACE
    // Nothing to do for virtual gpio bus
}

void GPIOBUS_Virtual::EnableIRQ()
{
    GPIO_FUNCTION_TRACE
    // Nothing to do for virtual gpio bus
}

//---------------------------------------------------------------------------
//
//	Set output pin
//
//---------------------------------------------------------------------------
void GPIOBUS_Virtual::PinSetSignal(int hw_pin, bool ast)
{
    LOGTRACE("%s hwpin: %d", __PRETTY_FUNCTION__, (int)hw_pin)

    // Check for invalid pin
    if (hw_pin < 0) {
        return;
    }
#ifdef SHARED_MEMORY_GPIO
    if (sem_wait(mutex_sem) == -1) {
        LOGERROR("Unable to lock the shared memory")
        return;
    }
#endif
    if (ast) {
        // Set the "gpio" bit
        *signals |= 0x1 << hw_pin;
    } else {
        // Clear the "gpio" bit
        *signals ^= ~(0x1 << hw_pin);
    }
#ifdef SHARED_MEMORY_GPIO
    if (sem_post(mutex_sem) == -1) {
        LOGERROR("Unable to release the shared memory")
        return;
    }
#endif
}

//---------------------------------------------------------------------------
//
//	Set the signal drive strength
//
//---------------------------------------------------------------------------
void GPIOBUS_Virtual::DrvConfig(uint32_t drive)
{
    (void)drive;
    // Nothing to do for virtual GPIO
}

uint32_t GPIOBUS_Virtual::Acquire()
{
    GPIO_FUNCTION_TRACE;

    uint32_t signal_value = 0;
#ifdef SHARED_MEMORY_GPIO
    if (sem_wait(mutex_sem) == -1) {
        LOGERROR("Unable to lock the shared memory")
        return false;
    }
#endif
    signal_value = *signals;
#ifdef SHARED_MEMORY_GPIO
    if (sem_post(mutex_sem) == -1) {
        LOGERROR("Unable to release the shared memory")
        return false;
    }
#endif
    return signal_value;
}

