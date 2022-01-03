//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//
//---------------------------------------------------------------------------

#include "os.h"
#include "filepath.h"
#include "fileio.h"
#include "log.h"
#include "gpiobus.h"
#include "rascsi_version.h"
#include "spdlog/spdlog.h"
#include <sys/time.h>
#include <climits>
#include <sstream>
#include <iostream>
#include <getopt.h>
#include "rascsi.h"
#include "gpio_sim.h"
using namespace std;

// static BYTE prev_value[32] = {0xFF};
// static volatile bool running;		// Running flag
// GPIOBUS *bus;						// GPIO Bus

data_capture *data_capture_array;
DWORD max_samples = 1000000;
int actual_samples = 0;

const char* json_filename = "./radius_boot.json";

const char timestamp_label[] = "\"timestamp\":\"0x";
const char data_label[] = "\"data\":\"0x";

FILE *html_fp;

void read_json()
{
    char str_buf[1024];
    FILE *fp = fopen(json_filename, "r");
    int idx=0;

    while(fgets(str_buf, sizeof(str_buf), fp))
    {

        char timestamp[1024];
        char data[1024];
        uint64_t timestamp_uint;
        uint32_t data_uint;
        char *ptr;

        char *timestamp_str;
        char *data_str;
        timestamp_str = strnstr(str_buf, timestamp_label, sizeof(str_buf));
        if(!timestamp_str)continue;
        strncpy(timestamp, &timestamp_str[strlen(timestamp_label)],16);
        timestamp[16] = '\0';
        timestamp_uint = strtoull(timestamp,&ptr,16);

        data_str = strnstr(str_buf, data_label, sizeof(str_buf));
        if(!data_str)continue;
        strncpy(data, &data_str[strlen(data_label)],8);
        data[8] = '\0';
        data_uint = strtoul(data, &ptr, 16);

        // printf("Time: %016llX Data: %08X\n", timestamp_uint, data_uint);

        data_capture_array[idx].timestamp = timestamp_uint;
        data_capture_array[idx].data = data_uint;
        idx++;
    }
    fclose(fp);
    actual_samples = idx;
}

void dump_data(){
    data_capture *data;

    for(int idx=0; idx<actual_samples; idx++){
        data = &data_capture_array[idx];
        printf(" BSY:%d MSG:%d IO:%d CD:%d DATA:%02X\n",GetBsy(data),GetMsg(data),GetIo(data),GetCd(data),GetData(data));
    }
}

void decode_data(){
    data_capture *data;
    bool prev_data_valid = false;
    bool curr_data_valid;
    DWORD selected_id = 0;

    for(int idx=0; idx<actual_samples; idx++){
        data = &data_capture_array[idx];
        curr_data_valid = GetAck(data) && GetReq(data);
        BUS::phase_t phase = GetPhase(data);
        if(phase == BUS::selection && !GetBsy(data)){
            selected_id = GetData(data);
        }
        if(curr_data_valid && !prev_data_valid){
            printf("[%02X] Time: %lf Data Sample: %02X Phase: %s\n", selected_id, ((double)data->timestamp/10)/1000, GetData(data), GetPhaseStr(data));
        }
        prev_data_valid = curr_data_valid;

        //     prev_data_valid
        // printf(" BSY:%d MSG:%d IO:%d CD:%d DATA:%02X\n",GetBsy(data),GetMsg(data),GetIo(data),GetCd(data),GetData(data));
    }


}

void print_html_header(){
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

    fprintf(html_fp, "<h1>%s</h1>\n", json_filename);
    fprintf(html_fp, "<table>\n");
}

void print_html_footer(){
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

void print_html_data(){
    data_capture *data;
    bool prev_data_valid = false;
    bool curr_data_valid;
    DWORD selected_id = 0;
    BUS::phase_t prev_phase = BUS::busfree;
    bool close_row = false;
    int data_space_count = 0;
    bool collapsible_div_active = false;

    for(int idx=0; idx<actual_samples; idx++){
        data = &data_capture_array[idx];
        curr_data_valid = GetAck(data) && GetReq(data);
        BUS::phase_t phase = GetPhase(data);
        if(phase == BUS::selection && !GetBsy(data)){
            selected_id = GetData(data);
        }
        if(prev_phase != phase){
            if(close_row){
                if(collapsible_div_active){
                    fprintf(html_fp,"</div>\n");
                }
                fprintf(html_fp, "</code></td>\n");
                fprintf(html_fp, "</tr>\n");
                data_space_count = 0;
            }
            fprintf(html_fp, "<tr>");
            close_row = true; // Close the row the next time around
            fprintf(html_fp, "<td>%s</td>", GetPhaseStr(data));
            fprintf(html_fp, "<td>%02X</td>", selected_id);
            fprintf(html_fp, "<td><button type=\"button\" class=\"collapsible\"><code>00: ");
        }
        if(curr_data_valid && !prev_data_valid){
            fprintf(html_fp, "%02X", GetData(data));

            data_space_count++;
            if((data_space_count % 4) == 0){
                fprintf(html_fp, " ");
            }
            if(data_space_count == 16){
                fprintf(html_fp, "</button><div class=\"content\">\n");
                collapsible_div_active = true;
            }
            if((data_space_count % 16) == 0){
                fprintf(html_fp,"<p>\n%02X: ", data_space_count);
            }
        }

        // if(curr_data_valid && !prev_data_valid){
        //     printf("[%02X] Time: %lf Data Sample: %02X Phase: %s\n", selected_id, ((double)data->timestamp/10)/1000, GetData(data), GetPhaseStr(data));
        // }
        prev_data_valid = curr_data_valid;
        prev_phase = phase;

        //     prev_data_valid
        // printf(" BSY:%d MSG:%d IO:%d CD:%d DATA:%02X\n",GetBsy(data),GetMsg(data),GetIo(data),GetCd(data),GetData(data));
    }

}

void print_html(){

    char html_filename[256];
    char *file_ext;
    strncpy(html_filename, json_filename,sizeof html_filename);

    file_ext = strrchr(html_filename, '.');
    strcpy(file_ext, ".html");

    html_fp = fopen(html_filename, "w");

    print_html_header();
    print_html_data();
    print_html_footer();

    fclose(html_fp);
}



//---------------------------------------------------------------------------
//
//	Main processing
//
//---------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    printf("starting\n");
    data_capture_array = (data_capture*)malloc(max_samples * sizeof(data_capture));
    printf("reading json\n");
    read_json();
    printf("Actual samples: %d\n", actual_samples);
    // dump_data();
    decode_data();
    print_html();
}