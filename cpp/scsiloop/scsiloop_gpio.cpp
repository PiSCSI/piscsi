//---------------------------------------------------------------------------
//
//	SCSI Target Emulator PiSCSI for Raspberry Pi
//  Loopback tester utility
//
//	Copyright (C) 2022 akuker
//
//---------------------------------------------------------------------------

#include <spdlog/spdlog.h>
#include "scsiloop/scsiloop_gpio.h"
#include "hal/gpiobus_factory.h"
#include "hal/sbc_version.h"
#include "scsiloop/scsiloop_cout.h"
#include "hal/log.h"

#if defined CONNECT_TYPE_STANDARD
#include "hal/connection_type/connection_standard.h"
#elif defined CONNECT_TYPE_FULLSPEC
#include "hal/connection_type/connection_fullspec.h"
#elif defined CONNECT_TYPE_AIBOM
#include "hal/connection_type/connection_aibom.h"
#elif defined CONNECT_TYPE_GAMERNIUM
#include "hal/connection_type/connection_gamernium.h"
#else
#error Invalid connection type or none specified
#endif

ScsiLoop_GPIO::ScsiLoop_GPIO()
{
    bus = GPIOBUS_Factory::Create(BUS::mode_e::TARGET);
    if (bus == nullptr) {
        throw bus_exception("Unable to create bus");
    }

    // At compile time, we don't know which type of board we're using. So, we'll build these at runtime.
    // TODO: This logic should be re-structured when/if other variants of SBCs are added in.
    if (SBC_Version::IsRaspberryPi()) {
        loopback_conn_table.push_back(
            loopback_connection{.this_pin = PIN_DT0, .connected_pin = PIN_ACK, .dir_ctrl_pin = PIN_DTD});
        loopback_conn_table.push_back(
            loopback_connection{.this_pin = PIN_DT1, .connected_pin = PIN_SEL, .dir_ctrl_pin = PIN_DTD});
        loopback_conn_table.push_back(
            loopback_connection{.this_pin = PIN_DT2, .connected_pin = PIN_ATN, .dir_ctrl_pin = PIN_DTD});
        loopback_conn_table.push_back(
            loopback_connection{.this_pin = PIN_DT3, .connected_pin = PIN_RST, .dir_ctrl_pin = PIN_DTD});
        loopback_conn_table.push_back(
            loopback_connection{.this_pin = PIN_DT4, .connected_pin = PIN_CD, .dir_ctrl_pin = PIN_DTD});
        loopback_conn_table.push_back(
            loopback_connection{.this_pin = PIN_DT5, .connected_pin = PIN_IO, .dir_ctrl_pin = PIN_DTD});
        loopback_conn_table.push_back(
            loopback_connection{.this_pin = PIN_DT6, .connected_pin = PIN_MSG, .dir_ctrl_pin = PIN_DTD});
        loopback_conn_table.push_back(
            loopback_connection{.this_pin = PIN_DT7, .connected_pin = PIN_REQ, .dir_ctrl_pin = PIN_DTD});
        loopback_conn_table.push_back(
            loopback_connection{.this_pin = PIN_DP, .connected_pin = PIN_BSY, .dir_ctrl_pin = PIN_DTD});
        loopback_conn_table.push_back(
            loopback_connection{.this_pin = PIN_ATN, .connected_pin = PIN_DT2, .dir_ctrl_pin = PIN_IND});
        loopback_conn_table.push_back(
            loopback_connection{.this_pin = PIN_RST, .connected_pin = PIN_DT3, .dir_ctrl_pin = PIN_IND});
        loopback_conn_table.push_back(
            loopback_connection{.this_pin = PIN_ACK, .connected_pin = PIN_DT0, .dir_ctrl_pin = PIN_IND});
        loopback_conn_table.push_back(
            loopback_connection{.this_pin = PIN_REQ, .connected_pin = PIN_DT7, .dir_ctrl_pin = PIN_TAD});
        loopback_conn_table.push_back(
            loopback_connection{.this_pin = PIN_MSG, .connected_pin = PIN_DT6, .dir_ctrl_pin = PIN_TAD});
        loopback_conn_table.push_back(
            loopback_connection{.this_pin = PIN_CD, .connected_pin = PIN_DT4, .dir_ctrl_pin = PIN_TAD});
        loopback_conn_table.push_back(
            loopback_connection{.this_pin = PIN_IO, .connected_pin = PIN_DT5, .dir_ctrl_pin = PIN_TAD});
        loopback_conn_table.push_back(
            loopback_connection{.this_pin = PIN_BSY, .connected_pin = PIN_DP, .dir_ctrl_pin = PIN_TAD});
        loopback_conn_table.push_back(
            loopback_connection{.this_pin = PIN_SEL, .connected_pin = PIN_DT1, .dir_ctrl_pin = PIN_IND});
        pin_name_lookup[PIN_DT0] = " d0";
        pin_name_lookup[PIN_DT1] = " d1";
        pin_name_lookup[PIN_DT2] = " d2";
        pin_name_lookup[PIN_DT3] = " d3";
        pin_name_lookup[PIN_DT4] = " d4";
        pin_name_lookup[PIN_DT5] = " d5";
        pin_name_lookup[PIN_DT6] = " d6";
        pin_name_lookup[PIN_DT7] = " d7";
        pin_name_lookup[PIN_DP]  = " dp";
        pin_name_lookup[PIN_ATN] = "atn";
        pin_name_lookup[PIN_RST] = "rst";
        pin_name_lookup[PIN_ACK] = "ack";
        pin_name_lookup[PIN_REQ] = "req";
        pin_name_lookup[PIN_MSG] = "msg";
        pin_name_lookup[PIN_CD]  = " cd";
        pin_name_lookup[PIN_IO]  = " io";
        pin_name_lookup[PIN_BSY] = "bsy";
        pin_name_lookup[PIN_SEL] = "sel";
        pin_name_lookup[PIN_IND] = "ind";
        pin_name_lookup[PIN_TAD] = "tad";
        pin_name_lookup[PIN_DTD] = "dtd";

        local_pin_dtd = PIN_DTD;
        local_pin_tad = PIN_TAD;
        local_pin_ind = PIN_IND;
        local_pin_ack = PIN_ACK;
        local_pin_sel = PIN_SEL;
        local_pin_atn = PIN_ATN;
        local_pin_rst = PIN_RST;
        local_pin_cd  = PIN_CD;
        local_pin_io  = PIN_IO;
        local_pin_msg = PIN_MSG;
        local_pin_req = PIN_REQ;
        local_pin_bsy = PIN_BSY;
        local_pin_dt0 = PIN_DT0;
        local_pin_dt1 = PIN_DT1;
        local_pin_dt2 = PIN_DT2;
        local_pin_dt3 = PIN_DT3;
        local_pin_dt4 = PIN_DT4;
        local_pin_dt5 = PIN_DT5;
        local_pin_dt6 = PIN_DT6;
        local_pin_dt7 = PIN_DT7;
        local_pin_dp  = PIN_DP;
    } else {
        spdlog::error("Unsupported board version: " + SBC_Version::GetAsString());
    }
}

