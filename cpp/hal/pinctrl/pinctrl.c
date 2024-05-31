#define _GNU_SOURCE
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/time.h>

#include "gpiolib.h"

#define ARRAY_SIZE(_a) (sizeof(_a)/sizeof(_a[0]))

const char *program_name = "pinctrl";

static int pin_mode = 0;
static int verbose_mode = 0;
static unsigned num_gpios;

struct poll_gpio_state {
    unsigned int num;
    unsigned int gpio;
    const char *name;
    int level;
};

int num_poll_gpios;
struct poll_gpio_state *poll_gpios;

static void print_gpio_alts_info(unsigned gpio)
{
    const char *name;
    unsigned int num = gpio;
    int fsel;

    if (pin_mode)
        gpio = gpio_for_pin(num);
    if (!gpio_num_is_valid(gpio))
        return;

    name = gpio_get_name(gpio);
    if (pin_mode && strchr(name, '/'))
        name = strchr(name, '/') + 1;

    printf("%d", num);
    if (name[0])
        printf(", %s", name);
    for (fsel = GPIO_FSEL_FUNC0; ; fsel++)
    {
        name = gpio_get_gpio_fsel_name(gpio, fsel);
        if (!name)
            break;
        printf(", %s", name);
    }
    printf("\n");
}

static void usage()
{
    const char *name = program_name;

    printf("\n");
    printf("WARNING! %s set writes directly to the GPIO control registers\n", name);
    printf("ignoring whatever else may be using them (such as Linux drivers) -\n");
    printf("it is designed as a debug tool, only use it if you know what you\n");
    printf("are doing and at your own risk!\n");
    printf("\n");
    printf("Running %s with the help argument prints this help.\n", name);
    printf("%s can get and print the state of a GPIO (or all GPIOs)\n", name);
    printf("and can be used to set the function, pulls and value of a GPIO.\n");
    printf("%s must be run as root.\n", name);
    printf("Use:\n");
    printf("  %s [-p] [-v] get [GPIO]\n", name);
    printf("OR\n");
    printf("  %s [-p] [-v] [-e] set <GPIO> [options]\n", name);
    printf("OR\n");
    printf("  %s [-p] [-v] poll <GPIO>\n", name);
    printf("OR\n");
    printf("  %s [-p] [-v] funcs [GPIO]\n", name);
    printf("OR\n");
    printf("  %s [-p] [-v] lev [GPIO]\n", name);
    printf("OR\n");
    printf("  %s -c <chip> [funcs] [GPIO]\n", name);
    printf("\n");
    printf("GPIO is a comma-separated list of GPIO names, numbers or ranges (without\n");
    printf("spaces), e.g. 4 or 18-21 or BT_ON,9-11\n");
    printf("\n");
    printf("Note that omitting [GPIO] from \"%s get\" prints all GPIOs.\n", name);
    printf("If the -p option is given, GPIO numbers are replaced by pin numbers on the\n");
    printf("40-way header. If the -v option is given, the output is more verbose. Including\n");
    printf("the -e option in a \"set\" causes pinctrl to echo back the new pin states.\n");
    printf("%s funcs will dump all the possible GPIO alt functions in CSV format\n", name);
    printf("or if [GPIO] is specified the alternate funcs just for that specific GPIO.\n");
    printf("The -c option allows the alt functions (and only the alt function) for a named\n");
    printf("chip to be displayed, even if that chip is not present in the current system.\n");
    printf("\n");
    printf("Valid [options] for %s set are:\n", name);
    printf("  ip      set GPIO as input\n");
    printf("  op      set GPIO as output\n");
    printf("  a0-a8   set GPIO to alt function in the range 0 to 8 (range varies by model)\n");
    printf("  no      set GPIO to no function (NONE)\n");
    printf("  pu      set GPIO in-pad pull up\n");
    printf("  pd      set GPIO pin-pad pull down\n");
    printf("  pn      set GPIO pull none (no pull)\n");
    printf("  dh      set GPIO to drive high (1) level (only valid if set to be an output)\n");
    printf("  dl      set GPIO to drive low (0) level (only valid if set to be an output)\n");
    printf("Examples:\n");
    printf("  %s get              Prints state of all GPIOs one per line\n", name);
    printf("  %s get 10           Prints state of GPIO10\n", name);
    printf("  %s get 10,11        Prints state of GPIO10 and GPIO11\n", name);
    printf("  %s set 10 a2        Set GPIO10 to fsel 2 function (nand_wen_clk)\n", name);
    printf("  %s -e set 10 pu     Enable GPIO10 ~50k in-pad pull up, echoing the result\n", name);
    printf("  %s set 10 pd        Enable GPIO10 ~50k in-pad pull down\n", name);
    printf("  %s set 10 op        Set GPIO10 to be an output\n", name);
    printf("  %s set 10 dl        Set GPIO10 to output low/zero (must already be set as an output)\n", name);
    printf("  %s set 10 ip pd     Set GPIO10 to input with pull down\n", name);
    printf("  %s set 35 a1 pu     Set GPIO35 to fsel 1 (jtag_2_clk) with pull up\n", name);
    printf("  %s set 20 op pn dh  Set GPIO20 to output with no pull and driving high\n", name);
    printf("  %s lev 4            Prints the level (1 or 0) of GPIO4\n", name);
    printf("  %s -c bcm2835 9-11  Display the alt functions for GPIOs 9-11 on bcm2835\n", name);
}

