#define _FILE_OFFSET_BITS 64
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "gpiochip.h"
#include "util.h"

#define ARRAY_SIZE(_a) (sizeof(_a)/sizeof(_a[0]))

#define MAX_GPIO_CHIPS 8

typedef struct GPIO_CHIP_INSTANCE_
{
    const GPIO_CHIP_T *chip;
    const char *name;
    const char *dtnode;
    int mem_fd;
    void *priv;
    uint64_t phys_addr;
    unsigned num_gpios;
    uint32_t base;
} GPIO_CHIP_INSTANCE_T;

static unsigned num_gpio_chips;
static GPIO_CHIP_INSTANCE_T gpio_chips[MAX_GPIO_CHIPS];

static unsigned num_gpios;
static unsigned first_hdr_pin = GPIO_INVALID;
static unsigned last_hdr_pin = GPIO_INVALID;
static const char *gpio_names[MAX_GPIO_PINS];
static unsigned hdr_gpios[NUM_HDR_PINS + 1];

const char *pull_names[] = { "pn", "pd", "pu", "--" };
const char *drive_names[] = { "dl", "dh", "--" };
const char *fsel_names[] =
{
    "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7",
    "a8", "??", "??", "??", "??", "??", "??", "??",
    "ip", "op", "gp", "no"
};

void (*verbose_callback)(const char *);

static GPIO_CHIP_INSTANCE_T *gpio_create_instance(const GPIO_CHIP_T *chip,
                                                  uint64_t phys_addr,
                                                  const char *name,
                                                  const char *dtnode)
{
    GPIO_CHIP_INSTANCE_T *inst = &gpio_chips[num_gpio_chips];

    if (num_gpio_chips >= MAX_GPIO_CHIPS)
    {
        assert(0);
        return NULL;
    }

    inst->chip = chip;
    inst->name = name ? name : chip->name;
    inst->dtnode = dtnode;
    inst->phys_addr = phys_addr;
    inst->priv = NULL;
    inst->base = 0;

    inst->priv = chip->interface->gpio_create_instance(chip, dtnode);
    if (!inst->priv)
        return NULL;

    num_gpio_chips++;

    return inst;
}

static int gpio_get_interface(unsigned gpio,
                              const GPIO_CHIP_INTERFACE_T **iface_ptr,
                              void **priv, unsigned *offset)
{
    unsigned i;

    *iface_ptr = NULL;
    for (i = 0; i < num_gpio_chips; i++)
    {
        GPIO_CHIP_INSTANCE_T *inst = &gpio_chips[i];
        const GPIO_CHIP_T *chip = inst->chip;
        if (gpio >= inst->base && gpio < (inst->base + inst->num_gpios))
        {
            *iface_ptr = chip->interface;
            *priv = inst->priv;
            *offset = gpio - inst->base;
            return 0;
        }
    }
    return -1;
}

int gpio_num_is_valid(unsigned gpio)
{
    return gpio < MAX_GPIO_PINS && !!gpio_names[gpio];
}

GPIO_DIR_T gpio_get_dir(unsigned gpio)
{
    const GPIO_CHIP_INTERFACE_T *iface = NULL;
    unsigned gpio_offset;
    void *priv;

    if (gpio_get_interface(gpio, &iface, &priv, &gpio_offset) == 0)
        return iface->gpio_get_dir(priv, gpio_offset);
    return DIR_MAX;
}

void gpio_set_dir(unsigned gpio, GPIO_DIR_T dir)
{
    const GPIO_CHIP_INTERFACE_T *iface = NULL;
    unsigned gpio_offset;
    void *priv;

    if (gpio_get_interface(gpio, &iface, &priv, &gpio_offset) == 0)
        iface->gpio_set_dir(priv, gpio_offset, dir);
}

GPIO_FSEL_T gpio_get_fsel(unsigned gpio)
{
    const GPIO_CHIP_INTERFACE_T *iface = NULL;
    GPIO_FSEL_T fsel = GPIO_FSEL_MAX;
    unsigned gpio_offset;
    void *priv;

    if (gpio_get_interface(gpio, &iface, &priv, &gpio_offset) == 0)
        fsel = iface->gpio_get_fsel(priv, gpio_offset);

    if (fsel == GPIO_FSEL_GPIO)
    {
        if (gpio_get_dir(gpio) == DIR_OUTPUT)
            fsel = GPIO_FSEL_OUTPUT;
        else
            fsel = GPIO_FSEL_INPUT;
    }

    return fsel;
}

