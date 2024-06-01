// //---------------------------------------------------------------------------
// //
// //	SCSI Target Emulator PiSCSI
// //	for Raspberry Pi
// //
// //	Powered by XM6 TypeG Technology.
// //  Copyright (C) 2022-2024 akuker
// //	Copyright (C) 2016-2020 GIMONS
// //
// //	[ GPIO-SCSI bus for SBCs using the Raspberry Pi RP1 chip ]
// //
// //---------------------------------------------------------------------------

// #pragma once

// #include "hal/data_sample_raspberry.h"
// #include "hal/gpiobus.h"
// #include "shared/scsi.h"
// #include <map>

// // //---------------------------------------------------------------------------
// // //
// // //	SCSI signal pin assignment setting
// // //	  GPIO pin mapping table for SCSI signals.
// // //	  PIN_DT0～PIN_SEL
// // //
// // //---------------------------------------------------------------------------

// // #define ALL_SCSI_PINS
// //     ((1 << PIN_DT0) | (1 << PIN_DT1) | (1 << PIN_DT2) | (1 << PIN_DT3) | (1 << PIN_DT4) | (1 << PIN_DT5) |
// //      (1 << PIN_DT6) | (1 << PIN_DT7) | (1 << PIN_DP) | (1 << PIN_ATN) | (1 << PIN_RST) | (1 << PIN_ACK) |
// //      (1 << PIN_REQ) | (1 << PIN_MSG) | (1 << PIN_CD) | (1 << PIN_IO) | (1 << PIN_BSY) | (1 << PIN_SEL))

// // #define GPIO_INEDGE ((1 << PIN_BSY) | (1 << PIN_SEL) | (1 << PIN_ATN) | (1 << PIN_ACK) | (1 << PIN_RST))

// // #define GPIO_MCI ((1 << PIN_MSG) | (1 << PIN_CD) | (1 << PIN_IO))

// #define UNUSED(x) (void)(x)

// // //---------------------------------------------------------------------------
// // //
// // //	Constant declarations (GIC)
// // //
// // //---------------------------------------------------------------------------
// // const static uint32_t ARM_GICD_BASE = 0xFF841000;
// // const static uint32_t ARM_GICC_BASE = 0xFF842000;
// // const static uint32_t ARM_GIC_END   = 0xFF847FFF;
// // const static int GICD_CTLR          = 0x000;
// // const static int GICD_IGROUPR0      = 0x020;
// // const static int GICD_ISENABLER0    = 0x040;
// // const static int GICD_ICENABLER0    = 0x060;
// // const static int GICD_ISPENDR0      = 0x080;
// // const static int GICD_ICPENDR0      = 0x0A0;
// // const static int GICD_ISACTIVER0    = 0x0C0;
// // const static int GICD_ICACTIVER0    = 0x0E0;
// // const static int GICD_IPRIORITYR0   = 0x100;
// // const static int GICD_ITARGETSR0    = 0x200;
// // const static int GICD_ICFGR0        = 0x300;
// // const static int GICD_SGIR          = 0x3C0;
// // const static int GICC_CTLR          = 0x000;
// // const static int GICC_PMR           = 0x001;
// // const static int GICC_IAR           = 0x003;
// // const static int GICC_EOIR          = 0x004;

// // //---------------------------------------------------------------------------
// // //
// // //	Constant declarations (GIC IRQ)
// // //
// // //---------------------------------------------------------------------------
// // const static int GIC_IRQLOCAL0 = (16 + 14);
// // const static int GIC_GPIO_IRQ  = (32 + 116); // GPIO3

// //---------------------------------------------------------------------------
// //
// //	Class definition
// //
// //---------------------------------------------------------------------------
// class GPIOBUS_RPi_rp1 : public GPIOBUS
// {
// public:
//     GPIOBUS_RPi_rp1() = default;
//     ~GPIOBUS_RPi_rp1() override = default;
//     bool Init(mode_e mode = mode_e::TARGET) override;

//     void Reset() override;
//     void Cleanup() override;

//     //	Bus signal acquisition
//     uint32_t Acquire() override;

//     // Set ENB signal
//     void SetENB(bool ast) override;

//     // Get BSY signal
//     bool GetBSY() const override;
//     // Set BSY signal
//     void SetBSY(bool ast) override;

