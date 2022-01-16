//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//  Copyright (C) 2020-2021 akuker
//  Copyright (C) 2020 joshua stein <jcs@jcs.org>
//
//  Licensed under the BSD 3-Clause License. 
//  See LICENSE file in the project root folder.
//
//  [ Emulation of the Radius PowerView SCSI Display Adapter ]
//
//  Note: This requires the Radius RadiusWare driver.
//
//  Framebuffer integration originally done by Joshua Stein:
//     https://github.com/jcs/RASCSI/commit/6da9e9f3ffcd38eb89413cd445f7407739c54bca
//
//---------------------------------------------------------------------------

#include "scsi_powerview.h"
#include <sstream>
#include <iomanip>

#include "exceptions.h"
#include <err.h>
#include <fcntl.h>
#include <linux/fb.h>
#include "os.h"
#include "disk.h"
#include <sys/mman.h>
#include "log.h"
#include "controllers/scsidev_ctrl.h"

static unsigned char reverse_table[256];

const BYTE SCSIPowerView::m_inquiry_response[] = {
0x03, 0x00, 0x01, 0x01, 0x46, 0x00, 0x00, 0x00, 0x52, 0x41, 0x44, 0x49, 0x55, 0x53, 0x20, 0x20,
0x50, 0x6F, 0x77, 0x65, 0x72, 0x56, 0x69, 0x65, 0x77, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
0x56, 0x31, 0x2E, 0x30, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x20, 0x00, 0x01, 0x00, 0x00, 0x00,
0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00,
0x05, 0x00, 0x00, 0x00, 0x00, 0x06, 0x43, 0xF9, 0x00, 0x00, 0xFF,
};

SCSIPowerView::SCSIPowerView() : Disk("SCPV")
{
	// AddCommand(SCSIDEV::eCmdPvReadConfig, "Read PowerView Config", &SCSIPowerView::CmdPvReadConfig);
	// AddCommand(SCSIDEV::eCmdPvWriteConfig, "Write PowerView Config", &SCSIPowerView::CmdPvWriteConfig);
	// AddCommand(SCSIDEV::eCmdPvWriteFrameBuffer, "Write Framebuffer", &SCSIPowerView::CmdWriteFramebuffer);
	// AddCommand(SCSIDEV::eCmdPvWriteColorPalette, "Write Color Palette", &SCSIPowerView::CmdWriteColorPalette);
	AddCommand(SCSIDEV::eCmdPvReadConfig, "Unknown PowerViewC8", &SCSIPowerView::CmdReadConfig);
	AddCommand(SCSIDEV::eCmdPvWriteConfig, "Unknown PowerViewC9", &SCSIPowerView::CmdWriteConfig);
	AddCommand(SCSIDEV::eCmdPvWriteFrameBuffer, "Unknown PowerViewCA", &SCSIPowerView::CmdWriteFramebuffer);
	AddCommand(SCSIDEV::eCmdPvWriteColorPalette, "Unknown PowerViewCB", &SCSIPowerView::CmdWriteColorPalette);
	AddCommand(SCSIDEV::eCmdUnknownPowerViewCC, "Unknown PowerViewCC", &SCSIPowerView::UnknownCommandCC);

	struct fb_var_screeninfo fbinfo;
	struct fb_fix_screeninfo fbfixinfo;

	// create lookup table
	for (int i = 0; i < 256; i++) {
		unsigned char b = i;
		b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
		b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
		b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
		reverse_table[i] = b;
	}

	for(int i=0; i < (256/8); i++){
		LOGINFO("%02X %02X %02X %02X %02X %02X %02X %02X ", reverse_table[(i*8)],reverse_table[(i*8)+1],reverse_table[(i*8)+2],reverse_table[(i*8)+3], reverse_table[(i*8)+4], reverse_table[(i*8)+5], reverse_table[(i*8)+6], reverse_table[(i*7)] );
	}

	// TODO: receive these through a SCSI message sent by the remote
	this->screen_width = 624;
	this->screen_height = 840;

	this->fbfd = open("/dev/fb0", O_RDWR);
	if (this->fbfd == -1)
		err(1, "open /dev/fb0");

	if (ioctl(this->fbfd, FBIOGET_VSCREENINFO, &fbinfo))
		err(1, "ioctl FBIOGET_VSCREENINFO");

	if (ioctl(this->fbfd, FBIOGET_FSCREENINFO, &fbfixinfo))
		err(1, "ioctl FBIOGET_FSCREENINFO");

	// if (fbinfo.bits_per_pixel != 32)
	// 	errx(1, "TODO: support %d bpp", fbinfo.bits_per_pixel);

	this->fbwidth = fbinfo.xres;
	this->fbheight = fbinfo.yres;
	this->fbbpp = fbinfo.bits_per_pixel;
	this->fblinelen = fbfixinfo.line_length;
	this->fbsize = fbfixinfo.smem_len;

	LOGINFO("SCSIVideo drawing on %dx%d %d bpp framebuffer\n",
	    this->fbwidth, this->fbheight, this->fbbpp);

	this->fb = (char *)mmap(0, this->fbsize, PROT_READ | PROT_WRITE, MAP_SHARED,
		this->fbfd, 0);
	if ((int)this->fb == -1)
		err(1, "mmap");

	memset(this->fb, 0, this->fbsize);

	this->m_powerview_resolution_x = 624;
	this->m_powerview_resolution_y = 840;

	fbcon_cursor(false);
	fbcon_blank(false);
	ClearFrameBuffer();

}