static int do_gpio_get(unsigned int gpio)
{
    unsigned int num = gpio;
    const char *name;
    int fsel;
    int level;

    if (pin_mode)
    {
        gpio = gpio_for_pin(num);
        switch (gpio)
        {
        case GPIO_INVALID:
        case GPIO_GND:
        case GPIO_5V:
        case GPIO_3V3:
        case GPIO_1V8:
        case GPIO_OTHER:
            name = gpio_get_name(gpio);
            printf("%2d: %s\n", num, name);
            return 0;
        }
    }

    if (!gpio_num_is_valid(gpio))
        return 1;

    fsel = gpio_get_fsel(gpio);
    printf("%2d: %2s ", num, gpio_get_fsel_name(fsel));
    if (fsel == GPIO_FSEL_OUTPUT)
        printf("%s", gpio_get_drive_name(gpio_get_drive(gpio)));
    else
        printf("  ");

    name = gpio_get_name(gpio);
    if (pin_mode && strchr(name, '/'))
        name = strchr(name, '/') + 1;

    level = gpio_get_level(gpio);

    printf(" %s | %s // %s%s%s\n",
           gpio_get_pull_name(gpio_get_pull(gpio)),
           (level == 1) ? "hi" : (level == 0) ? "lo" : "--",
           name ? name : "",
           name ? " = " : "",
           gpio_get_gpio_fsel_name(gpio, fsel));

    return 0;
}

static int do_gpio_set(unsigned int gpio, int fsparam, int drive, int pull)
{
    unsigned int num = gpio;

    if (pin_mode)
    {
        gpio = gpio_for_pin(num);
        if (gpio >= num_gpios)
        {
            printf("Pin %d cannot be set\n", num);
            return 1;
        }
    }

    if (!gpio_num_is_valid(gpio))
        return 1;

    if (fsparam != GPIO_FSEL_MAX)
        gpio_set_fsel(gpio, fsparam);
    else
        fsparam = gpio_get_fsel(gpio);

    if (drive != DRIVE_MAX)
    {
        if (fsparam == GPIO_FSEL_OUTPUT)
        {
            gpio_set_drive(gpio, drive);
        }
        else
        {
            printf("Can't set pin value, not an output\n");
            return 1;
        }
    }

    if (pull != PULL_MAX)
        gpio_set_pull(gpio, pull);

    return 0;
}

