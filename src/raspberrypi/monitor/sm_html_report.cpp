//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//
//---------------------------------------------------------------------------

#include <stdio.h>
#include <iostream>
#include <fstream>
#include "os.h"
#include "log.h"
#include "sm_reports.h"
#include "rascsi_version.h"

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
            << "<h1>RaSCSI scsimon Capture Tool</h1>" << endl \
            << "<pre>Version " << rascsi_get_version_string() \
            << __DATE__ << " " << __TIME__ << endl \
            << "Copyright (C) 2016-2020 GIMONS" << endl \
            << "Copyright (C) 2020-2021 Contributors to the RaSCSI project" << endl \
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


static void print_html_data(ofstream& html_fp, const data_capture *data_capture_array, DWORD capture_count)
{
    const data_capture *data;
    bool prev_data_valid = false;
    bool curr_data_valid;
    DWORD selected_id = 0;
    BUS::phase_t prev_phase = BUS::busfree;
    bool close_row = false;
    int data_space_count = 0;
    bool collapsible_div_active = false;
    bool button_active = false;

    html_fp << "<table>" << endl;

    for (DWORD idx = 0; idx < capture_count; idx++)
    {
        data = &data_capture_array[idx];
        curr_data_valid = GetAck(data) && GetReq(data);
        BUS::phase_t phase = GetPhase(data);
        if (phase == BUS::selection && !GetBsy(data))
        {
            selected_id = GetData(data);
        }
        if (prev_phase != phase)
        {
            if (close_row)
            {
                if (collapsible_div_active)
                {
                    html_fp << "</code></div>";
                }
                else if (button_active)
                {
                    html_fp << "</code></button>";
                }
                html_fp <<  "</td>";
                if (data_space_count < 1)
                {
                    html_fp << "<td>--</td>";
                }
                else
                {
                    html_fp << "<td>wc: " << std::dec << "(0x" << std::hex << data_space_count << ")</td>";
                }
                html_fp << "</tr>" << endl;
                data_space_count = 0;
            }
            html_fp << "<tr>";
            close_row = true; // Close the row the next time around
            html_fp << "<td>" << (double)data->timestamp / 100000 << "</td>";
            html_fp << "<td>" << GetPhaseStr(data) << "</td>";
            html_fp << "<td>" << std::hex << selected_id << "</td>";
            html_fp << "<td>";
        }
        if (curr_data_valid && !prev_data_valid)
        {
            if (data_space_count == 0)
            {
                button_active = true;
                html_fp << "<button type=\"button\" class=\"collapsible\"><code>";
            }
            if ((data_space_count % 16) == 0)
            {
                html_fp << std::hex << data_space_count << ": ";
            }

            html_fp << fmt::format("{0:02X}", GetData(data));

            data_space_count++;
            if ((data_space_count % 4) == 0)
            {
                html_fp <<  " ";
            }
            if (data_space_count == 16)
            {
                html_fp << "</code></button><div class=\"content\"><code>" << endl;
                collapsible_div_active = true;
                button_active = false;
            }
            if (((data_space_count % 16) == 0) && (data_space_count > 17))
            {
                html_fp << "<br>" << endl;
            }
        }
        prev_data_valid = curr_data_valid;
        prev_phase = phase;
    }
}

void scsimon_generate_html(const char *filename, const data_capture *data_capture_array, DWORD capture_count)
{
    LOGINFO("Creating HTML report file (%s)", filename);

    ofstream html_ofstream;

    html_ofstream.open(filename, ios::out);

    html_ofstream << html_header;
    print_copyright_info(html_ofstream);
    print_html_data(html_ofstream, data_capture_array, capture_count);

    html_ofstream << html_footer;

    html_ofstream.close();
}