void gpio_set_fsel(unsigned gpio, const GPIO_FSEL_T func)
{
    const GPIO_CHIP_INTERFACE_T *iface = NULL;
    unsigned gpio_offset;
    void *priv;

    if (gpio_get_interface(gpio, &iface, &priv, &gpio_offset) == 0)
        iface->gpio_set_fsel(priv, gpio_offset, func);
}

void gpio_set_drive(unsigned gpio, GPIO_DRIVE_T drv)
{
    const GPIO_CHIP_INTERFACE_T *iface = NULL;
    unsigned gpio_offset;
    void *priv;

    if (gpio_get_interface(gpio, &iface, &priv, &gpio_offset) == 0)
        iface->gpio_set_drive(priv, gpio_offset, drv);
}

void gpio_set(unsigned gpio)
{
    const GPIO_CHIP_INTERFACE_T *iface = NULL;
    unsigned gpio_offset;
    void *priv;

    if (gpio_get_interface(gpio, &iface, &priv, &gpio_offset) == 0)
    {
        iface->gpio_set_drive(priv, gpio_offset, 1);
        iface->gpio_set_dir(priv, gpio_offset, DIR_OUTPUT);
    }
}

void gpio_clear(unsigned gpio)
{
    const GPIO_CHIP_INTERFACE_T *iface = NULL;
    unsigned gpio_offset;
    void *priv;

    if (gpio_get_interface(gpio, &iface, &priv, &gpio_offset) == 0)
    {
        iface->gpio_set_drive(priv, gpio_offset, 0);
        iface->gpio_set_dir(priv, gpio_offset, DIR_OUTPUT);
    }
}

int gpio_get_level(unsigned gpio)
{
    const GPIO_CHIP_INTERFACE_T *iface = NULL;
    unsigned gpio_offset;
    void *priv;

    if (gpio_get_interface(gpio, &iface, &priv, &gpio_offset) == 0)
        return iface->gpio_get_level(priv, gpio_offset);
    return 0;
}

GPIO_DRIVE_T gpio_get_drive(unsigned gpio)
{
    const GPIO_CHIP_INTERFACE_T *iface = NULL;
    unsigned gpio_offset;
    void *priv;

    if (gpio_get_interface(gpio, &iface, &priv, &gpio_offset) == 0)
        return iface->gpio_get_drive(priv, gpio_offset);
    return DRIVE_MAX;
}

GPIO_PULL_T gpio_get_pull(unsigned gpio)
{
    const GPIO_CHIP_INTERFACE_T *iface = NULL;
    unsigned gpio_offset;
    void *priv;

    if (gpio_get_interface(gpio, &iface, &priv, &gpio_offset) == 0)
        return iface->gpio_get_pull(priv, gpio_offset);
    return PULL_MAX;
}

void gpio_set_pull(unsigned gpio, GPIO_PULL_T pull)
{
    const GPIO_CHIP_INTERFACE_T *iface = NULL;
    unsigned gpio_offset;
    void *priv;

    if (gpio_get_interface(gpio, &iface, &priv, &gpio_offset) == 0)
        iface->gpio_set_pull(priv, gpio_offset, pull);
}

