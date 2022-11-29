//---------------------------------------------------------------------------
//
//  SCSI Target Emulator RaSCSI Reloaded
//  for Raspberry Pi
//
//  Copyright (C) 2022 akuker
//
//  [ Utility functions for working with Allwinner CPUs ]
//
//  This should include generic functions that can be applicable to
//  different variants of the SunXI (Allwinner) SoCs
//
//---------------------------------------------------------------------------

#include "hal/sunxi_utils.h"
#include <stdio.h>
#include <string>

using namespace std;

static const string BLACK   = "\033[30m"; /* Black */
static const string RED     = "\033[31m"; /* Red */
static const string GREEN   = "\033[32m"; /* Green */
static const string YELLOW  = "\033[33m"; /* Yellow */
static const string BLUE    = "\033[34m"; /* Blue */
static const string MAGENTA = "\033[35m"; /* Magenta */
static const string CYAN    = "\033[36m"; /* Cyan */
static const string WHITE   = "\033[37m"; /* White */

// TODO: this is only a debug function that will be removed at a later date.....
void dump_gpio_registers(SunXI::sunxi_gpio_reg_t *regs)
{
    printf("%s--- GPIO BANK 0 CFG: %08X %08X %08X %08X\n", CYAN.c_str(), regs->gpio_bank[0].CFG[0],
           regs->gpio_bank[0].CFG[1], regs->gpio_bank[0].CFG[2], regs->gpio_bank[0].CFG[3]);

    printf("---      Dat: (%08X)  DRV: %08X %08X\n", regs->gpio_bank[0].DAT, regs->gpio_bank[0].DRV[0],
           regs->gpio_bank[0].DRV[1]);
    printf("---      Pull: %08X %08x\n", regs->gpio_bank[0].PULL[0], regs->gpio_bank[0].PULL[1]);

    printf("--- GPIO INT CFG: %08X %08X %08X\n", regs->gpio_int.CFG[0], regs->gpio_int.CFG[1], regs->gpio_int.CFG[2]);
    printf("---      CTL: (%08X)  STA: %08X DEB: %08X\n %s", regs->gpio_int.CTL, regs->gpio_int.STA, regs->gpio_int.DEB,
           WHITE.c_str());
}