
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

// #define PERIPHERAL_BASE 0x20000000   // For Pi 1 and 2
//#define PERIPHERAL_BASE 0x3F000000      // For Pi 3
#define PERIPHERAL_BASE 0xfe000000      // For PI 4
#define SYSTEM_TIMER_OFFSET 0x3000


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

// Get access to the System Timer registers in user memory space.
system_timer_t * get_system_timer() {
    void *system_timer;
    int  fd;

    if ((fd = open("/dev/mem", O_RDWR | O_SYNC) ) < 0) {
        printf("1 can't open /dev/mem \n");
        exit(-1);
    }

    system_timer = mmap(
        NULL,
        4096,
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        fd,
        ST_BASE
    );

    close(fd);

    if (system_timer == MAP_FAILED) {
        printf("mmap error %d\n", (int)system_timer);  // errno also set!
        exit(-1);
    }
    return (system_timer_t*)system_timer;
}

int main(int argc, char **argv) {
    volatile system_timer_t* system_timer = get_system_timer();
    uint32_t t0, t1 = 0;

    uint32_t delay = 2;

    while (1) {
        t0 = system_timer->counter_low;
        while((system_timer->counter_low - t0) < delay)
        // usleep(100);
        t1 = system_timer->counter_low;
        printf ("Elaspsed = %d\n", t1 - t0);
        printf ("Conter high = %d\n", system_timer->counter_high);
        t0 = t1;
        sleep(1);
    }
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