//     // Get SEL signal
//     bool GetSEL() const override;
//     // Set SEL signal
//     void SetSEL(bool ast) override;

//     // Get ATN signal
//     bool GetATN() const override;
//     // Set ATN signal
//     void SetATN(bool ast) override;

//     // Get ACK signal
//     bool GetACK() const override;
//     // Set ACK signal
//     void SetACK(bool ast) override;

//     // Get ACT signal
//     bool GetACT() const override;
//     // Set ACT signal
//     void SetACT(bool ast) override;

//     // Get RST signal
//     bool GetRST() const override;
//     // Set RST signal
//     void SetRST(bool ast) override;

//     // Get MSG signal
//     bool GetMSG() const override;
//     // Set MSG signal
//     void SetMSG(bool ast) override;

//     // Get CD signal
//     bool GetCD() const override;
//     // Set CD signal
//     void SetCD(bool ast) override;

//     // Get IO signal
//     bool GetIO() override;
//     // Set IO signal
//     void SetIO(bool ast) override;

//     // Get REQ signal
//     bool GetREQ() const override;
//     // Set REQ signal
//     void SetREQ(bool ast) override;

//     // Get DAT signal
//     uint8_t GetDAT() override;
//     // Set DAT signal
//     void SetDAT(uint8_t dat) override;

//     bool WaitREQ(bool ast) override
//     {
//         UNUSED(ast);
//         printf("FIX THIS\n");
//         return false;
//         // return WaitSignal(PIN_REQ, ast);
//     }
//     bool WaitACK(bool ast) override
//     {
//         UNUSED(ast);
//         printf("FIX THIS\n");
//         return false;
//         // return WaitSignal(PIN_ACK, ast);
//     }
//     static uint32_t bcm_host_get_peripheral_address();

//     unique_ptr<DataSample> GetSample(uint64_t timestamp) override
//     {

//         // Acquire();
//         printf("FIX THIS\n");
//         return make_unique<DataSample_Raspberry>(0, timestamp);
//         // return make_unique<DataSample_Raspberry>(signals, timestamp);
//     }

//     //   protected:
//     // All bus signals
//     uint32_t signals = 0;
//     //     // GPIO input level
//     //     volatile uint32_t *level = nullptr;

//     //   private:
//     //     // SCSI I/O signal control
//     //     void MakeTable() override;
//     //     // Create work data
//     //     void SetControl(int pin, bool ast) override;
//     //     // Set Control Signal
//     //     void SetMode(int pin, int mode) override;
//     //     // Set SCSI I/O mode
//     //     bool GetSignal(int pin) const override;
//     //     // Get SCSI input signal value
//     //     void SetSignal(int pin, bool ast) override;
//     //     // Set SCSI output signal value

//     //     // Interrupt control
//     //     void DisableIRQ() override;
//     //     // IRQ Disabled
//     //     void EnableIRQ() override;
//     //     // IRQ Enabled

//     //     //  GPIO pin functionality settings
//     //     void PinConfig(int pin, int mode) override;
//     //     // GPIO pin direction setting
//     //     void PullConfig(int pin, int mode) override;
//     //     // GPIO pin pull up/down resistor setting
//     //     void PinSetSignal(int pin, bool ast) override;
//     //     // Set GPIO output signal
//     //     void DrvConfig(uint32_t drive) override;
//     //     // Set GPIO drive strength

//     //     static uint32_t get_dt_ranges(const char *filename, uint32_t offset);

//     //     uint32_t baseaddr = 0; // Base address

//     //     int rpitype = 0; // Type of Raspberry Pi

//     //     // GPIO register
//     //     volatile uint32_t *gpio = nullptr; // NOSONAR: volatile needed for register access
//     //     // PADS register
//     //     volatile uint32_t *pads = nullptr; // NOSONAR: volatile needed for register access

//     //     // Interrupt control register
//     //     volatile uint32_t *irpctl = nullptr;

//     //     // Interrupt enabled state
//     //     volatile uint32_t irptenb; // NOSONAR: volatile needed for register access

//     //     // QA7 register
//     //     volatile uint32_t *qa7regs = nullptr;
//     //     // Interupt control target CPU.
//     //     volatile int tintcore; // NOSONAR: volatile needed for register access

