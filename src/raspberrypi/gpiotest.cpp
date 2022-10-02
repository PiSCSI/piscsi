
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
#include "hal/sbc_version.h"
#include "hal/systimer.h"
#include <sched.h>
#include <string>
#include "common.h"
#include "log.h"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"

#include "hal/gpiobus.h"
#include "hal/gpiobus_factory.h"



using namespace std;
using namespace spdlog;

bool InitBus()
{
	LOGTRACE("%s", __PRETTY_FUNCTION__);
	SBC_Version::Init();
	
	return true;
}

void test_timer();
void test_gpio();

//---------------------------------------------------------------------------
//
//	Pin the thread to a specific CPU
//
//---------------------------------------------------------------------------
void FixCpu(int cpu)
{
	LOGTRACE("%s", __PRETTY_FUNCTION__);
	// Get the number of CPUs
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	sched_getaffinity(0, sizeof(cpu_set_t), &cpuset);
	int cpus = CPU_COUNT(&cpuset);

	// Set the thread affinity
	if (cpu < cpus) {
		CPU_ZERO(&cpuset);
		CPU_SET(cpu, &cpuset);
		sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
	}
}

string current_log_level = "";

bool SetLogLevel(const string& log_level)
{
	if (log_level == "trace") {
		set_level(level::trace);
	}
	else if (log_level == "debug") {
		set_level(level::debug);
	}
	else if (log_level == "info") {
		set_level(level::info);
	}
	else if (log_level == "warn") {
		set_level(level::warn);
	}
	else if (log_level == "err") {
		set_level(level::err);
	}
	else if (log_level == "critical") {
		set_level(level::critical);
	}
	else if (log_level == "off") {
		set_level(level::off);
	}
	else {
		return false;
	}

	current_log_level = log_level;

	LOGINFO("Set log level to '%s'", current_log_level.c_str())

	return true;
}


void TerminationHandler(int signum)
{
	// Cleanup();

	exit(signum);
}


#include "wiringPi.h"

void blink(){

	printf("Simple Blink\n");
	wiringPiSetup();
	for(int i=0; i<20; i++){
	pinMode(i,OUTPUT);
	}

	for(int i=0; i<20; i++){
		for(int pin=0; pin<20; pin++){
			printf("Setting pin %d high\n", pin);
			digitalWrite(pin, HIGH);
			delay(100);
		}
		for(int pin=0; pin<20; pin++){
			printf("Setting pin %d low\n", pin);
			digitalWrite(pin, LOW);
			delay(100);
		}
	}



}


//---------------------------------------------------------------------------
//
//	Main processing
//
//---------------------------------------------------------------------------
int main(int argc, char* argv[])
{

	// added setvbuf to override stdout buffering, so logs are written immediately and not when the process exits.
	setvbuf(stdout, nullptr, _IONBF, 0);

	if(argc > 1){
		SetLogLevel(argv[1]);
	}else{
		SetLogLevel("trace");
	}

	// Create a thread-safe stdout logger to process the log messages
	auto logger = spdlog::stdout_color_mt("rascsi stdout logger");

	// Signal handler to detach all devices on a KILL or TERM signal
	struct sigaction termination_handler;
	termination_handler.sa_handler = TerminationHandler;
	sigemptyset(&termination_handler.sa_mask);
	termination_handler.sa_flags = 0;
	sigaction(SIGINT, &termination_handler, nullptr);
	sigaction(SIGTERM, &termination_handler, nullptr);

	if (!InitBus()) {
		return EPERM;
	}

	// blink();
	// test_timer();
	test_gpio();

}

void test_gpio(){
	// Force the system timer to initialize
	SysTimer::instance().SleepNsec(0);

	auto gpio_bus = GPIOBUS_Factory::Create();
	gpio_bus->Init(TARGET);
	printf("----------- Done with init()\n");
	printf("----------- Done with init()\n");
	printf("----------- Done with init()\n");
	printf("----------- Done with init()\n");

	for(int i = 0; i< 10; i++){
		LOGTRACE("%s Blink on", __PRETTY_FUNCTION__);
//		gpio_bus->SetSignal(gpio_bus->PIN_ACT, true);
		sleep(1);
		LOGTRACE("%s Blink off", __PRETTY_FUNCTION__);
//		gpio_bus->SetSignal(gpio_bus->PIN_ACT, false);
		sleep(1);
	}

	//gpio_bus->SetDirectionOut();
	LOGTRACE("%s Data 0x00", __PRETTY_FUNCTION__);
	gpio_bus->SetDAT(0x0);
	sleep(1);
	LOGTRACE("%s Data 0xFF", __PRETTY_FUNCTION__);
	gpio_bus->SetDAT(0xFF);
	sleep(1);
	LOGTRACE("%s Data 0xAA", __PRETTY_FUNCTION__);
	gpio_bus->SetDAT(0xAA);
	sleep(1);
	LOGTRACE("%s Data 0x55", __PRETTY_FUNCTION__);
	gpio_bus->SetDAT(0x55);
}

void test_timer(){

	uint32_t before = SysTimer::instance().GetTimerLow();
	sleep(1);
	uint32_t after = SysTimer::instance().GetTimerLow();

	LOGINFO("first sample: %d %08X", (before - after), (after - before));

	uint64_t sum = 0;
	int count = 0;
	for(int i=0; i < 100; i++){
		before = SysTimer::instance().GetTimerLow();
		usleep(1000);
		after = SysTimer::instance().GetTimerLow();
		sum += (after - before);
		count ++;
	}

	LOGINFO("usleep() Average %d", (uint32_t)(sum / count));

	sum = 0;
	count = 0;
	for(int i=0; i < 100; i++){
		before = SysTimer::instance().GetTimerLow();
		SysTimer::instance().SleepUsec(1000);
		after = SysTimer::instance().GetTimerLow();
		sum += (after - before);
		count ++;
	}
	LOGINFO("usleep() Average %d", (uint32_t)(sum / count));

	before = SysTimer::instance().GetTimerLow();
	SysTimer::instance().SleepNsec(1000000);
	after = SysTimer::instance().GetTimerLow();
	LOGINFO("SysTimer::instance().SleepNSec: %d (expected ~1000)", (uint32_t)(after - before));


}