void gpio_get_pin_range(unsigned *first, unsigned *last)
{
    if (first_hdr_pin == GPIO_INVALID)
    {
        unsigned i;

        for (i = 0; i < num_gpio_chips; i++)
        {
            if (!strncmp(gpio_chips[i].name, "bcm2", 4) ||
                !strcmp(gpio_chips[i].name, "rp1"))
            {
                // Assume it's the standard RPi 40-pin header layout
                uint32_t base = gpio_chips[i].base;

                hdr_gpios[3] = base + 2;
                hdr_gpios[5] = base + 3;
                hdr_gpios[7] = base + 4;
                hdr_gpios[8] = base + 14;
                hdr_gpios[10] = base + 15;
                hdr_gpios[11] = base + 17;
                hdr_gpios[12] = base + 18;
                hdr_gpios[13] = base + 27;
                hdr_gpios[15] = base + 22;
                hdr_gpios[16] = base + 23;
                hdr_gpios[18] = base + 24;
                hdr_gpios[19] = base + 10;
                hdr_gpios[21] = base + 9;
                hdr_gpios[18] = base + 24;
                hdr_gpios[22] = base + 25;
                hdr_gpios[23] = base + 11;
                hdr_gpios[24] = base + 8;
                hdr_gpios[26] = base + 7;
                hdr_gpios[27] = base + 0;
                hdr_gpios[28] = base + 1;
                hdr_gpios[29] = base + 5;
                hdr_gpios[31] = base + 6;
                hdr_gpios[32] = base + 12;
                hdr_gpios[33] = base + 13;
                hdr_gpios[35] = base + 19;
                hdr_gpios[36] = base + 16;
                hdr_gpios[37] = base + 26;
                hdr_gpios[38] = base + 20;
                hdr_gpios[40] = base + 21;

                first_hdr_pin = 1;
                last_hdr_pin = 40;
                break;
            }
        }
    }
    if (first)
        *first = first_hdr_pin;
    if (last)
        *last = last_hdr_pin;
}

unsigned gpio_for_pin(int pin)
{
    if (pin >= 1 && pin <= NUM_HDR_PINS)
        return hdr_gpios[pin];
    return GPIO_INVALID;
}

int gpio_to_pin(unsigned gpio)
{
    int i;

    for (i = 1; i <= NUM_HDR_PINS; i++)
    {
        if (hdr_gpios[i] == gpio)
            return i;
    }
    return -1;
}

unsigned gpio_get_gpio_by_name(const char *name, int name_len)
{
    unsigned gpio;

    if (!name_len)
        name_len = strlen(name);
    for (gpio = 0; gpio < num_gpios; gpio++)
    {
        const char *gpio_name = gpio_names[gpio];
        const char *p;

        if (!gpio_name)
            continue;

        for (p = gpio_name; *p; )
        {
            int len = strcspn(p, "/");
            if (len == name_len && memcmp(name, p, name_len) == 0)
                return gpio;
            p += len;
            if (*p == '/')
                p++;
        }
    }

    return GPIO_INVALID;
}

const char *gpio_get_name(unsigned gpio)
{
    if (gpio < num_gpios)
        return gpio_names[gpio];
    switch (gpio)
    {
    case GPIO_INVALID:
        return "-";
    case GPIO_GND:
        return "gnd";
    case GPIO_5V:
        return "5v";
    case GPIO_3V3:
        return "3v3";
    case GPIO_1V8:
        return "1v8";
    case GPIO_OTHER:
    default:
        return "???";
    }
}

const char *gpio_get_gpio_fsel_name(unsigned gpio, GPIO_FSEL_T fsel)
{
    const GPIO_CHIP_INTERFACE_T *iface = NULL;
    unsigned gpio_offset;
    void *priv;

    if (gpio_get_interface(gpio, &iface, &priv, &gpio_offset) == 0)
        return iface->gpio_get_fsel_name(priv, gpio_offset, fsel);
    return NULL;
}

const char *gpio_get_fsel_name(GPIO_FSEL_T fsel)
{
    if ((unsigned)fsel < ARRAY_SIZE(fsel_names))
        return fsel_names[fsel];
    return NULL;
}

const char *gpio_get_pull_name(GPIO_PULL_T pull)
{
    if ((unsigned)pull < ARRAY_SIZE(pull_names))
        return pull_names[pull];
    return NULL;
}

const char *gpio_get_drive_name(GPIO_DRIVE_T drive)
{
    if ((unsigned)drive < ARRAY_SIZE(drive_names))
        return drive_names[drive];
    return NULL;
}

static const GPIO_CHIP_T *gpio_find_chip(const char *name)
{
    const GPIO_CHIP_T *chip;

    for (chip = &__start_gpiochips; name && chip < &__stop_gpiochips; chip++) {
        if (!strcmp(name, chip->name) ||
            !strcmp(name, chip->compatible))
            return chip;
    }

    return NULL;
}

