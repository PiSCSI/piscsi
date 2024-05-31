#ifndef GPIOCHIP_H
#define GPIOCHIP_H

#include "gpiolib.h"

#define DECLARE_GPIO_CHIP(name, compatible, iface, size, data) \
    GPIO_CHIP_T name ## _chip __attribute__ ((section ("gpiochips"))) __attribute__ ((used)) = \
    { #name, compatible, iface, size, data }

typedef struct GPIO_CHIP_INTERFACE_ GPIO_CHIP_INTERFACE_T;

typedef struct GPIO_CHIP_
{
    const char *name;
    const char *compatible;
    const GPIO_CHIP_INTERFACE_T *interface;
    int size;
    uintptr_t data;
} GPIO_CHIP_T;

struct GPIO_CHIP_INTERFACE_
{
    void * (*gpio_create_instance)(const GPIO_CHIP_T *chip, const char *dtnode);
    int (*gpio_count)(void *priv);
    void * (*gpio_probe_instance)(void *priv, volatile uint32_t *base);
    GPIO_FSEL_T (*gpio_get_fsel)(void *priv, uint32_t gpio);
    void (*gpio_set_fsel)(void *priv, uint32_t gpio, const GPIO_FSEL_T func);
    void (*gpio_set_drive)(void *priv, uint32_t gpio, GPIO_DRIVE_T drv);
    void (*gpio_set_dir)(void *priv, uint32_t gpio, GPIO_DIR_T dir);
    GPIO_DIR_T (*gpio_get_dir)(void *priv, uint32_t gpio);
    int (*gpio_get_level)(void *priv, uint32_t gpio);  /* The actual level observed */
    GPIO_DRIVE_T (*gpio_get_drive)(void *priv, uint32_t gpio);  /* What it is being driven as */
    GPIO_PULL_T (*gpio_get_pull)(void *priv, uint32_t gpio);
    void (*gpio_set_pull)(void *priv, uint32_t gpio, GPIO_PULL_T pull);
    const char * (*gpio_get_name)(void *priv, uint32_t gpio);
    const char * (*gpio_get_fsel_name)(void *priv, uint32_t gpio, GPIO_FSEL_T fsel);
};

extern const GPIO_CHIP_T __start_gpiochips;
extern const GPIO_CHIP_T __stop_gpiochips;

#endif
