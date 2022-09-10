
// Kernel module to access cycle count registers:
//       https://matthewarcus.wordpress.com/2018/01/27/using-the-cycle-counter-registers-on-the-raspberry-pi-3/


//https://mindplusplus.wordpress.com/2013/05/21/accessing-the-raspberry-pis-1mhz-timer/

// Reading register from user space:
//      https://stackoverflow.com/questions/59749160/reading-from-register-of-allwinner-h3-arm-processor


// Maybe kernel patch>
//https://yhbt.net/lore/all/20140707085858.GG16262@lukather/T/


//
// Access the Raspberry Pi System Timer registers directly. 
//
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdint.h>


#include "common.h"

typedef uint32_t u32;

/* from u-boot code: */
struct sun4i_dram_para {
	u32 baseaddr;
	u32 clock;
	u32 type;
	u32 rank_num;
	u32 density;
	u32 io_width;
	u32 bus_width;
	u32 cas;
	u32 zq;
	u32 odt_en;
	u32 size;
	u32 tpr0;
	u32 tpr1;
	u32 tpr2;
	u32 tpr3;
	u32 tpr4;
	u32 tpr5;
	u32 emr1;
	u32 emr2;
	u32 emr3;
};

#define DEVMEM_FILE "/dev/mem"
static int devmem_fd;

enum sunxi_soc_version {
	SUNXI_SOC_SUN4I = 0x1623, /* A10 */
	SUNXI_SOC_SUN5I = 0x1625, /* A13, A10s */
	SUNXI_SOC_SUN6I = 0x1633, /* A31 */
	SUNXI_SOC_SUN7I = 0x1651, /* A20 */
	SUNXI_SOC_SUN8I = 0x1650, /* A23 */
	SUNXI_SOC_SUN9I = 0x1667, /* A33 */
	SUNXI_SOC_SUN10I = 0x1635, /* A80 */
    SUNXI_SOC_H3 = 0x1680, /* H3 */
};

static enum sunxi_soc_version soc_version;

// #define PERIPHERAL_BASE 0x20000000   // For Pi 1 and 2
//#define PERIPHERAL_BASE 0x3F000000      // For Pi 3
#define PERIPHERAL_BASE 0xfe000000      // For PI 4
#define SYSTEM_TIMER_OFFSET 0x3000

volatile void* hs_timer;
volatile void* system_bus;

static const uint32_t system_bus_base_address = 0x01C20000;

static const uint32_t BUS_CLK_GATING_REG0 = 0x60;
static const uint32_t BUS_CLK_GATING_REG1 = 0x64;
static const uint32_t BUS_CLK_GATING_REG2 = 0x68;
static const uint32_t BUS_CLK_GATING_REG3 = 0x6C;
static const uint32_t BUS_CLK_GATING_REG4 = 0x70;

static const uint32_t BUS_CLK_GATING_REG0_USBOHCI3 =31;
static const uint32_t BUS_CLK_GATING_REG0_USBOHCI2 =30;
static const uint32_t BUS_CLK_GATING_REG0_USBOHCI1 =29;
static const uint32_t BUS_CLK_GATING_REG0_USBOHCI0 =28;
static const uint32_t BUS_CLK_GATING_REG0_USBEHCI3 =27;
static const uint32_t BUS_CLK_GATING_REG0_USBEHCI2 =26;
static const uint32_t BUS_CLK_GATING_REG0_USBEHCI1 =25;
static const uint32_t BUS_CLK_GATING_REG0_USBEHCI0 =24;
static const uint32_t BUS_CLK_GATING_REG0_USB_OTG =23;
static const uint32_t BUS_CLK_GATING_REG0_SPI1 =21;
static const uint32_t BUS_CLK_GATING_REG0_SPI0 =20;
static const uint32_t BUS_CLK_GATING_REG0_HSTMR =19;
static const uint32_t BUS_CLK_GATING_REG0_TS =18;
static const uint32_t BUS_CLK_GATING_REG0_EMAC =17;
static const uint32_t BUS_CLK_GATING_REG0_DRAM =14;
static const uint32_t BUS_CLK_GATING_REG0_NAND =13;
static const uint32_t BUS_CLK_GATING_REG0_MMC2 =10;
static const uint32_t BUS_CLK_GATING_REG0_MMC1 =9;
static const uint32_t BUS_CLK_GATING_REG0_MMC0 =8;
static const uint32_t BUS_CLK_GATING_REG0_DMA =6;
static const uint32_t BUS_CLK_GATING_REG0_CE =5;


