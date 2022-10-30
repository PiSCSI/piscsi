//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI Reloaded
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//	[ GPIO-SCSI bus ]
//
//---------------------------------------------------------------------------

#pragma once

#include <map>
#include "config.h"
#include "hal/gpiobus.h"
#include "hal/board_type.h"
#include "log.h"
#include "scsi.h"

//---------------------------------------------------------------------------
//
//	Class definition
//
//---------------------------------------------------------------------------
class GPIOBUS_Allwinner : public GPIOBUS
{
  public:
    // Basic Functions
    GPIOBUS_Allwinner()  = default;
    ~GPIOBUS_Allwinner() override = default;
    // Destructor
    bool Init(mode_e mode = mode_e::TARGET, board_type::rascsi_board_type_e 
                rascsi_type = board_type::rascsi_board_type_e::BOARD_TYPE_FULLSPEC) override;

    // Initialization
    void Reset() override;
    // Reset
    void Cleanup() override;
    // Cleanup

    //---------------------------------------------------------------------------
    //
    //	Bus signal acquisition
    //
    //---------------------------------------------------------------------------
    uint32_t Acquire() override;

    BYTE GetDAT() override;
    // Get DAT signal
    void SetDAT(BYTE dat) override;
    // Set DAT signal
  protected:
    // SCSI I/O signal control
    void MakeTable() override;
    // Create work data
    void SetControl(board_type::pi_physical_pin_e pin, board_type::gpio_high_low_e ast) override;
    // Set Control Signal
    void SetMode(board_type::pi_physical_pin_e pin, board_type::gpio_direction_e mode) override;
    // Set SCSI I/O mode
    bool GetSignal(board_type::pi_physical_pin_e pin) const override;
    // Get SCSI input signal value
    void SetSignal(board_type::pi_physical_pin_e pin, board_type::gpio_high_low_e ast) override;
    // Set SCSI output signal value
    bool WaitSignal(board_type::pi_physical_pin_e pin, board_type::gpio_high_low_e ast) override;
    // Wait for a signal to change
    // Interrupt control
    void DisableIRQ() override;
    // IRQ Disabled
    void EnableIRQ() override;
    // IRQ Enabled

    //  GPIO pin functionality settings
    void PinConfig(board_type::pi_physical_pin_e pin, board_type::gpio_direction_e mode) override;
    // GPIO pin direction setting
    void PullConfig(board_type::pi_physical_pin_e pin, board_type::gpio_pull_up_down_e mode) override;
    // GPIO pin pull up/down resistor setting
    void PinSetSignal(board_type::pi_physical_pin_e pin, board_type::gpio_high_low_e ast) override;
    // Set GPIO output signal
    void DrvConfig(DWORD drive) override;
    // Set GPIO drive strength

    // Map the physical pin number to the logical GPIO number
    const static std::map<board_type::pi_physical_pin_e, int> phys_to_gpio_map;

#if !defined(__x86_64__) && !defined(__X86__)
    uint32_t baseaddr = 0; // Base address
#endif

    volatile uint32_t *gpio = nullptr; // GPIO register