//---------------------------------------------------------------------------
//
//	Enable/Disable the framebuffer flashing cursor
//      From: https://linux-arm-kernel.infradead.narkive.com/XL0ylAHW/turn-off-framebuffer-cursor
//
//---------------------------------------------------------------------------
void SCSIPowerView::fbcon_cursor(bool blank)
{
	int fd = open("/dev/tty1", O_RDWR);
	if (0 < fd)
	{
		write(fd, "\033[?25", 5);
		write(fd, blank ? "h" : "l", 1);
	}
	else
	{
		LOGWARN("%s Unable to open /dev/tty1", __PRETTY_FUNCTION__);
	}
	close(fd);
}

//---------------------------------------------------------------------------
//
//	Enable/disable the framebuffer blanking
//      From: https://linux-arm-kernel.infradead.narkive.com/XL0ylAHW/turn-off-framebuffer-cursor
//
//---------------------------------------------------------------------------
void SCSIPowerView::fbcon_blank(bool blank)
{
	int ret = ioctl(fbfd, FBIOBLANK, blank ? VESA_POWERDOWN : VESA_NO_BLANKING);
	if(ret < 0){
		LOGWARN("%s Unable to disable blanking", __PRETTY_FUNCTION__);
	}
	return;
}

SCSIPowerView::~SCSIPowerView()
{
	// Re-enable the cursor
	fbcon_cursor(true);

	munmap(this->fb, this->fbsize);
	close(this->fbfd);

	for (auto const& command : commands) {
		delete command.second;
	}
}

void SCSIPowerView::AddCommand(SCSIDEV::scsi_command opcode, const char* name, void (SCSIPowerView::*execute)(SASIDEV *))
{
	commands[opcode] = new command_t(name, execute);
}


void SCSIPowerView::ClearFrameBuffer(){


		// For each row
		for (DWORD idx_row_y = 0; idx_row_y < 800; idx_row_y++){


			// For each column
			for (DWORD idx_col_x = 0; idx_col_x < 800; idx_col_x++){

			// int loc = (col * (this->fbbpp / 8)) + (row * this->fblinelen);
		 		uint32_t loc = ((idx_col_x) * (this->fbbpp / 8)) + ((idx_row_y) * fblinelen);
				
				// LOGDEBUG("!!! x:%d y:%d !! pix_idx:%06X pix_byte:%04X pix_bit:%02X pixel: %02X, loc:%08X",idx_col_x, idx_row_y, pixel_byte_idx, pixel_byte, pixel_bit, pixel, loc);

				*(this->fb + loc + 0) = 0xFF; 
				*(this->fb + loc + 1) = 0x00; 
				*(this->fb + loc + 2) = 0x00; 

				// for(int i=0 ; i< (this->fbbpp/8); i++){
				// 	*(this->fb + loc + i) = 0xFF;
				// }
			}
		}


}

