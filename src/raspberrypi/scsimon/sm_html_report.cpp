//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*) for Raspberry Pi
//
//	Copyright (C) 2020-2021 akuker
//
//	[ SCSI Bus Monitor ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "log.h"
using namespace std;
#include "sm_reports.h"

static void print_html_header(FILE *html_fp, const char *html_filename){
    fprintf(html_fp, "<html>\n");

    fprintf(html_fp, "<head>\n");
    fprintf(html_fp, "<style>\n");
    fprintf(html_fp, "table, th, td {\n");
    fprintf(html_fp, "  border: 1px solid black;\n");
    fprintf(html_fp, "  border-collapse: collapse;\n");
    fprintf(html_fp, "}\n");

    fprintf(html_fp, ".collapsible {\n");
    fprintf(html_fp, "  background-color: #777;\n");
    fprintf(html_fp, "  color: white;\n");
    fprintf(html_fp, "  cursor: pointer;\n");
    // fprintf(html_fp, "  padding: 18px;\n");
    fprintf(html_fp, "  width: 100%%;\n");
    fprintf(html_fp, "  border: none;\n");
    fprintf(html_fp, "  text-align: left;\n");
    fprintf(html_fp, "  outline: none;\n");
    // fprintf(html_fp, "  font-size: 15px;\n");
    fprintf(html_fp, "}\n");
    fprintf(html_fp, "\n");
    fprintf(html_fp, ".active, .collapsible:hover {\n");
    fprintf(html_fp, "  background-color: #555;\n");
    fprintf(html_fp, "}\n");
    fprintf(html_fp, "\n");
    fprintf(html_fp, ".content {\n");
    fprintf(html_fp, "  padding: 0;\n");
    fprintf(html_fp, "  display: none;\n");
    fprintf(html_fp, "  overflow: hidden;\n");
    fprintf(html_fp, "  background-color: #f1f1f1;\n");
    fprintf(html_fp, "}\n");

    fprintf(html_fp, "</style>\n");
    fprintf(html_fp, "</head>\n");

    fprintf(html_fp, "<h1>%s</h1>\n", html_filename);
    fprintf(html_fp, "<table>\n");
}

static void print_html_footer(FILE *html_fp){
    fprintf(html_fp, "</table>\n\n");

    fprintf(html_fp, "<script>\n");
    fprintf(html_fp, "var coll = document.getElementsByClassName(\"collapsible\");\n");
    fprintf(html_fp, "var i;\n");
    fprintf(html_fp, "\n");
    fprintf(html_fp, "for (i = 0; i < coll.length; i++) {\n");
    fprintf(html_fp, "  coll[i].addEventListener(\"click\", function() {\n");
    fprintf(html_fp, "    this.classList.toggle(\"active\");\n");
    fprintf(html_fp, "    var content = this.nextElementSibling;\n");
    fprintf(html_fp, "    if (content.style.display === \"block\") {\n");
    fprintf(html_fp, "      content.style.display = \"none\";\n");
    fprintf(html_fp, "    } else {\n");
    fprintf(html_fp, "      content.style.display = \"block\";\n");
    fprintf(html_fp, "    }\n");
    fprintf(html_fp, "  });\n");
    fprintf(html_fp, "}\n");
    fprintf(html_fp, "</script>\n\n");


    fprintf(html_fp, "</html>\n");
}

static void print_html_data(FILE *html_fp, const  data_capture *data_capture_array, int capture_count){
    const data_capture *data;
    bool prev_data_valid = false;
    bool curr_data_valid;
    DWORD selected_id = 0;
    BUS::phase_t prev_phase = BUS::busfree;
    bool close_row = false;
    int data_space_count = 0;
    bool collapsible_div_active = false;
    bool button_active = false;

    for(int idx=0; idx<capture_count; idx++){
        data = &data_capture_array[idx];
        curr_data_valid = GetAck(data) && GetReq(data);
        BUS::phase_t phase = GetPhase(data);
        if(phase == BUS::selection && !GetBsy(data)){
            selected_id = GetData(data);
        }
        if(prev_phase != phase){
            if(close_row){
                if(collapsible_div_active){
                    fprintf(html_fp,"</code></div>\n");
                }
                else if(button_active){
                    fprintf(html_fp, "</code></button>");
                }
                fprintf(html_fp, "</td>");
                if(data_space_count < 1){
                    fprintf(html_fp, "<td>--</td>");
                }else{
                    fprintf(html_fp, "<td>wc: %d (0x%X)</td>", data_space_count, data_space_count);
                }
                fprintf(html_fp, "</tr>\n");
                data_space_count = 0;
            }
            fprintf(html_fp, "<tr>");
            close_row = true; // Close the row the next time around
            fprintf(html_fp, "<td>%f</td>", (double)data->timestamp/100000);
            fprintf(html_fp, "<td>%s</td>", GetPhaseStr(data));
            fprintf(html_fp, "<td>%02X</td>", selected_id);
            fprintf(html_fp, "<td>");
        }
        if(curr_data_valid && !prev_data_valid){
            if (data_space_count == 0){
                button_active = true;
                fprintf(html_fp, "<button type=\"button\" class=\"collapsible\"><code>");
            }
            if((data_space_count % 16) == 0){
                fprintf(html_fp,"%02X: ", data_space_count);
            }

            fprintf(html_fp, "%02X", GetData(data));

            data_space_count++;
            if((data_space_count % 4) == 0){
                fprintf(html_fp, " ");
            }
            if(data_space_count == 16){
                fprintf(html_fp, "</code></button><div class=\"content\"><code>\n");
                collapsible_div_active = true;
                button_active = false;
            }
            if(((data_space_count % 16) == 0) && (data_space_count > 17)){
                fprintf(html_fp,"<br>\n");
            }
        }
        prev_data_valid = curr_data_valid;
        prev_phase = phase;
    }

}

void scsimon_generate_html(const char* filename, const  data_capture *data_capture_array, int capture_count){
    FILE *html_fp = fopen(filename, "w");

    print_html_header(html_fp, filename);
    print_html_data(html_fp, data_capture_array, capture_count);
    print_html_footer(html_fp);

    fclose(html_fp);
}