void ScsiLoop_GPIO::Cleanup()
{
    bus->Cleanup();
}

// Set transceivers IC1 and IC2 to OUTPUT
void ScsiLoop_GPIO::set_dtd_out()
{
    LOGTRACE("%s", __PRETTY_FUNCTION__);
    bus->SetControl(local_pin_dtd, DTD_OUT);
}

// Set transceivers IC1 and IC2 to INPUT
void ScsiLoop_GPIO::set_dtd_in()
{
    LOGTRACE("%s", __PRETTY_FUNCTION__);
    bus->SetControl(local_pin_dtd, DTD_IN);
}
// Set transceiver IC4 to OUTPUT
void ScsiLoop_GPIO::set_ind_out()
{
    LOGTRACE("%s", __PRETTY_FUNCTION__);
    bus->SetControl(local_pin_ind, IND_OUT);
}
// Set transceiver IC4 to INPUT
void ScsiLoop_GPIO::set_ind_in()
{
    LOGTRACE("%s", __PRETTY_FUNCTION__);
    bus->SetControl(local_pin_ind, IND_IN);
}
// Set transceiver IC3 to OUTPUT
void ScsiLoop_GPIO::set_tad_out()
{
    LOGTRACE("%s", __PRETTY_FUNCTION__);
    bus->SetControl(local_pin_tad, TAD_OUT);
}
// Set transceiver IC3 to INPUT
void ScsiLoop_GPIO::set_tad_in()
{
    LOGTRACE("%s", __PRETTY_FUNCTION__);
    bus->SetControl(local_pin_tad, TAD_IN);
}