void SCSIPowerView::dump_command(SASIDEV *controller){

            LOGWARN("                %02X %02X %02X %02X %02X %02X %02X %02X [%02X] \n",
				ctrl->cmd[0],
				ctrl->cmd[1],
				ctrl->cmd[2],
				ctrl->cmd[3],
				ctrl->cmd[4],
				ctrl->cmd[5],
				ctrl->cmd[6],
				ctrl->cmd[7],
				ctrl->cmd[8]);

	// for(int i=0; i<8; i++){
    //         LOGWARN("   [%d]: %08X\n",i, ctrl->cmd[i]);	
	// }

}

//---------------------------------------------------------------------------
//
//	PowerView Read Configuration Parameter
//
//---------------------------------------------------------------------------
// void SCSIPowerView::CmdPvReadConfig(SASIDEV *controller)
void SCSIPowerView::CmdReadConfig(SASIDEV *controller)
{

	// Set transfer amount
	ctrl->length = ctrl->cmd[6];
	LOGWARN("%s Message Length %d", __PRETTY_FUNCTION__, (int)ctrl->length);
	dump_command(controller);

	if (ctrl->length <= 0) {
		// Failure (Error)
		controller->Error();
		return;
	}

	// Response to "C8 00 00 31 83 00 01 00" is "00"
	if(ctrl->cmd[4] == 0x83){

		if(ctrl->length != 1){
			LOGWARN("%s Received unexpected length: %02X %02X %02X %02X %02X %02X", __PRETTY_FUNCTION__, ctrl->cmd[0], ctrl->cmd[1], ctrl->cmd[2], ctrl->cmd[3], ctrl->cmd[4], ctrl->cmd[5]);
		}
	}
	// Response to "C8 00 00 31 00 00 03 00" is "01 09 08"
	else if(ctrl->cmd[4] == 0x00){
		if(ctrl->length != 3){
			LOGWARN("%s Received unexpected length: %02X %02X %02X %02X %02X %02X", __PRETTY_FUNCTION__, ctrl->cmd[0], ctrl->cmd[1], ctrl->cmd[2], ctrl->cmd[3], ctrl->cmd[4], ctrl->cmd[5]);
		}
	}
	// Response to "C8 00 00 31 82 00 01 00" is "01"
	else if(ctrl->cmd[4] == 0x82){
		ctrl->buffer[0] = 0x01;
		if(ctrl->length != 1){
			LOGWARN("%s Received unexpected length: %02X %02X %02X %02X %02X %02X", __PRETTY_FUNCTION__, ctrl->cmd[0], ctrl->cmd[1], ctrl->cmd[2], ctrl->cmd[3], ctrl->cmd[4], ctrl->cmd[5]);
		}
	}
	else{
		LOGWARN("%s Unhandled command received: %02X %02X %02X %02X %02X %02X", __PRETTY_FUNCTION__, ctrl->cmd[0], ctrl->cmd[1], ctrl->cmd[2], ctrl->cmd[3], ctrl->cmd[4], ctrl->cmd[5]);
		ctrl->buffer[0] = 0x00;
		ctrl->buffer[1] = 0x00;
		ctrl->buffer[2] = 0x00;
	}

	// Set next block
	ctrl->blocks = 1;
	ctrl->next = 1;

	controller->DataIn();
}


//---------------------------------------------------------------------------
//
//	Unknown Command C9
//
//---------------------------------------------------------------------------
// void SCSIPowerView::CmdPvWriteConfig(SASIDEV *controller)
void SCSIPowerView::CmdWriteConfig(SASIDEV *controller)
{

	// Set transfer amount
	ctrl->length = ctrl->cmd[6];
	LOGTRACE("%s Message Length %d", __PRETTY_FUNCTION__, (int)ctrl->length);
	// dump_command(controller);
	// LOGWARN("Controller: %08X ctrl: %08X", (DWORD)controller->GetCtrl(), (DWORD)ctrl);
	// if (ctrl->length <= 0) {
	// 	// Failure (Error)
	// 	controller->Error(); 
	// 	return;
	// }

	if (ctrl->length == 0){
		controller->Status();
	}
	else
	{
		// Set next block
		ctrl->blocks = 1;
		ctrl->next = 1;

		controller->DataOut();
	}
}