int gpiolib_init(void)
{
    const GPIO_CHIP_T *chip;
    GPIO_CHIP_INSTANCE_T *inst;
    char pathbuf[FILENAME_MAX];
    char gpiomem_idx[4];
    const char *dtpath = "/proc/device-tree";
    const char *p;
    char *alias = NULL, *names, *end, *compatible;
    uint64_t phys_addr;
    size_t names_len;
    unsigned gpio_base;
    unsigned pin, i;

    for (pin = 0; pin <= NUM_HDR_PINS; pin++)
        hdr_gpios[pin] = GPIO_INVALID;

    // There is currently only one header layout
    hdr_gpios[1] = GPIO_3V3;
    hdr_gpios[17] = GPIO_3V3;
    hdr_gpios[2] = GPIO_5V;
    hdr_gpios[4] = GPIO_5V;
    hdr_gpios[6] = GPIO_GND;
    hdr_gpios[9] = GPIO_GND;
    hdr_gpios[14] = GPIO_GND;
    hdr_gpios[20] = GPIO_GND;
    hdr_gpios[25] = GPIO_GND;
    hdr_gpios[30] = GPIO_GND;
    hdr_gpios[34] = GPIO_GND;
    hdr_gpios[39] = GPIO_GND;

    if (verbose_callback)
        (*verbose_callback)("GPIO chips:\n");

    dt_set_path(dtpath);

    for (i = 0; ; i++)
    {
        sprintf(pathbuf, "gpio%d", i);
        sprintf(gpiomem_idx, "%d", i);
        alias = dt_read_prop("/aliases", pathbuf, NULL);
        if (!alias && i == 0)
        {
            alias = dt_read_prop("/aliases", "gpio", NULL);
            gpiomem_idx[0] = 0;
        }
        if (!alias)
            break;

        compatible = dt_read_prop(alias, "compatible", NULL);
        if (!compatible)
        {
            sprintf(pathbuf, "%s/..", alias);
            compatible = dt_read_prop(pathbuf, "compatible", NULL);
        }

        chip = gpio_find_chip(compatible);
        dt_free(compatible);

        if (!chip)
        {
            // Skip the unknown gpio chip
            dt_free(alias);
            continue;
        }

        phys_addr = dt_parse_addr(alias);
        if (phys_addr == INVALID_ADDRESS)
        {
            dt_free(alias);
            return -1;
        }

        inst = gpio_create_instance(chip, phys_addr, NULL, alias);
        if (!inst)
        {
            dt_free(alias);
            return -1;
        }

        sprintf(pathbuf, "/dev/gpiomem%s", gpiomem_idx);
        inst->mem_fd = open(pathbuf, O_RDWR|O_SYNC);
    }

    gpio_base = 0;
    num_gpios = 0;

    for (i = 0; i < num_gpio_chips; i++)
    {
        unsigned gpio;

        inst = &gpio_chips[i];
        inst->base = gpio_base;
        chip = inst->chip;
        inst->num_gpios = chip->interface->gpio_count(inst->priv);

        if (verbose_callback)
        {
            char msg_buf[100];
            snprintf(msg_buf, sizeof(msg_buf), "  %" PRIx64 ": %s (%d gpios)\n",
                     inst->phys_addr, chip->name, inst->num_gpios);
            (*verbose_callback)(msg_buf);
        }

        if (!inst->num_gpios)
            continue;
        num_gpios = gpio_base + inst->num_gpios;
        gpio_base = ROUND_UP(num_gpios, 100);

        if (num_gpios > MAX_GPIO_PINS)
            return -1;

        names = dt_read_prop(inst->dtnode, "gpio-line-names", &names_len);
        end = names + names_len;

        for (gpio = 0, p = names; gpio < inst->num_gpios; gpio++)
        {
            static const char *names[] = { "ID_SD", "ID_SC" };
            static const int pins[] = { 27, 28 };
            char name_buf[32];
            const char *name = "-", *arch_name;

            if (p && p < end)
            {
                name = p;
                p += strlen(p) + 1;
                if (sscanf(name, "PIN%u", &pin) != 1 ||
                    pin < 1 || pin > NUM_HDR_PINS)
                {
                    unsigned i;
                    pin = 0;
                    for (i = 0; i < ARRAY_SIZE(names); i++)
                    {
                        if (strcmp(name, names[i]) == 0)
                        {
                            pin = pins[i];
                            break;
                        }
                    }
                }

                if (pin >= 1)
                {
                    hdr_gpios[pin] = inst->base + gpio;
                    if ((pin < first_hdr_pin) || (first_hdr_pin == GPIO_INVALID))
                        first_hdr_pin = pin;
                    if ((pin > last_hdr_pin) || (last_hdr_pin == GPIO_INVALID))
                        last_hdr_pin = pin;
                }
            }

            arch_name = chip->interface->gpio_get_name(inst->priv, gpio);
            if (!name[0] || !arch_name)
            {
                gpio_names[inst->base + gpio] = NULL;
                continue;
            }

            if (strcmp(name, arch_name) != 0)
            {
                if (strcmp(name, "-") == 0)
                {
                    name = arch_name;
                }
                else
                {
                    if (snprintf(name_buf, sizeof(name_buf), "%s/%s",
                                 name, arch_name) >= (int)sizeof(name_buf))
                        name_buf[sizeof(name_buf) - 1] = '\0';
                    name = name_buf;
                }
            }

            gpio_names[inst->base + gpio] = strdup(name);
        }

        dt_free(names);
    }

    // On a board with PINs, show pins 1-40
    if (first_hdr_pin == 3)
        first_hdr_pin = 1;

    return (int)num_gpios;
}