// Set the specified transciever to an OUTPUT. All of the other transceivers
// will be set to inputs. If a non-existent direction gpio is specified, this
// will set all of the transceivers to inputs.
void ScsiLoop_GPIO::set_output_channel(int out_gpio)
{
    LOGTRACE("%s tad: %d dtd: %d ind: %d", CONNECT_DESC.c_str(), (int)local_pin_tad, (int)local_pin_dtd,
             (int)local_pin_ind);
    if (out_gpio == local_pin_tad)
        set_tad_out();
    else
        set_tad_in();
    if (out_gpio == local_pin_dtd)
        set_dtd_out();
    else
        set_dtd_in();
    if (out_gpio == local_pin_ind)
        set_ind_out();
    else
        set_ind_in();
}

// Initialize the GPIO library, set all of the gpios associated with SCSI signals to outputs and set
// all of the direction control gpios to outputs
void ScsiLoop_GPIO::loopback_setup()
{
    LOGTRACE("%s", __PRETTY_FUNCTION__);

    for (loopback_connection cur_gpio : loopback_conn_table) {
        if (cur_gpio.this_pin == -1) {
            continue;
        }
        bus->PinConfig(cur_gpio.this_pin, GPIO_OUTPUT);
        bus->PullConfig(cur_gpio.this_pin, GPIO_PULLNONE);
    }

    // Setup direction control
    if (local_pin_ind != -1) {
        bus->PinConfig(local_pin_ind, GPIO_OUTPUT);
    }
    if (local_pin_tad != -1) {
        bus->PinConfig(local_pin_tad, GPIO_OUTPUT);
    }
    if (local_pin_dtd != -1) {
        bus->PinConfig(local_pin_dtd, GPIO_OUTPUT);
    }
}

// Main test procedure.This will execute for each of the SCSI pins to make sure its connected
// properly.
int ScsiLoop_GPIO::test_gpio_pin(loopback_connection &gpio_rec, vector<string> &error_list, bool &loopback_adapter_missing)
{
    LOGTRACE("%s", __PRETTY_FUNCTION__);

    int err_count  = 0;
    int sleep_time = 1000;

    LOGTRACE("dir ctrl pin: %d", (int)gpio_rec.dir_ctrl_pin);
    set_output_channel(gpio_rec.dir_ctrl_pin);
    usleep(sleep_time);

    // Set all GPIOs high (initialize to a known state)
    for (auto cur_gpio : loopback_conn_table) {
        // bus->SetSignal(cur_gpio.this_pin, OFF);
        bus->SetMode(cur_gpio.this_pin, GPIO_INPUT);
    }

    usleep(sleep_time);
    bus->Acquire();

    // ############################################
    // set the test gpio low
    bus->SetMode(gpio_rec.this_pin, GPIO_OUTPUT);
    bus->SetSignal(gpio_rec.this_pin, ON);

    usleep(sleep_time);

    bus->Acquire();
    // loop through all of the gpios
    for (auto cur_gpio : loopback_conn_table) {
        printf(".");
        // all of the gpios should be high except for the test gpio and the connected gpio
        LOGTRACE("calling bus->GetSignal(%d)", (int)cur_gpio.this_pin);
        auto cur_val = bus->GetSignal(cur_gpio.this_pin);
        LOGDEBUG("%d [%s] is %d", (int)cur_gpio.this_pin, pin_name_lookup.at(cur_gpio.this_pin).c_str(), (int)cur_val);

        if (cur_gpio.this_pin == gpio_rec.this_pin) {
            if (cur_val != ON) {
                error_list.push_back(
                    fmt::format("Loopback test: Test commanded GPIO {} [{}] to be low, but it did not respond",
                                cur_gpio.this_pin, pin_name_lookup.at(cur_gpio.this_pin)));
                err_count++;
            }
        } else if (cur_gpio.this_pin == gpio_rec.connected_pin) {
            if (cur_val != ON) {
                error_list.push_back(
                    fmt::format("Loopback test: GPIO {} [{}] should be driven low, but {} [{}] did not affect it",
                                cur_gpio.this_pin, pin_name_lookup.at(cur_gpio.this_pin), cur_gpio.connected_pin,
                                pin_name_lookup.at(cur_gpio.connected_pin)));

                err_count++;
            }
            else{
                loopback_adapter_missing = false;
            }
        } else {
            if (cur_val != OFF) {
                error_list.push_back(
                    fmt::format("Loopback test: GPIO {} [{}] was incorrectly pulled low, when it shouldn't be",
                                cur_gpio.this_pin, pin_name_lookup.at(cur_gpio.this_pin)));
                err_count++;
            }
        }
    }

    // ############################################
    // set the transceivers to input
    set_output_channel(-1);

    usleep(sleep_time);

    bus->Acquire();
    // # loop through all of the gpios
    for (auto cur_gpio : loopback_conn_table) {
        printf(".");
        // all of the gpios should be high except for the test gpio
        LOGTRACE("calling bus->GetSignal(%d)", (int)cur_gpio.this_pin);
        auto cur_val = bus->GetSignal(cur_gpio.this_pin);
        LOGDEBUG("%d [%s] is %d", (int)cur_gpio.this_pin, pin_name_lookup.at(cur_gpio.this_pin).c_str(), (int)cur_val);

        if (cur_gpio.this_pin == gpio_rec.this_pin) {
            if (cur_val != ON) {
                error_list.push_back(
                    fmt::format("Loopback test: Test commanded GPIO {} [{}] to be low, but it did not respond",
                                cur_gpio.this_pin, pin_name_lookup.at(cur_gpio.this_pin)));
                err_count++;
            }
        } else {
            if (cur_val != OFF) {
                error_list.push_back(
                    fmt::format("Loopback test: GPIO {} [{}] was incorrectly pulled low, when it shouldn't be",
                                cur_gpio.this_pin, pin_name_lookup.at(cur_gpio.this_pin)));
                err_count++;
            }
        }
    }

    // Set the transceiver back to output
    set_output_channel(gpio_rec.dir_ctrl_pin);
    usleep(sleep_time);

    // #############################################
    // set the test gpio high
    bus->SetMode(gpio_rec.this_pin, GPIO_OUTPUT);
    bus->SetSignal(gpio_rec.this_pin, OFF);

    usleep(sleep_time);

    bus->Acquire();
    // loop through all of the gpios
    for (auto cur_gpio : loopback_conn_table) {
        printf(".");

        auto cur_val = bus->GetSignal(cur_gpio.this_pin);
        LOGTRACE("%d [%s] is %d", (int)cur_gpio.this_pin, pin_name_lookup.at(cur_gpio.this_pin).c_str(), (int)cur_val);

        if (cur_gpio.this_pin == gpio_rec.this_pin) {
            if (cur_val != OFF) {
                error_list.push_back(
                    fmt::format("Loopback test: Test commanded GPIO {} [{}] to be high, but it did not respond",
                                cur_gpio.this_pin, pin_name_lookup.at(cur_gpio.this_pin)));
                err_count++;
            }
        } else {
            if (cur_val != OFF) {
                error_list.push_back(
                    fmt::format("Loopback test: GPIO {} [{}] was incorrectly pulled low, when it shouldn't be",
                                cur_gpio.this_pin, pin_name_lookup.at(cur_gpio.this_pin)));
                err_count++;
            }
        }
    }

    ScsiLoop_Cout::FinishTest(fmt::format("GPIO {:<3} [{}]", gpio_rec.this_pin, pin_name_lookup.at(gpio_rec.this_pin)),
                              err_count);
    return err_count;
}

