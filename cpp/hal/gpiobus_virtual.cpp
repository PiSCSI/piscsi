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

#include "hal/gpiobus_virtual.h"
#include "config.h"
#include "hal/board_type.h"
#include "hal/gpiobus.h"
#include "hal/systimer.h"
#include "log.h"
#include <map>
#include <string.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>

const std::map<board_type::pi_physical_pin_e, int> GPIOBUS_Virtual::phys_to_gpio_map = {
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

bool GPIOBUS_Virtual::Init(mode_e mode, board_type::rascsi_board_type_e rascsi_type)
{
    GPIO_FUNCTION_TRACE
    GPIOBUS::Init(mode, rascsi_type);

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
    signals = (uint32_t *)malloc(sizeof(uint32_t));
#endif

    GPIOBUS::InitializeGpio();

    return true;
}

void GPIOBUS_Virtual::Cleanup()
{
    GPIO_FUNCTION_TRACE

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
#ifdef SHARED_MEMORY_GPIO
    munmap(static_cast<void *>(signals), sizeof(uint32_t));
    shm_unlink(SHARED_MEM_NAME.c_str());
    sem_close(mutex_sem);
#else
    free(signals);
#endif
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
    data          = ((data >> (phys_to_gpio_map.at(board->pin_dt0) - 0)) & (1 << 0)) |
           ((data >> (phys_to_gpio_map.at(board->pin_dt1) - 1)) & (1 << 1)) |
           ((data >> (phys_to_gpio_map.at(board->pin_dt2) - 2)) & (1 << 2)) |
           ((data >> (phys_to_gpio_map.at(board->pin_dt3) - 3)) & (1 << 3)) |
           ((data >> (phys_to_gpio_map.at(board->pin_dt4) - 4)) & (1 << 4)) |
           ((data >> (phys_to_gpio_map.at(board->pin_dt5) - 5)) & (1 << 5)) |
           ((data >> (phys_to_gpio_map.at(board->pin_dt6) - 6)) & (1 << 6)) |
           ((data >> (phys_to_gpio_map.at(board->pin_dt7) - 7)) & (1 << 7));
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

    PinSetSignal(board->pin_dt0, board->bool_to_gpio_state((dat >> 0) & 0x1));
    PinSetSignal(board->pin_dt1, board->bool_to_gpio_state((dat >> 1) & 0x1));
    PinSetSignal(board->pin_dt2, board->bool_to_gpio_state((dat >> 2) & 0x1));
    PinSetSignal(board->pin_dt3, board->bool_to_gpio_state((dat >> 3) & 0x1));
    PinSetSignal(board->pin_dt4, board->bool_to_gpio_state((dat >> 4) & 0x1));
    PinSetSignal(board->pin_dt5, board->bool_to_gpio_state((dat >> 5) & 0x1));
    PinSetSignal(board->pin_dt6, board->bool_to_gpio_state((dat >> 6) & 0x1));
    PinSetSignal(board->pin_dt7, board->bool_to_gpio_state((dat >> 7) & 0x1));
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
void GPIOBUS_Virtual::SetControl(board_type::pi_physical_pin_e pin, board_type::gpio_high_low_e ast)
{
    LOGTRACE("%s hwpin: %d", __PRETTY_FUNCTION__, (int)pin);
    PinSetSignal(pin, ast);
}

//---------------------------------------------------------------------------
//
//	Input/output mode setting
//
//---------------------------------------------------------------------------
void GPIOBUS_Virtual::SetMode(board_type::pi_physical_pin_e hw_pin, board_type::gpio_direction_e mode)
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
bool GPIOBUS_Virtual::GetSignal(board_type::pi_physical_pin_e hw_pin) const
{
    GPIO_FUNCTION_TRACE
    int pin               = phys_to_gpio_map.at(hw_pin);
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
    return (signal_value >> pin) & 1;
}

//---------------------------------------------------------------------------
//
//	Set output signal value
//
//---------------------------------------------------------------------------
void GPIOBUS_Virtual::SetSignal(board_type::pi_physical_pin_e hw_pin, board_type::gpio_high_low_e ast)
{
    GPIO_FUNCTION_TRACE
    PinSetSignal(hw_pin, ast);
}

//---------------------------------------------------------------------------
//
//	Wait for signal change
//
// TODO: maybe this should be moved to SCSI_Bus?
//---------------------------------------------------------------------------
bool GPIOBUS_Virtual::WaitSignal(board_type::pi_physical_pin_e hw_pin, board_type::gpio_high_low_e ast)
{
    // Get current time
    uint32_t now = SysTimer::GetTimerLow();

    // Calculate timeout (3000ms)
    uint32_t timeout = 3000 * 1000;

    // end immediately if the signal has changed
    do {
        // Immediately upon receiving a reset
        (void)Acquire();
        if (GetRST()) {
            return false;
        }

        // Check for the signal edge
        if (GetSignal(hw_pin) == board->gpio_state_to_bool(ast)) {
            return true;
        }
    } while ((SysTimer::GetTimerLow() - now) < timeout);

    // We timed out waiting for the signal
    return false;
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
//	Pin direction setting (input/output)
//
//---------------------------------------------------------------------------
void GPIOBUS_Virtual::PinConfig(board_type::pi_physical_pin_e hw_pin, board_type::gpio_direction_e mode)
{
    GPIO_FUNCTION_TRACE(void) hw_pin;
    (void)mode;
    // Nothing to do to configure pins for virtual gpio bus
}

//---------------------------------------------------------------------------
//
//	Pin pull-up/pull-down setting
//
//---------------------------------------------------------------------------
void GPIOBUS_Virtual::PullConfig(board_type::pi_physical_pin_e hw_pin, board_type::gpio_pull_up_down_e mode)
{
    GPIO_FUNCTION_TRACE(void) hw_pin;
    (void)mode;
    // Nothing to do to configure pins for virtual gpio bus
}

//---------------------------------------------------------------------------
//
//	Set output pin
//
//---------------------------------------------------------------------------
void GPIOBUS_Virtual::PinSetSignal(board_type::pi_physical_pin_e hw_pin, board_type::gpio_high_low_e ast)
{
    LOGTRACE("%s hwpin: %d", __PRETTY_FUNCTION__, (int)hw_pin);

    int gpio_bit = phys_to_gpio_map.at(hw_pin);

    // Check for invalid pin
    if (gpio_bit < 0) {
        return;
    }
#ifdef SHARED_MEMORY_GPIO
    if (sem_wait(mutex_sem) == -1) {
        LOGERROR("Unable to lock the shared memory")
        return;
    }
#endif
    if (ast == board_type::gpio_high_low_e::GPIO_STATE_HIGH) {
        // Set the "gpio" bit
        *signals |= 0x1 << gpio_bit;
    } else {
        // Clear the "gpio" bit
        *signals ^= ~(0x1 << gpio_bit);
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