//     //     // Interupt control
//     //     volatile uint32_t tintctl; // NOSONAR: volatile needed for register access
//     //     // GICC priority setting
//     //     volatile uint32_t giccpmr; // NOSONAR: volatile needed for register access

//     // #if !defined(__x86_64__) && !defined(__X86__)
//     //     // GIC Interrupt distributor register
//     //     volatile uint32_t *gicd = nullptr;
//     // #endif
//     //     // GIC CPU interface register
//     //     volatile uint32_t *gicc = nullptr;

//     //     // RAM copy of GPFSEL0-4  values (GPIO Function Select)
//     //     array<uint32_t, 4> gpfsel;

//     // #if SIGNAL_CONTROL_MODE == 0
//     //     // Data mask table
//     //     array<array<uint32_t, 256>, 3> tblDatMsk;
//     //     // Data setting table
//     //     array<array<uint32_t, 256>, 3> tblDatSet;
//     // #else
//     //     // Data mask table
//     //     array<uint32_t, 256> tblDatMsk = {};
//     //     // Table setting table
//     //     array<uint32_t, 256> tblDatSet = {};
//     // #endif

//     //     static const array<int, 19> SignalTable;

//     //     const static int GPIO_FSEL_0     = 0;
//     //     const static int GPIO_FSEL_1     = 1;
//     //     const static int GPIO_FSEL_2     = 2;
//     //     const static int GPIO_FSEL_3     = 3;
//     //     const static int GPIO_SET_0      = 7;
//     //     const static int GPIO_CLR_0      = 10;
//     //     const static int GPIO_LEV_0      = 13;
//     //     const static int GPIO_EDS_0      = 16;
//     //     const static int GPIO_REN_0      = 19;
//     //     const static int GPIO_FEN_0      = 22;
//     //     const static int GPIO_HEN_0      = 25;
//     //     const static int GPIO_LEN_0      = 28;
//     //     const static int GPIO_AREN_0     = 31;
//     //     const static int GPIO_AFEN_0     = 34;
//     //     const static int GPIO_PUD        = 37;
//     //     const static int GPIO_CLK_0      = 38;
//     //     const static int GPIO_GPPINMUXSD = 52;
//     //     const static int GPIO_PUPPDN0    = 57;
//     //     const static int GPIO_PUPPDN1    = 58;
//     //     const static int GPIO_PUPPDN3    = 59;
//     //     const static int GPIO_PUPPDN4    = 60;
//     //     const static int PAD_0_27        = 11;
//     //     const static int IRPT_PND_IRQ_B  = 0;
//     //     const static int IRPT_PND_IRQ_1  = 1;
//     //     const static int IRPT_PND_IRQ_2  = 2;
//     //     const static int IRPT_FIQ_CNTL   = 3;
//     //     const static int IRPT_ENB_IRQ_1  = 4;
//     //     const static int IRPT_ENB_IRQ_2  = 5;
//     //     const static int IRPT_ENB_IRQ_B  = 6;
//     //     const static int IRPT_DIS_IRQ_1  = 7;
//     //     const static int IRPT_DIS_IRQ_2  = 8;
//     //     const static int IRPT_DIS_IRQ_B  = 9;
//     //     const static int QA7_CORE0_TINTC = 16;
//     //     const static int GPIO_IRQ        = (32 + 20); // GPIO3

//     //     const static uint32_t IRPT_OFFSET = 0x0000B200;
//     //     const static uint32_t PADS_OFFSET = 0x00100000;
//     //     const static uint32_t GPIO_OFFSET = 0x00200000;
//     //     const static uint32_t QA7_OFFSET  = 0x01000000;

//     const static uint32_t RP1_NUM_GPIOS = 54;

//     const static uint32_t RP1_IO_BANK0_OFFSET = 0x00000000;
//     const static uint32_t RP1_IO_BANK1_OFFSET = 0x00004000;
//     const static uint32_t RP1_IO_BANK2_OFFSET = 0x00008000;
//     const static uint32_t RP1_SYS_RIO_BANK0_OFFSET = 0x00010000;
//     const static uint32_t RP1_SYS_RIO_BANK1_OFFSET = 0x00014000;
//     const static uint32_t RP1_SYS_RIO_BANK2_OFFSET = 0x00018000;
//     const static uint32_t RP1_PADS_BANK0_OFFSET = 0x00020000;
//     const static uint32_t RP1_PADS_BANK1_OFFSET = 0x00024000;
//     const static uint32_t RP1_PADS_BANK2_OFFSET = 0x00028000;

