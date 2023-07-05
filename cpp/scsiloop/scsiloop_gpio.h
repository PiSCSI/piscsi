//---------------------------------------------------------------------------
//
//	SCSI Target Emulator PiSCSI for Raspberry Pi
//  Loopback tester utility
//
//	Copyright (C) 2022 akuker
//
//---------------------------------------------------------------------------

#include <map>
#include <string>
#include <vector>

#include "hal/gpiobus.h"

using namespace std;

class ScsiLoop_GPIO
{
  public:
    ScsiLoop_GPIO();
    ~ScsiLoop_GPIO() = default;
    int RunLoopbackTest(vector<string> &error_list);
    int RunDataInputTest(vector<string> &error_list);
    int RunDataOutputTest(vector<string> &error_list);
    void Cleanup();

  private:
    struct loopback_connections_struct {
        int this_pin;
        int connected_pin;
        int dir_ctrl_pin;
    };
    using loopback_connection = loopback_connections_struct;

    std::map<int, string> pin_name_lookup;
    std::vector<loopback_connection> loopback_conn_table;

    void set_dtd_out();
    void set_dtd_in();
    void set_ind_out();
    void set_ind_in();
    void set_tad_out();
    void set_tad_in();
    void set_output_channel(int out_gpio);
    void loopback_setup();

    int test_gpio_pin(loopback_connection &gpio_rec, vector<string> &error_list, bool &loopback_adapter_missing);

    void set_dat_inputs_loop(uint8_t value);

    void dat_input_test_setup();
    void dat_output_test_setup();

    uint8_t get_dat_outputs_loop();

    int local_pin_dtd = -1;
    int local_pin_tad = -1;
    int local_pin_ind = -1;
    int local_pin_ack = -1;
    int local_pin_sel = -1;
    int local_pin_atn = -1;
    int local_pin_rst = -1;
    int local_pin_cd  = -1;
    int local_pin_io  = -1;
    int local_pin_msg = -1;
    int local_pin_req = -1;
    int local_pin_bsy = -1;

    int local_pin_dt0 = -1;
    int local_pin_dt1 = -1;
    int local_pin_dt2 = -1;
    int local_pin_dt3 = -1;
    int local_pin_dt4 = -1;
    int local_pin_dt5 = -1;
    int local_pin_dt6 = -1;
    int local_pin_dt7 = -1;
    int local_pin_dp  = -1;

    shared_ptr<BUS> bus;
};