int ScsiLoop_GPIO::RunLoopbackTest(vector<string> &error_list)
{
    int errors = 0;
    bool loopback_adapter_missing = true;
    LOGTRACE("%s", __PRETTY_FUNCTION__);
    loopback_setup();

    for (auto cur_gpio : loopback_conn_table) {
        ScsiLoop_Cout::StartTest(
            fmt::format("GPIO {:<3}[{}]", cur_gpio.this_pin, pin_name_lookup.at(cur_gpio.this_pin).c_str()));

        errors += test_gpio_pin(cur_gpio, error_list, loopback_adapter_missing);
    }

    if(loopback_adapter_missing){
        error_list.push_back("All of the loop-backed signals failed. Is the loopback adapter missing? (A special cable is required to use scsiloop)");
    }
    return errors;
}

void ScsiLoop_GPIO::dat_input_test_setup()
{
    LOGTRACE("%s", __PRETTY_FUNCTION__);

    for (loopback_connection cur_gpio : loopback_conn_table) {
        if (cur_gpio.this_pin == -1) {
            continue;
        }
        bus->PinConfig(cur_gpio.this_pin, GPIO_OUTPUT);
        bus->PullConfig(cur_gpio.this_pin, GPIO_PULLNONE);
    }

    bus->PinConfig(local_pin_dt0, GPIO_INPUT);
    bus->PinConfig(local_pin_dt1, GPIO_INPUT);
    bus->PinConfig(local_pin_dt2, GPIO_INPUT);
    bus->PinConfig(local_pin_dt3, GPIO_INPUT);
    bus->PinConfig(local_pin_dt4, GPIO_INPUT);
    bus->PinConfig(local_pin_dt5, GPIO_INPUT);
    bus->PinConfig(local_pin_dt6, GPIO_INPUT);
    bus->PinConfig(local_pin_dt7, GPIO_INPUT);

    set_dtd_in();
    set_tad_out();
    set_ind_out();
}