//     const static uint32_t RP1_RW_OFFSET = 0x0000;
//     const static uint32_t RP1_XOR_OFFSET = 0x1000;
//     const static uint32_t RP1_SET_OFFSET = 0x2000;
//     const static uint32_t RP1_CLR_OFFSET = 0x3000;

//     const static uint32_t RP1_GPIO_CTRL_FSEL_LSB = 0;
//     const static uint32_t RP1_GPIO_CTRL_FSEL_MASK = (0x1f << RP1_GPIO_CTRL_FSEL_LSB);
//     const static uint32_t RP1_GPIO_CTRL_OUTOVER_LSB = 12;
//     const static uint32_t RP1_GPIO_CTRL_OUTOVER_MASK = (0x03 << RP1_GPIO_CTRL_OUTOVER_LSB);
//     const static uint32_t RP1_GPIO_CTRL_OEOVER_LSB = 14;
//     const static uint32_t RP1_GPIO_CTRL_OEOVER_MASK = (0x03 << RP1_GPIO_CTRL_OEOVER_LSB);

//     const static uint32_t RP1_PADS_OD_SET = (1 << 7);
//     const static uint32_t RP1_PADS_IE_SET = (1 << 6);
//     const static uint32_t RP1_PADS_PUE_SET = (1 << 3);
//     const static uint32_t RP1_PADS_PDE_SET = (1 << 2);

//     inline uint32_t RP1_GPIO_IO_REG_STATUS_OFFSET(int offset) { (((offset * 2) + 0) * sizeof(uint32_t)); }
//     inline uint32_t RP1_GPIO_IO_REG_CTRL_OFFSET(int offset) { (((offset * 2) + 1) * sizeof(uint32_t)); }
//     inline uint32_t RP1_GPIO_PADS_REG_OFFSET(int offset) { (sizeof(uint32_t) + (offset * sizeof(uint32_t))); }

//     const static uint32_t RP1_GPIO_SYS_RIO_REG_OUT_OFFSET = 0x0;
//     const static uint32_t RP1_GPIO_SYS_RIO_REG_OE_OFFSET = 0x4;
//     const static uint32_t RP1_GPIO_SYS_RIO_REG_SYNC_IN_OFFSET = 0x8;

//     inline void rp1_gpio_write32(volatile uint32_t *base, int peri_offset, int reg_offset, uint32_t value)
//     {
//         base[(peri_offset + reg_offset) / 4] = value;
//     }

//     inline uint32_t rp1_gpio_read32(volatile uint32_t *base, int peri_offset, int reg_offset)
//     {
//         return base[(peri_offset + reg_offset) / 4];
//     }



// typedef struct
// {
//    uint32_t io[3];
//    uint32_t pads[3];
//    uint32_t sys_rio[3];
// } GPIO_STATE_T;

// typedef enum
// {
//     RP1_FSEL_ALT0       = 0x0,
//     RP1_FSEL_ALT1       = 0x1,
//     RP1_FSEL_ALT2       = 0x2,
//     RP1_FSEL_ALT3       = 0x3,
//     RP1_FSEL_ALT4       = 0x4,
//     RP1_FSEL_ALT5       = 0x5,
//     RP1_FSEL_ALT6       = 0x6,
//     RP1_FSEL_ALT7       = 0x7,
//     RP1_FSEL_ALT8       = 0x8,
//     RP1_FSEL_COUNT,
//     RP1_FSEL_SYS_RIO    = RP1_FSEL_ALT5,
//     RP1_FSEL_NULL       = 0x1f
// } RP1_FSEL_T;

// const static GPIO_STATE_T gpio_state;
// const static int rp1_bank_base[];









// #define NUM_HDR_PINS 40
// #define MAX_GPIO_PINS 300

// #define GPIO_INVALID (~0U)
// #define GPIO_GND (~1U)
// #define GPIO_5V (~2U)
// #define GPIO_3V3 (~3U)
// #define GPIO_1V8 (~4U)
// #define GPIO_OTHER (~5U)