static const uint32_t BUS_SOFT_RST_REG0 = 0x2C0;
static const uint32_t BUS_SOFT_RST_REG1 = 0x2C4;
static const uint32_t BUS_SOFT_RST_REG2 = 0x2C8;
static const uint32_t BUS_SOFT_RST_REG3 = 0x2D0;
static const uint32_t BUS_SOFT_RST_REG4 = 0x2D8;


static const uint32_t BUS_SOFT_RST_REG0_USBOHCI3 =31;
static const uint32_t BUS_SOFT_RST_REG0_USBOHCI2 =30;
static const uint32_t BUS_SOFT_RST_REG0_USBOHCI1 =29;
static const uint32_t BUS_SOFT_RST_REG0_USBOHCI0 =28;
static const uint32_t BUS_SOFT_RST_REG0_USBEHCI3 =27;
static const uint32_t BUS_SOFT_RST_REG0_USBEHCI2 =26;
static const uint32_t BUS_SOFT_RST_REG0_USBEHCI1 =25;
static const uint32_t BUS_SOFT_RST_REG0_USBEHCI0 =24;
static const uint32_t BUS_SOFT_RST_REG0_USB_OTG =23;
static const uint32_t BUS_SOFT_RST_REG0_SPI1 =21;
static const uint32_t BUS_SOFT_RST_REG0_SPI0 =20;
static const uint32_t BUS_SOFT_RST_REG0_HSTMR =19;
static const uint32_t BUS_SOFT_RST_REG0_TS =18;
static const uint32_t BUS_SOFT_RST_REG0_EMAC =17;
static const uint32_t BUS_SOFT_RST_REG0_DRAM =14;
static const uint32_t BUS_SOFT_RST_REG0_NAND =13;
static const uint32_t BUS_SOFT_RST_REG0_MMC2 =10;
static const uint32_t BUS_SOFT_RST_REG0_MMC1 =9;
static const uint32_t BUS_SOFT_RST_REG0_MMC0 =8;
static const uint32_t BUS_SOFT_RST_REG0_DMA =6;
static const uint32_t BUS_SOFT_RST_REG0_CE =5;



/*
 * Find out exactly which SoC we are dealing with.
 */
#define SUNXI_IO_SRAM_BASE	0x01C00000
#define SUNXI_IO_SRAM_SIZE	0x00001000

#define SUNXI_IO_SRAM_VERSION	0x24


static const uint32_t hs_timer_base_address = 0x01C60000;


static const uint32_t HS_TMR_IRA_EN_EG = 0x00;
static const uint32_t HS_TMR_IRQ_STAT_REG = 0x04;
static const uint32_t HS_TMR_CTRL_REG = 0x10;
static const uint32_t HS_TMR_INTV_LO_REG = 0x14;
static const uint32_t HS_TMR_INTV_HI_REG = 0x18;
static const uint32_t HS_TMR_CURNT_LO_REG = 0x1C;
static const uint32_t HS_TMR_CURNT_HI_REG = 0x20;




#define ST_BASE (PERIPHERAL_BASE + SYSTEM_TIMER_OFFSET) 

// Sytem Timer Registers layout
typedef struct {
    uint32_t control_and_status;
    uint32_t counter_low;
    uint32_t counter_high;
    uint32_t compare_0;
    uint32_t compare_1;
    uint32_t compare_2;
    uint32_t compare_3;
} system_timer_t;


inline uint32_t set_bit(uint32_t value, uint32_t bit_num){
    return( value | (1 << bit_num));
}

inline uint32_t clear_bit(uint32_t value, uint32_t bit_num){
    return( value & ~(1 << bit_num));
}

inline uint32_t get_bit(uint32_t value, uint32_t bit_num){
    return ((value >> bit_num) & 1UL);
}