    volatile uint32_t *pads = nullptr; // PADS register

static volatile uint32_t *gpio_map;



int bpi_piGpioLayout (void)
{
  FILE *bpiFd ;
  char buffer[1024];
  char hardware[1024];
  struct BPIBoards *board;
  static int  gpioLayout = -1 ;

  if (gpioLayout != -1)	// No point checking twice
    return gpioLayout ;

  bpi_found = 0; // -1: not init, 0: init but not found, 1: found
  if ((bpiFd = fopen("/var/lib/bananapi/board.sh", "r")) == NULL) {
    return -1;
  }
  while(!feof(bpiFd)) {
    fgets(buffer, sizeof(buffer), bpiFd);
    sscanf(buffer, "BOARD=%s", hardware);
    //printf("BPI: buffer[%s] hardware[%s]\n",buffer, hardware);
// Search for board:
    for (board = bpiboard ; board->name != NULL ; ++board) {
      //printf("BPI: name[%s] hardware[%s]\n",board->name, hardware);
      if (strcmp (board->name, hardware) == 0) {
        //gpioLayout = board->gpioLayout;
        gpioLayout = board->model; // BPI: use model to replace gpioLayout
        //printf("BPI: name[%s] gpioLayout(%d)\n",board->name, gpioLayout);
        if(gpioLayout >= 21) {
          bpi_found = 1;
          break;
        }
      }
    }
    if(bpi_found == 1) {
      break;
    }
  }
  fclose(bpiFd);
  //printf("BPI: name[%s] gpioLayout(%d)\n",board->name, gpioLayout);
  return gpioLayout ;
}

int bpi_get_rpi_info(rpi_info *info)
{
  struct BPIBoards *board=bpiboard;
  static int  gpioLayout = -1 ;
  char ram[64];
  char manufacturer[64];
  char processor[64];
  char type[64];

  gpioLayout = bpi_piGpioLayout () ;
  printf("BPI: gpioLayout(%d)\n", gpioLayout);
  if(bpi_found == 1) {
    board = &bpiboard[gpioLayout];
    printf("BPI: name[%s] gpioLayout(%d)\n",board->name, gpioLayout);
    sprintf(ram, "%dMB", piMemorySize [board->mem]);
    sprintf(type, "%s", piModelNames [board->model]);
     //add by jackzeng
     //jude mtk platform
    if(strcmp(board->name, "bpi-r2") == 0){
        bpi_found_mtk = 1;
	printf("found mtk board\n");
    }
    sprintf(manufacturer, "%s", piMakerNames [board->maker]);
    info->p1_revision = 3;
    info->type = type;
    info->ram  = ram;
    info->manufacturer = manufacturer;
    if(bpi_found_mtk == 1){
        info->processor = "MTK";
    }else{
	info->processor = "Allwinner";
    }
    
    strcpy(info->revision, "4001");
//    pin_to_gpio =  board->physToGpio ;
    pinToGpio_BP =  board->pinToGpio ;
    physToGpio_BP = board->physToGpio ;
    pinTobcm_BP = board->pinTobcm ;
    //printf("BPI: name[%s] bType(%d) model(%d)\n",board->name, bType, board->model);
    return 0;
  }
  return -1;
}

    

#if !defined(__x86_64__) && !defined(__X86__)
    volatile uint32_t *level = nullptr; // GPIO input level
#endif

    volatile uint32_t *irpctl = nullptr; // Interrupt control register

    volatile uint32_t irptenb; // Interrupt enabled state

    volatile uint32_t *qa7regs = nullptr; // QA7 register

    volatile int tintcore; // Interupt control target CPU.

    volatile uint32_t tintctl; // Interupt control

    volatile uint32_t giccpmr; // GICC priority setting

#if !defined(__x86_64__) && !defined(__X86__)
    volatile uint32_t *gicd = nullptr; // GIC Interrupt distributor register
#endif

    volatile uint32_t *gicc = nullptr; // GIC CPU interface register

    array<uint32_t, 4> gpfsel; // GPFSEL0-4 backup values

    uint32_t signals = 0; // All bus signals

#ifdef USE_SEL_EVENT_ENABLE
    struct gpioevent_request selevreq = {}; // SEL signal event request

    int epfd; // epoll file descriptor
#endif        // USE_SEL_EVENT_ENABLE

#if SIGNAL_CONTROL_MODE == 0
    array<array<uint32_t, 256>, 3> tblDatMsk; // Data mask table

    array<array<uint32_t, 256>, 3> tblDatSet; // Data setting table
#else
    array<uint32_t, 256> tblDatMsk = {}; // Data mask table

    array<uint32_t, 256> tblDatSet = {}; // Table setting table
#endif


uint32_t sunxi_readl(volatile uint32_t *addr);
void sunxi_writel(volatile uint32_t *addr, uint32_t val);


    int sunxi_setup(void);

    void sunxi_set_pullupdn(int gpio, int pud);
    void sunxi_setup_gpio(int gpio, int direction, int pud);
    int sunxi_gpio_function(int gpio);

    void sunxi_output_gpio(int gpio, int value);
    int sunxi_input_gpio(int gpio);






};