//---------------------------------------------------------------------------
//
//	Unknown Command CA
//
//---------------------------------------------------------------------------
void SCSIPowerView::CmdWriteFramebuffer(SASIDEV *controller)
{
	// Make sure the receive buffer is big enough
	if (ctrl->bufsize < POWERVIEW_BUFFER_SIZE) {
		free(ctrl->buffer);
		ctrl->bufsize = POWERVIEW_BUFFER_SIZE;
		ctrl->buffer = (BYTE *)malloc(ctrl->bufsize);
	}

	// Set transfer amount
	uint16_t width_x = ctrl->cmd[5] + (ctrl->cmd[4] << 8);
	uint16_t height_y = ctrl->cmd[7] + (ctrl->cmd[6] << 8);

	ctrl->length = width_x * height_y;
	// if(ctrl->cmd[9] == 0){
	// 	ctrl->length = 0x9600;
	// }
	// else {
	// 	ctrl->length = ctrl->cmd[7] * 2;
	// }
	LOGTRACE("%s Message Length %d [%08X]", __PRETTY_FUNCTION__, (int)ctrl->length, (unsigned int)ctrl->length);
	// LOGDEBUG("                %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X [%02X %02X]\n",
	// 			ctrl->cmd[0],
	// 			ctrl->cmd[1],
	// 			ctrl->cmd[2],
	// 			ctrl->cmd[3],
	// 			ctrl->cmd[4],
	// 			ctrl->cmd[5],
	// 			ctrl->cmd[6],
	// 			ctrl->cmd[7],
	// 			ctrl->cmd[8],
	// 			ctrl->cmd[9],
	// 			ctrl->cmd[10],
	// 			ctrl->cmd[11],
	// 			ctrl->cmd[12]);


	if (ctrl->length <= 0) {
		// Failure (Error)
		controller->Error();
		return;
	}

	// Set next block
	ctrl->blocks = 1;
	ctrl->next = 1;

	controller->DataOut();
}


//---------------------------------------------------------------------------
//
//	Unknown Command CB
//
//---------------------------------------------------------------------------
void SCSIPowerView::CmdWriteColorPalette(SASIDEV *controller)
{

	// Set transfer amount
	ctrl->length = ctrl->cmd[6];
	ctrl->length = ctrl->length * 4;
	LOGTRACE("%s Message Length %d", __PRETTY_FUNCTION__, (int)ctrl->length);
	// dump_command(controller);
	if (ctrl->length <= 0) {
		// Failure (Error)
		controller->Error();
		return;
	}

	// Set next block
	ctrl->blocks = 1;
	ctrl->next = 1;

	controller->DataOut();
}


//---------------------------------------------------------------------------
//
//	Unknown Command CC
//
//---------------------------------------------------------------------------
void SCSIPowerView::UnknownCommandCC(SASIDEV *controller)
{

	// Set transfer amount
	// ctrl->length = ctrl->cmd[6];
	ctrl->length = 0x8bb;
	LOGTRACE("%s Message Length %d", __PRETTY_FUNCTION__, (int)ctrl->length);
	// dump_command(controller);
	if (ctrl->length <= 0) {
		// Failure (Error)
		controller->Error();
		return;
	}

	// Set next block
	ctrl->blocks = 1;
	ctrl->next = 1;

	controller->DataOut();
}

bool SCSIPowerView::Dispatch(SCSIDEV *controller)
{
	ctrl = controller->GetCtrl();

	if (commands.count(static_cast<SCSIDEV::scsi_command>(ctrl->cmd[0]))) {
		command_t *command = commands[static_cast<SCSIDEV::scsi_command>(ctrl->cmd[0])];

		LOGDEBUG("%s Executing %s ($%02X)", __PRETTY_FUNCTION__, command->name, (unsigned int)ctrl->cmd[0]);

		(this->*command->execute)(controller);

		return true;
	}

	LOGTRACE("%s Calling base class for dispatching $%02X", __PRETTY_FUNCTION__, (unsigned int)ctrl->cmd[0]);

	// The base class handles the less specific commands
	return Disk::Dispatch(controller);
}

bool SCSIPowerView::Init(const map<string, string>& params)
{
	SetParams(params.empty() ? GetDefaultParams() : params);

	return true;
}

void SCSIPowerView::Open(const Filepath& path)
{

}