int gpiolib_init_by_name(const char *name)
{
    const GPIO_CHIP_T *chip;
    GPIO_CHIP_INSTANCE_T *inst;
    unsigned gpio;
    int pin;

    for (pin = 0; pin <= NUM_HDR_PINS; pin++)
        hdr_gpios[pin] = GPIO_INVALID;

    if (verbose_callback)
        (*verbose_callback)("GPIO chips:\n");

    chip = gpio_find_chip(name);
    if (!chip)
        return 0;

    inst = gpio_create_instance(chip, 0, NULL, NULL);
    if (!inst)
        return -1;

    inst->num_gpios = chip->interface->gpio_count(inst->priv);

    num_gpios = inst->num_gpios;

    for (gpio = 0; gpio < inst->num_gpios; gpio++)
    {
        const char *name = chip->interface->gpio_get_name(inst->priv, gpio);
        if (!name)
        {
            gpio_names[inst->base + gpio] = NULL;
            continue;
        }

        gpio_names[gpio] = strdup(name);
    }

    if (inst->num_gpios && verbose_callback)
    {
        char msg_buf[100];
        snprintf(msg_buf, sizeof(msg_buf), "  %s (%d gpios)\n",
                 chip->name, inst->num_gpios);
        (*verbose_callback)(msg_buf);
    }

    return (int)num_gpios;
}

int gpiolib_mmap(void)
{
    int pagesize = getpagesize();
    int mem_fd = -1;
    unsigned i;

    for (i = 0; i < num_gpio_chips; i++)
    {
        GPIO_CHIP_INSTANCE_T *inst;
        const GPIO_CHIP_T *chip;
        void *gpio_map;
        void *new_priv;
        unsigned align;

        inst = &gpio_chips[i];
        chip = inst->chip;

        align = inst->phys_addr & (pagesize - 1);

        if (inst->mem_fd >= 0)
        {
            gpio_map = mmap(
                NULL,                   /* Any address in our space will do */
                chip->size + align,     /* Map length */
                PROT_READ | PROT_WRITE, /* Enable reading & writing */
                MAP_SHARED,             /* Shared with other processes */
                inst->mem_fd,           /* File to map */
                0                       /* Offset to GPIO peripheral */
                );
        }
        else
        {
            if (mem_fd < 0)
            {
                mem_fd = open("/dev/mem", O_RDWR|O_SYNC);
                if (mem_fd < 0)
                    return errno;
            }
            gpio_map = mmap(
                NULL,                   /* Any address in our space will do */
                chip->size + align,     /* Map length */
                PROT_READ | PROT_WRITE, /* Enable reading & writing */
                MAP_SHARED,             /* Shared with other processes */
                mem_fd,                 /* File to map */
                inst->phys_addr - align /* Offset to GPIO peripheral */
                );
        }

        if (gpio_map == MAP_FAILED)
            return errno;

        new_priv = chip->interface->gpio_probe_instance(inst->priv,
                                                        (void *)((char *)gpio_map + align));
        if (!new_priv)
            return -1;
        inst->priv = new_priv;
    }

    return 0;
}

void gpiolib_set_verbose(void (*callback)(const char *))
{
    verbose_callback = callback;
}