static int do_gpio_level(unsigned int gpio)
{
    unsigned int num = gpio;
    int level;

    if (pin_mode)
    {
        gpio = gpio_for_pin(num);
        switch (gpio)
        {
        case GPIO_INVALID:
            return 1;
        case GPIO_GND:
            printf("G");
            return 0;
        case GPIO_5V:
            printf("5");
            return 0;
        case GPIO_3V3:
            printf("3");
            return 0;
        case GPIO_1V8:
            printf("8");
            return 0;
        case GPIO_OTHER:
            printf("?");
            return 0;
        }
    }

    if (!gpio_num_is_valid(gpio))
        return 1;

    level = gpio_get_level(gpio);
    if (level >= 0)
        printf("%d", level);
    else
        printf("-");

    return 0;
}

static int do_gpio_poll_add(unsigned int gpio)
{
    struct poll_gpio_state *new_gpio;
    unsigned int num = gpio;

    if (pin_mode)
        gpio = gpio_for_pin(num);

    if (!gpio_num_is_valid(gpio))
        return 1;

    poll_gpios = reallocarray(poll_gpios, num_poll_gpios + 1,
                              sizeof(*poll_gpios));
    new_gpio = &poll_gpios[num_poll_gpios];
    new_gpio->num = num;
    new_gpio->gpio = gpio;
    new_gpio->name = gpio_get_name(gpio);
    new_gpio->level = -1; /* Unknown */
    num_poll_gpios++;

    return 0;
}

static void do_gpio_poll(void)
{
    unsigned int idle_count = 0;
    struct timeval idle_start;
    int i;

    while (num_poll_gpios)
    {
        int changed = 0;
        for (i = 0; i < num_poll_gpios; i++)
        {
            struct poll_gpio_state *state = &poll_gpios[i];
            int level = gpio_get_level(state->gpio);
            if (level != state->level)
            {
                if (idle_count)
                {
                    struct timeval now;
                    uint64_t interval_us;

                    gettimeofday(&now, NULL);
                    interval_us =
                        (uint64_t)(now.tv_sec - idle_start.tv_sec) * 1000000 +
                        (now.tv_usec - idle_start.tv_usec);
                    printf("+%" PRIu64 "us\n", interval_us);
                    idle_count = 0;
                }
                printf("%2d: %s // %s\n", state->num, level ? "hi" : "lo", state->name);
                state->level = level;
                changed = 1;
                fflush(stdout);
            }
        }
        if (!changed)
        {
            if (!idle_count)
                gettimeofday(&idle_start, NULL);
            idle_count++;
        }
    }
}

static void verbose_callback(const char *msg)
{
    printf("%s", msg);
}

