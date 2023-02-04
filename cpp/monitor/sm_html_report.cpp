//---------------------------------------------------------------------------
//
//	SCSI Target Emulator PiSCSI
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//  Copyright (C) 2021-2022 akuker
//
//---------------------------------------------------------------------------

#include "shared/log.h"
#include "shared/piscsi_version.h"
#include "sm_reports.h"
#include <iostream>
#include <fstream>

using namespace std;

const static string html_header = R"(
<html>
<head>
<style>
table, th, td {
  border: 1px solid black;
  border-collapse: collapse;
}
h1 {
  text-align: center;
}

h2 {
  text-align: center;
}

h3 {
  text-align: center;
}
pre {
  text-align: center;
}
.collapsible {
  background-color: #777;
  color: white;
  cursor: pointer;
  width: 100%;
  border: none;
  text-align: left;
  outline: none;
}

.active, .collapsible:hover {
  background-color: #555;
}

.content {
  padding: 0;
  display: none;
  overflow: hidden;
  background-color: #f1f1f1;
}
</style>
</head>
)";

static void print_copyright_info(ofstream& html_fp)
{
    html_fp << "<table>" << endl \
            << "<h1>PiSCSI scsimon Capture Tool</h1>" << endl \
            << "<pre>Version " << piscsi_get_version_string() \
            << __DATE__ << " " << __TIME__ << endl \
            << "Copyright (C) 2016-2020 GIMONS" << endl \
            << "Copyright (C) 2020-2021 Contributors to the PiSCSI project" << endl \
            << "</pre></table>" << endl \
            << "<br>" << endl;
}

static const string html_footer = R"(
</table>
<script>
var coll = document.getElementsByClassName("collapsible");
var i;

for (i = 0; i < coll.length; i++) {
  coll[i].addEventListener("click", function() {
    this.classList.toggle("active");
    var content = this.nextElementSibling;
    if (content.style.display === "block") {
      content.style.display = "none";
    } else {
      content.style.display = "block";
    }
  });
}
</script>
</html>
)";


static void print_html_data(ofstream& html_fp, const vector<shared_ptr<DataSample>> &data_capture_array)
{
    shared_ptr<DataSample> data;
    bool prev_data_valid = false;
    bool curr_data_valid;
    uint32_t selected_id = 0;
    phase_t prev_phase = phase_t::busfree;
    bool close_row = false;
    int data_space_count = 0;
    bool collapsible_div_active = false;
    bool button_active = false;

    html_fp << "<table>" << endl;

    for (auto data: data_capture_array) {
        curr_data_valid = data->GetACK() && data->GetREQ();
        phase_t phase = data->GetPhase();
        if (phase == phase_t::selection && !data->GetBSY()) {
            selected_id = data->GetDAT();
        }
        if (prev_phase != phase) {
            if (close_row) {
                if (collapsible_div_active) {
                    html_fp << "</code></div>";
                }
                else if (button_active) {
                    html_fp << "</code></button>";
                }
                html_fp <<  "</td>";
                if (data_space_count < 1) {
                    html_fp << "<td>--</td>";
                }
                else {
                    html_fp << "<td>wc: " << std::dec << "(0x" << std::hex << data_space_count << ")</td>";
                }
                html_fp << "</tr>" << endl;
                data_space_count = 0;
            }
            html_fp << "<tr>";
            close_row = true; // Close the row the next time around
            html_fp << "<td>" << (double)data->GetTimestamp() / 100000 << "</td>";
            html_fp << "<td>" << data->GetPhaseStr() << "</td>";
            html_fp << "<td>" << std::hex << selected_id << "</td>";
            html_fp << "<td>";
        }
        if (curr_data_valid && !prev_data_valid) {
            if (data_space_count == 0) {
                button_active = true;
                html_fp << "<button type=\"button\" class=\"collapsible\"><code>";
            }
            if ((data_space_count % 16) == 0) {
                html_fp << std::hex << data_space_count << ": ";
            }

            html_fp << fmt::format("{0:02X}", data->GetDAT());

            data_space_count++;
            if ((data_space_count % 4) == 0) {
                html_fp <<  " ";
            }
            if (data_space_count == 16) {
                html_fp << "</code></button><div class=\"content\"><code>" << endl;
                collapsible_div_active = true;
                button_active = false;
            }
            if (((data_space_count % 16) == 0) && (data_space_count > 17)) {
                html_fp << "<br>" << endl;
            }
        }
        prev_data_valid = curr_data_valid;
        prev_phase = phase;
    }
}

void scsimon_generate_html(const string &filename, const vector<shared_ptr<DataSample>> &data_capture_array)
{
    LOGINFO("Creating HTML report file (%s)", filename.c_str())

    ofstream html_ofstream;

    html_ofstream.open(filename.c_str(), ios::out);

    html_ofstream << html_header;
    print_copyright_info(html_ofstream);
    print_html_data(html_ofstream,  data_capture_array);

    html_ofstream << html_footer;

    html_ofstream.close();
}