void ScsiLoop_GPIO::dat_output_test_setup()
{
    LOGTRACE("%s", __PRETTY_FUNCTION__);

    for (loopback_connection cur_gpio : loopback_conn_table) {
        if (cur_gpio.this_pin == -1) {
            continue;
        }
        bus->PinConfig(cur_gpio.this_pin, GPIO_INPUT);
        bus->PullConfig(cur_gpio.this_pin, GPIO_PULLNONE);
    }

    bus->PinConfig(local_pin_dt0, GPIO_OUTPUT);
    bus->PinConfig(local_pin_dt1, GPIO_OUTPUT);
    bus->PinConfig(local_pin_dt2, GPIO_OUTPUT);
    bus->PinConfig(local_pin_dt3, GPIO_OUTPUT);
    bus->PinConfig(local_pin_dt4, GPIO_OUTPUT);
    bus->PinConfig(local_pin_dt5, GPIO_OUTPUT);
    bus->PinConfig(local_pin_dt6, GPIO_OUTPUT);
    bus->PinConfig(local_pin_dt7, GPIO_OUTPUT);

    set_dtd_out();
    set_tad_in();
    set_ind_in();
}

void ScsiLoop_GPIO::set_dat_inputs_loop(uint8_t value)
{
    bus->SetSignal(local_pin_ack, (value >> 0) & 0x1);
    bus->SetSignal(local_pin_sel, (value >> 1) & 0x1);
    bus->SetSignal(local_pin_atn, (value >> 2) & 0x1);
    bus->SetSignal(local_pin_rst, (value >> 3) & 0x1);
    bus->SetSignal(local_pin_cd, (value >> 4) & 0x1);
    bus->SetSignal(local_pin_io, (value >> 5) & 0x1);
    bus->SetSignal(local_pin_msg, (value >> 6) & 0x1);
    bus->SetSignal(local_pin_req, (value >> 7) & 0x1);
}

uint8_t ScsiLoop_GPIO::get_dat_outputs_loop()
{
    uint8_t value = 0;
    value |= ((bus->GetSignal(local_pin_ack) & 0x1) << 0);
    value |= ((bus->GetSignal(local_pin_sel) & 0x1) << 1);
    value |= ((bus->GetSignal(local_pin_atn) & 0x1) << 2);
    value |= ((bus->GetSignal(local_pin_rst) & 0x1) << 3);
    value |= ((bus->GetSignal(local_pin_cd) & 0x1) << 4);
    value |= ((bus->GetSignal(local_pin_io) & 0x1) << 5);
    value |= ((bus->GetSignal(local_pin_msg) & 0x1) << 6);
    value |= ((bus->GetSignal(local_pin_req) & 0x1) << 7);
    return value;
}

int ScsiLoop_GPIO::RunDataInputTest(vector<string> &error_list)
{
    int err_count     = 0;
    int delay_time_us = 1000;
    dat_input_test_setup();

    ScsiLoop_Cout::StartTest("data inputs  ");

    for (uint32_t val = 0; val < UINT8_MAX; val++) {
        set_dat_inputs_loop(val);
        usleep(delay_time_us);

        bus->Acquire();
        uint8_t read_val = bus->GetDAT();

        if (read_val != (uint8_t)(val & 0xFF)) {
            error_list.push_back(fmt::format("DAT Inputs: Expected value {} but got {}", val, read_val));
            err_count++;
        }
        if ((val % 0x7) == 0) {
            printf(".");
        }
    }

    ScsiLoop_Cout::FinishTest("DAT Inputs", err_count);

    set_dat_inputs_loop(0);
    return err_count;
}

int ScsiLoop_GPIO::RunDataOutputTest(vector<string> &error_list)
{
    int err_count     = 0;
    int delay_time_us = 1000;
    dat_output_test_setup();

    ScsiLoop_Cout::StartTest("data outputs ");

    for (uint32_t val = 0; val < UINT8_MAX; val++) {
        bus->SetDAT(val);
        usleep(delay_time_us);

        bus->Acquire();
        uint8_t read_val = get_dat_outputs_loop();

        if (read_val != (uint8_t)(val & 0xFF)) {
            error_list.push_back(fmt::format("DAT Outputs: Expected value {} but got {}", val, read_val));
            err_count++;
        }
        if ((val % 0x7) == 0) {
            printf(".");
        }
    }

    ScsiLoop_Cout::FinishTest("DAT Outputs", err_count);

    return err_count;
}