int main(int argc, char *argv[])
{
    int ret;

    /* arg parsing */

    const char *named_chip = NULL;

    int set = 0;
    int get = 0;
    int level = 0;
    int poll = 0;
    int funcs = 0;
    int echo = 0;
    int pull = PULL_MAX;
    int infer_cmd = 0;
    int fsparam = GPIO_FSEL_MAX;
    int drive = DRIVE_MAX;
    uint32_t gpiomask[(MAX_GPIO_PINS + 31)/32] = { 0 };
    unsigned start_pin = GPIO_INVALID, end_pin, pin;
    int first_pin = 1;
    int i;

    argv++;
    argc--;

    while (argc && (argv[0][0] == '-'))
    {
        const char *arg = *(argv++);
        argc--;

        if (strcmp(arg, "-h") == 0)
        {
            usage();
            return 0;
        }
        else if (strcmp(arg, "-p") == 0)
        {
            pin_mode = 1;
        }
        else if (strcmp(arg, "-e") == 0)
        {
            echo = 1;
        }
        else if (strcmp(arg, "-v") == 0)
        {
            verbose_mode = 1;
        }
        else if (strcmp(arg, "-c") == 0)
        {
            if (!argc)
            {
                printf("* chip name expected - use 'pinctrl -h' for help\n");
                return -1;
            }
            named_chip = *(argv++);
            argc--;
        }
        else
        {
            printf("Unknown option '%s' - try \"%s help\"\n",
                   arg, program_name);
            exit(1);
        }
    }

    if (verbose_mode)
        gpiolib_set_verbose(&verbose_callback);

    if (named_chip)
        ret = gpiolib_init_by_name(named_chip);
    else
        ret = gpiolib_init();

    if (ret < 0)
    {
        printf("Failed to initialise gpiolib - %d\n", ret);
        return -1;
    }

    num_gpios = ret;
    if (!num_gpios)
    {
        printf("No GPIO chips found\n");
        return -1;
    }

    if (argc)
    {
        const char *cmd = *(argv++);
        argc--;

        if (strcmp(cmd, "help") == 0)
        {
            usage();
            return 0;
        }

        get = strcmp(cmd, "get") == 0;
        set = strcmp(cmd, "set") == 0;
        level = strcmp(cmd, "level") == 0 || strcmp(cmd, "lev") == 0;
        poll = strcmp(cmd, "poll") == 0;
        funcs = strcmp(cmd, "funcs") == 0;

        if (!set && !get && !level && !poll && !funcs)
        {
            /* Back up in case we can decode this as a pin */
            argv--;
            argc++;
            infer_cmd = 1;
        }
    }
    else if (named_chip)
    {
        funcs = 1;
    }
    else
    {
        get = 1;
    }

    if (pin_mode)
    {
       gpio_get_pin_range(&start_pin, &end_pin);
       if (start_pin == GPIO_INVALID)
       {
           printf("No PIN numbers declared in DT - pin mode disabled\n");
           pin_mode = 0;
       }
    }

    if (start_pin == GPIO_INVALID)
    {
        start_pin = 0;
        end_pin = num_gpios - 1;
    }

    if (argc) /* expect pin number/name(s) next */
    {
        char *p = *(argv++);
        argc--;

        while (p)
        {
            unsigned gpio, gpio2;
            int len, len2;
            len = strcspn(p, "-,");
            ret = sscanf(p, "%u%n", &gpio, &len2);
            if (ret == 1 && len == len2 && gpio >= num_gpios)
                break;
            else if (ret != 1 || len != len2)
            {
                gpio = gpio_get_gpio_by_name(p, len);
                if (gpio == GPIO_INVALID)
                    break;
                if (pin_mode)
                {
                    int pin = gpio_to_pin(gpio);
                    if (pin < 0)
                    {
                        printf("Signal \"%*s\" is not on a header pin\n",
                               len, p);
                        return 1;
                    }
                    gpio = (unsigned)pin;
                }
            }
            p += len;

            if (*p == '\0' && argc && (argv[0][0] == '-' || argv[0][0] == ','))
            {
                p = *(argv++);
                argc--;
            }

            if (*p == '-')
            {
                p++;
                len = strcspn(p, "-,");
                ret = sscanf(p, "%u%n", &gpio2, &len2);
                if (ret == 1 && len == len2 && gpio2 >= num_gpios)
                    break;
                else if (ret != 1 || len != len2)
                {
                    gpio2 = gpio_get_gpio_by_name(p, len);
                    if (gpio2 == GPIO_INVALID)
                        break;
                    if (pin_mode)
                    {
                        int pin = gpio_to_pin(gpio2);
                        if (pin < 0)
                        {
                            printf("Signal \"%*s\" is not on a header pin\n",
                                   len, p);
                            return 1;
                        }
                        gpio2 = (unsigned)pin;
                    }
                }
                if (gpio2 < gpio)
                {
                    int tmp = gpio2;
                    gpio2 = gpio;
                    gpio = tmp;
                }
                p += len;
            }
            else
            {
                gpio2 = gpio;
            }
            while (gpio <= gpio2)
            {
                gpiomask[gpio/32] |= (1 << (gpio % 32));
                gpio++;
            }
            if (*p == '\0' && argc && argv[0][0] == ',')
            {
                p = *(argv++);
                argc--;
            }
            if (*p == '\0')
            {
                p = NULL;
            }
            else
            {
                if (*p != ',')
                    break;
                p++;
            }
        }
        if (p)
        {
            if (infer_cmd && p == argv[-1])
                printf("Unknown command \"%s\"\n", p);
            else
                printf("Unknown GPIO \"%s\"\n", p);
            return 1;
        }
    }
    else if (set)
    {
        printf("Need GPIO number to set\n");
        return 1;
    }
    else if (poll)
    {
        printf("Need GPIO number to poll\n");
        return 1;
    }

    if (set && !argc)
    {
        printf("Need a function or pull to set\n");
        return 1;
    }

    if ((get || funcs) && argc)
    {
        printf("Too many arguments\n");
        return 1;
    }

    if (infer_cmd)
    {
        if (named_chip)
            funcs = 1;
        else if (argc)
            set = 1;
        else
            get = 1;
    }

    /* parse remaining args */
    while (argc)
    {
        const char *arg = *(argv++);
        argc--;

        if (strcmp(arg, "dh") == 0)
            drive = DRIVE_HIGH;
        else if (strcmp(arg, "dl") == 0)
            drive = DRIVE_LOW;
        else if (strcmp(arg, "gp") == 0)
            fsparam = GPIO_FSEL_GPIO;
        else if (strcmp(arg, "ip") == 0)
            fsparam = GPIO_FSEL_INPUT;
        else if (strcmp(arg, "op") == 0)
            fsparam = GPIO_FSEL_OUTPUT;
        else if (strcmp(arg, "no") == 0)
            fsparam = GPIO_FSEL_NONE;
        else if (strcmp(arg, "a0") == 0)
            fsparam = GPIO_FSEL_FUNC0;
        else if (strcmp(arg, "a1") == 0)
            fsparam = GPIO_FSEL_FUNC1;
        else if (strcmp(arg, "a2") == 0)
            fsparam = GPIO_FSEL_FUNC2;
        else if (strcmp(arg, "a3") == 0)
            fsparam = GPIO_FSEL_FUNC3;
        else if (strcmp(arg, "a4") == 0)
            fsparam = GPIO_FSEL_FUNC4;
        else if (strcmp(arg, "a5") == 0)
            fsparam = GPIO_FSEL_FUNC5;
        else if (strcmp(arg, "a6") == 0)
            fsparam = GPIO_FSEL_FUNC6;
        else if (strcmp(arg, "a7") == 0)
            fsparam = GPIO_FSEL_FUNC7;
        else if (strcmp(arg, "a8") == 0)
            fsparam = GPIO_FSEL_FUNC8;
        else if (strcmp(arg, "pu") == 0)
            pull = PULL_UP;
        else if (strcmp(arg, "pd") == 0)
            pull = PULL_DOWN;
        else if (strcmp(arg, "pn") == 0)
            pull = PULL_NONE;
        else
        {
            printf("Unknown argument \"%s\"\n", arg);
            return 1;
        }
    }

    for (i = ARRAY_SIZE(gpiomask) - 1; i >= 0; i--)
    {
        if (gpiomask[i])
            break;
    }
    if (i < 0)
        memset(gpiomask, 0xff, sizeof(gpiomask));

    if (!funcs)
    {
        ret = gpiolib_mmap();
        if (ret)
        {
            if (ret == EACCES && geteuid())
                printf("Must be root\n");
            else
                printf("Failed to mmap gpiolib - %s\n", strerror(ret));
            return -1;
        }
    }

    for (pin = start_pin; pin < end_pin + 1; pin++)
    {
        if (!(gpiomask[pin / 32] & (1 << (pin % 32))))
            continue;

        if (get)
            do_gpio_get(pin);
        if (set)
            do_gpio_set(pin, fsparam, drive, pull);
        if (level)
        {
            if (!first_pin)
                printf(" ");
            do_gpio_level(pin);
            first_pin = 0;
        }
        if (poll)
            do_gpio_poll_add(pin);
        if (funcs)
            print_gpio_alts_info(pin);
    }

    if (level)
        printf("\n");

    if (set && echo)
    {
        for (pin = start_pin; pin < end_pin + 1; pin++)
        {
            if (!(gpiomask[pin / 32] & (1 << (pin % 32))))
                continue;

            do_gpio_get(pin);
        }
    }

    if (poll)
            do_gpio_poll();

    return 0;
}