//---------------------------------------------------------------------------
//
//	INQUIRY
//
//---------------------------------------------------------------------------
int SCSIPowerView::Inquiry(const DWORD *cdb, BYTE *buf)
{
	int allocation_length = cdb[4] + (((DWORD)cdb[3]) << 8);
	if (allocation_length > 0) {
		if ((unsigned int)allocation_length > sizeof(m_inquiry_response)) {
			allocation_length = (int)sizeof(m_inquiry_response);
		}

		memcpy(buf, m_inquiry_response, allocation_length);

		// // Basic data
		// // buf[0] ... Processor Device
		// // buf[1] ... Not removable
		// // buf[2] ... SCSI-2 compliant command system
		// // buf[3] ... SCSI-2 compliant Inquiry response
		// // buf[4] ... Inquiry additional data
		// // http://www.bitsavers.org/pdf/apple/scsi/dayna/daynaPORT/pocket_scsiLINK/pocketscsilink_inq.png
		// memset(buf, 0, allocation_length);
		// buf[0] = 0x03;
		// buf[2] = 0x01;
		// buf[4] = 0x1F;

		// // Padded vendor, product, revision
		// memcpy(&buf[8], GetPaddedName().c_str(), 28);
	}

	LOGTRACE("response size is %d", allocation_length);

	return allocation_length;
}




//---------------------------------------------------------------------------
//
//	READ
//
//---------------------------------------------------------------------------
int SCSIPowerView::Read(const DWORD *cdb, BYTE *buf, uint64_t block)
{
	int rx_packet_size = 0;
    return rx_packet_size;

}

bool SCSIPowerView::WriteConfiguration(const DWORD *cdb, const BYTE *buf, const DWORD length)
{

	std::stringstream ss;
    ss << std::hex;

     for( DWORD i(0) ; i < length; ++i ){
         ss << std::setw(2) << std::setfill('0') << (int)buf[i];
	 }

	LOGDEBUG("%s Received Config Write of length %d: %s",__PRETTY_FUNCTION__, length, ss.str().c_str());

	return true;

}

bool SCSIPowerView::WriteUnknownCC(const DWORD *cdb, const BYTE *buf, const DWORD length)
{

	if(length > sizeof(unknown_cc_data)){
		LOGERROR("%s Received Unknown CC data that is larger (%d bytes) than allocated size (%d bytes)", __PRETTY_FUNCTION__, length, sizeof(unknown_cc_data));
		return false;
	}

	LOGINFO("%s Unknown CC of size %ul received", __PRETTY_FUNCTION__, length);
	memcpy(unknown_cc_data, buf, length);
	unknown_cc_data_length = length;
	return true;
}


bool SCSIPowerView::WriteColorPalette(const DWORD *cdb, const BYTE *buf, const DWORD length)
{
	if(length > sizeof(color_palette)){
		LOGERROR("%s Received Color Palette that is larger (%d bytes) than allocated size (%d bytes)", __PRETTY_FUNCTION__, length, sizeof(color_palette));
		return false;
	}

	LOGINFO("%s Color Palette of size %ul received", __PRETTY_FUNCTION__, length);
	memcpy(color_palette, buf, length);
	color_palette_length = length;
	return true;
}