// typedef enum
// {
//     GPIO_FSEL_FUNC0,
//     GPIO_FSEL_FUNC1,
//     GPIO_FSEL_FUNC2,
//     GPIO_FSEL_FUNC3,
//     GPIO_FSEL_FUNC4,
//     GPIO_FSEL_FUNC5,
//     GPIO_FSEL_FUNC6,
//     GPIO_FSEL_FUNC7,
//     GPIO_FSEL_FUNC8,
//     /* ... */
//     GPIO_FSEL_INPUT = 0x10,
//     GPIO_FSEL_OUTPUT,
//     GPIO_FSEL_GPIO, /* Preserves direction if possible, else input */
//     GPIO_FSEL_NONE, /* If possible, else input */
//     GPIO_FSEL_MAX
// } GPIO_FSEL_T;

// typedef enum
// {
//     PULL_NONE,
//     PULL_DOWN,
//     PULL_UP,
//     PULL_MAX
// } GPIO_PULL_T;

// typedef enum
// {
//     DIR_INPUT,
//     DIR_OUTPUT,
//     DIR_MAX,
// } GPIO_DIR_T;

// typedef enum
// {
//     DRIVE_LOW,
//     DRIVE_HIGH,
//     DRIVE_MAX
// } GPIO_DRIVE_T;





// typedef struct GPIO_CHIP_
// {
//     const char *name;
//     const char *compatible;
//     // const GPIO_CHIP_INTERFACE_T *interface;
//     int size;
//     uintptr_t data;
// } GPIO_CHIP_T;





// // const char *GPIOBUS_RPi_rp1::rp1_gpio_fsel_names[GPIOBUS_RPi_rp1::RP1_NUM_GPIOS][GPIOBUS_RPi_rp1::RP1_FSEL_COUNT];
// void rp1_gpio_get_bank(int num, int *bank, int *offset);
// uint32_t rp1_gpio_ctrl_read(volatile uint32_t *base, int bank, int offset);
// void rp1_gpio_ctrl_write(volatile uint32_t *base, int bank, int offset, uint32_t value);
// uint32_t rp1_gpio_pads_read(volatile uint32_t *base, int bank, int offset);
// void rp1_gpio_pads_write(volatile uint32_t *base, int bank, int offset,  uint32_t value);
// uint32_t rp1_gpio_sys_rio_out_read(volatile uint32_t *base, int bank,  int offset);
// uint32_t rp1_gpio_sys_rio_sync_in_read(volatile uint32_t *base, int bank,  int offset);
// void rp1_gpio_sys_rio_out_set(volatile uint32_t *base, int bank, int offset);
// void rp1_gpio_sys_rio_out_clr(volatile uint32_t *base, int bank, int offset);
// uint32_t rp1_gpio_sys_rio_oe_read(volatile uint32_t *base, int bank);
// void rp1_gpio_sys_rio_oe_clr(volatile uint32_t *base, int bank, int offset);
// void rp1_gpio_sys_rio_oe_set(volatile uint32_t *base, int bank, int offset);
// void rp1_gpio_set_dir(void *priv, uint32_t gpio, GPIO_DIR_T dir);
// GPIO_DIR_T rp1_gpio_get_dir(void *priv, unsigned gpio);
// GPIO_FSEL_T rp1_gpio_get_fsel(void *priv, unsigned gpio);
// void rp1_gpio_set_fsel(void *priv, unsigned gpio, const GPIO_FSEL_T func);
// int rp1_gpio_get_level(void *priv, unsigned gpio);
// void rp1_gpio_set_drive(void *priv, unsigned gpio, GPIO_DRIVE_T drv);
// void rp1_gpio_set_pull(void *priv, unsigned gpio, GPIO_PULL_T pull);
// GPIO_PULL_T rp1_gpio_get_pull(void *priv, unsigned gpio);
// GPIO_DRIVE_T rp1_gpio_get_drive(void *priv, unsigned gpio);
// const char *rp1_gpio_get_name(void *priv, unsigned gpio);
// const char *rp1_gpio_get_fsel_name(void *priv, unsigned gpio, GPIO_FSEL_T fsel);
// void *rp1_gpio_create_instance(const GPIO_CHIP_T *chip, const char *dtnode);
// int rp1_gpio_count(void *priv);
// void *rp1_gpio_probe_instance(void *priv, volatile uint32_t *base);












// };