inline uint32_t io_readl(volatile void* base, uint32_t offset){
    return *(volatile uint32_t*) ((uint32_t)base + offset);
}

inline void io_writel(volatile void* base, uint32_t offset, uint32_t value){
    *(volatile uint32_t*) ((uint32_t)base + offset) = value;
}

void
inline sunxi_io_mask(void *base, int offset, unsigned int value, unsigned int mask)
{
	unsigned int tmp = io_readl(base, offset);

	tmp &= ~mask;
	tmp |= value & mask;

	io_writel(base, offset, tmp);
}

inline uint32_t sysbus_readl(uint32_t offset) { return io_readl(system_bus, offset); }
inline void sysbus_writel(uint32_t offset, uint32_t value){ io_writel(system_bus, offset, value);}
inline uint32_t hstimer_readl(uint32_t offset) { return io_readl(hs_timer, offset); }
inline void hstimer_writel(uint32_t offset, uint32_t value) { io_writel(hs_timer, offset, value);}

void dump_sys_bus(){

    printf("System bus..... %08X\n\r", system_bus_base_address);
    printf("Gating Reg: %08X %08X %08X %08X %08X\n\r",
        sysbus_readl(BUS_CLK_GATING_REG0), 
        sysbus_readl(BUS_CLK_GATING_REG1), 
        sysbus_readl(BUS_CLK_GATING_REG2), 
        sysbus_readl(BUS_CLK_GATING_REG3), 
        sysbus_readl(BUS_CLK_GATING_REG4)
        );
    printf("Reset Reg: %08X %08X %08X %08X %08X\n\r",
        sysbus_readl(BUS_SOFT_RST_REG0), 
        sysbus_readl(BUS_SOFT_RST_REG1), 
        sysbus_readl(BUS_SOFT_RST_REG2), 
        sysbus_readl(BUS_SOFT_RST_REG3), 
        sysbus_readl(BUS_SOFT_RST_REG4)
        );

}


void dump_hs_timer(){

    printf("Hs timer..... %08X\n\r", hs_timer_base_address);
    printf("HS_TMR_IRA_EN_EG %08X\n\r", hstimer_readl(HS_TMR_IRA_EN_EG));
    printf("HS_TMR_IRQ_STAT_REG %08X\n\r", hstimer_readl(HS_TMR_IRQ_STAT_REG));
    printf("HS_TMR_CTRL_REG %08X\n\r", hstimer_readl(HS_TMR_CTRL_REG));
    printf("HS_TMR_INTV_LO_REG %08X\n\r", hstimer_readl(HS_TMR_INTV_LO_REG));
    printf("HS_TMR_CURNT_LO_REG %08X\n\r", hstimer_readl(HS_TMR_CURNT_LO_REG));
    printf("HS_TMR_CURNT_HI_REG %08X\n\r", hstimer_readl(HS_TMR_CURNT_HI_REG));

}

static int
soc_version_read(void)
{
	void *base;
	unsigned int restore;

	base = mmap(NULL, SUNXI_IO_SRAM_SIZE, PROT_READ|PROT_WRITE,
		    MAP_SHARED, devmem_fd, SUNXI_IO_SRAM_BASE);
	if (base == MAP_FAILED) {
		fprintf(stderr, "Failed to map sram registers:");
        // %s\n",
		//	strerror(errno));
		return -1;
	}

	restore = io_readl(base, SUNXI_IO_SRAM_VERSION);

	sunxi_io_mask(base, SUNXI_IO_SRAM_VERSION, 0x8000, 0x8000);

	soc_version = (sunxi_soc_version)(io_readl(base, SUNXI_IO_SRAM_VERSION) >> 16);

	sunxi_io_mask(base, SUNXI_IO_SRAM_VERSION, restore, 0x8000);

	munmap(base, SUNXI_IO_SRAM_SIZE);

	return 0;
}