bool SCSIPowerView::WriteFrameBuffer(const DWORD *cdb, const BYTE *buf, const DWORD length)
{


	// try {

		// Apple portrait display resolution
		// 624 by 840

		// const int bits_per_pixel = 1;

		// BYTE pixel_value = 0;


	// this->screen_width = 624;
	// this->screen_height = 840;

	// // TODO: receive these through a SCSI message sent by the remote
	// this->screen_width = 624;
	// this->screen_height = 840;

		// Set transfer amount
		uint32_t update_width_x_bytes = ctrl->cmd[5] + (ctrl->cmd[4] << 8);
		uint32_t update_height_y_bytes = ctrl->cmd[7] + (ctrl->cmd[6] << 8);

		uint32_t offset = (uint32_t)ctrl->cmd[3] + ((uint32_t)ctrl->cmd[2] << 8) + ((uint32_t)ctrl->cmd[1] << 16);

		LOGDEBUG("%s act length %u offset:%06X (%f) wid:%06X height:%06X", __PRETTY_FUNCTION__, length, offset, ((float)offset/(screen_width*screen_height)), update_width_x_bytes, update_height_y_bytes);

		uint32_t offset_row = (uint32_t)((offset*8) / this->screen_width)/2;
		uint32_t offset_col = (uint32_t)(((offset*8)/2) % this->screen_width);
 
		LOGDEBUG("WriteFrameBuffer: Update x:%06X y:%06X width:%06X height:%06X", offset_col, offset_row, update_width_x_bytes * 8, update_height_y_bytes )
		

		// For each row
		for (DWORD idx_row_y = 0; idx_row_y < (update_height_y_bytes); idx_row_y++){


			// For each column
			for (DWORD idx_col_x = 0; idx_col_x < (update_width_x_bytes*8); idx_col_x++){

				DWORD pixel_byte_idx = (idx_row_y * update_width_x_bytes) + (idx_col_x / 8);
				BYTE pixel_byte = reverse_table[buf[pixel_byte_idx]];
				// BYTE pixel_byte =buf[pixel_byte_idx];
				DWORD pixel_bit = idx_col_x % 8;

				BYTE pixel = (pixel_byte & (1 << pixel_bit) ? 0 : 255);

			// int loc = (col * (this->fbbpp / 8)) + (row * this->fblinelen);
		 		uint32_t loc = ((idx_col_x + offset_col) * (this->fbbpp / 8)) + ((idx_row_y + offset_row) * fblinelen);
				
				// LOGDEBUG("!!! x:%d y:%d !! pix_idx:%06X pix_byte:%04X pix_bit:%02X pixel: %02X, loc:%08X",idx_col_x, idx_row_y, pixel_byte_idx, pixel_byte, pixel_bit, pixel, loc);

				for(int i=0 ; i< (this->fbbpp/8); i++){
					*(this->fb + loc + i) = pixel;
				}
			}
		}


	return true;
}

//---------------------------------------------------------------------------
//
//  Write
//
//   Command:  0a 00 00 LL LL XX (LLLL is data length, XX = 80 or 00)
//   Function: Write a packet at a time to the device (standard SCSI Write)
//   Type:     Output; the format of the data to be sent depends on the value
//             of XX, as follows:
//              - if XX = 00, LLLL is the packet length, and the data to be sent
//                must be an image of the data packet
//              - if XX = 80, LLLL is the packet length + 8, and the data to be
//                sent is:
//                  PP PP 00 00 XX XX XX ... 00 00 00 00
//                where:
//                  PPPP      is the actual (2-byte big-endian) packet length
//               XX XX ... is the actual packet
//
//---------------------------------------------------------------------------
bool SCSIPowerView::Write(const DWORD *cdb, const BYTE *buf, const DWORD length)
{
	BYTE data_format = cdb[5];
	// WORD data_length = (WORD)cdb[4] + ((WORD)cdb[3] << 8);

	if (data_format == 0x00){
		// m_tap->Tx(buf, data_length);
		// LOGTRACE("%s Transmitted %u bytes (00 format)", __PRETTY_FUNCTION__, data_length);
		return true;
	}
	else if (data_format == 0x80){
		// // The data length is specified in the first 2 bytes of the payload
		// data_length=(WORD)buf[1] + ((WORD)buf[0] << 8);
		// m_tap->Tx(&buf[4], data_length);
		// LOGTRACE("%s Transmitted %u bytes (80 format)", __PRETTY_FUNCTION__, data_length);
		return true;
	}
	else
	{
		// LOGWARN("%s Unknown data format %02X", __PRETTY_FUNCTION__, (unsigned int)command->format);
		LOGWARN("%s Unknown data format %02X", __PRETTY_FUNCTION__, (unsigned int)data_format);
		return true;
	}
}
	

bool SCSIPowerView::ReceiveBuffer(int len, BYTE *buffer)
{
	int row = 0;
	int col = 0;

	for (int i = 0; i < len; i++) {
		unsigned char j = reverse_table[buffer[i]];

		for (int bit = 0; bit < 8; bit++) {
			int loc = (col * (this->fbbpp / 8)) + (row * this->fblinelen);
			col++;
			if (col % this->screen_width == 0) {
				col = 0;
				row++;
			}

			*(this->fb + loc) = (j & (1 << bit)) ? 0 : 255;
			*(this->fb + loc + 1) = (j & (1 << bit)) ? 0 : 255;
			*(this->fb + loc + 2) = (j & (1 << bit)) ? 0 : 255;
		}
	}

	return TRUE;
}