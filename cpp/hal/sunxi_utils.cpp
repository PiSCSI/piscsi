#include "hal/sunxi_utils.h"
#include <stdio.h>

#define BLACK "\033[30m"   /* Black */
#define RED "\033[31m"     /* Red */
#define GREEN "\033[32m"   /* Green */
#define YELLOW "\033[33m"  /* Yellow */
#define BLUE "\033[34m"    /* Blue */
#define MAGENTA "\033[35m" /* Magenta */
#define CYAN "\033[36m"    /* Cyan */
#define WHITE "\033[37m"   /* White */
void dump_gpio_registers(SunXI::sunxi_gpio_reg_t *regs)
{
    printf(CYAN "--- GPIO BANK 0 CFG: %08X %08X %08X %08X\n", regs->gpio_bank[0].CFG[0], regs->gpio_bank[0].CFG[1],
           regs->gpio_bank[0].CFG[2], regs->gpio_bank[0].CFG[3]);

    printf("---      Dat: (%08X)  DRV: %08X %08X\n", regs->gpio_bank[0].DAT, regs->gpio_bank[0].DRV[0],
           regs->gpio_bank[0].DRV[1]);
    printf("---      Pull: %08X %08x\n", regs->gpio_bank[0].PULL[0], regs->gpio_bank[0].PULL[1]);

    printf("--- GPIO INT CFG: %08X %08X %08X\n", regs->gpio_int.CFG[0], regs->gpio_int.CFG[1], regs->gpio_int.CFG[2]);
    printf("---      CTL: (%08X)  STA: %08X DEB: %08X\n " WHITE, regs->gpio_int.CTL, regs->gpio_int.STA,
           regs->gpio_int.DEB);
}