// Get access to the System Timer registers in user memory space.
void get_hs_timer() {

    int  fd;

    if ((fd = open("/dev/mem", O_RDWR | O_SYNC) ) < 0) {
        printf("1 can't open /dev/mem \n");
        exit(-1);
    }

    hs_timer = mmap(
        NULL,
        4096,
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        fd,
        // ST_BASE
        hs_timer_base_address
    );

    system_bus = mmap(
        NULL,
        4096,
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        fd,
        // ST_BASE
        system_bus_base_address
    );

    close(fd);

    if (hs_timer == MAP_FAILED) {
        printf("mmap error %d\n", (int)hs_timer);  // errno also set!
        exit(-1);
    }
}

int main(int argc, char **argv) {
	devmem_fd = open(DEVMEM_FILE, O_RDWR);
	if (devmem_fd == -1) {
		fprintf(stderr, "Error: failed to open %s\n", DEVMEM_FILE);
	}

     (void)soc_version_read();
     printf("SoC Version: %08X\n\r", (uint32_t)soc_version);

     get_hs_timer();

    // uint32_t t0, t1 = 0;

    // uint32_t delay = 2;

    dump_sys_bus();





// By default, the HSTimer clock gating is masked. When it is necessary to use
// the HSTimer, its clock gating should be opened in BUS Clock Gating Register 0
// and then de-assert the software reset in BUS Software Reset Register 0 on the
// CCU module. If it is not needed to use the HSTimer, both the gating bit and
// the software reset bit should be set 0.


    printf("[Before] CLK GATE: %08X  SOFT RST: %08X\n\r",
        sysbus_readl(BUS_CLK_GATING_REG0), 
        sysbus_readl(BUS_SOFT_RST_REG0));

        sysbus_writel(BUS_CLK_GATING_REG0, set_bit(sysbus_readl(BUS_CLK_GATING_REG0), BUS_CLK_GATING_REG0_HSTMR));
        sysbus_writel(BUS_SOFT_RST_REG0, set_bit(sysbus_readl(BUS_SOFT_RST_REG0), BUS_SOFT_RST_REG0_HSTMR));


    printf("[After] CLK GATE: %08X  SOFT RST: %08X\n\r",
        sysbus_readl(BUS_CLK_GATING_REG0), 
        sysbus_readl(BUS_SOFT_RST_REG0));


// Make a 1us delay using HSTimer for an instance as follow:
// AHB1CLK will be configured as 100MHz and n_mode,
// Single mode and 2 pre-scale will be selected in this instance

// Set interval value Hi 0x0
hstimer_writel(HS_TMR_INTV_HI_REG, 0x0);
// Set interval value Lo 0x32
hstimer_writel(HS_TMR_INTV_LO_REG, 0x32000000);
// Select n_mode,2 prescale, single mode
hstimer_writel(HS_TMR_CTRL_REG, 0x90);
// Set reload bit
hstimer_writel(HS_TMR_CTRL_REG, hstimer_readl(HS_TMR_CTRL_REG)|(1<<1));
// Enable HSTimer
hstimer_writel(HS_TMR_CTRL_REG, hstimer_readl(HS_TMR_CTRL_REG)|(1<<0));
//Wait for HSTimer to generate pending
while (!(hstimer_readl(HS_TMR_IRQ_STAT_REG)&0x1));
// Clear HSTimer pending
hstimer_writel(HS_TMR_IRQ_STAT_REG, 1);

dump_hs_timer();


    // while (1) {
    //     t0 = system_timer->counter_low;
    //     while((system_timer->counter_low - t0) < delay)
    //     // usleep(100);
    //     t1 = system_timer->counter_low;
    //     printf ("Elaspsed = %d\n", t1 - t0);
    //     printf ("Conter high = %d\n", system_timer->counter_high);
    //     t0 = t1;
    //     sleep(1);
    // }
    return 0;
}


// int main(int argc, char **argv) {
//     volatile system_timer_t* system_timer = get_system_timer();
//     int32_t t0, t1;

//     while (1) {
//         t0 = system_timer->counter_low;
//         usleep(100);
//         t1 = system_timer->counter_low;
//         printf ("Elaspsed = %d\n", t1 - t0);
//         printf ("Conter high = %d\n", system_timer->counter_high);
//         t0 = t1;
//     }
//     return 0;
// }