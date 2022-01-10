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

#include "exceptions.h"
#include <err.h>
#include <fcntl.h>
#include <linux/fb.h>
#include "os.h"
#include "disk.h"
#include <sys/mman.h>
#include "log.h"

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
	AddCommand(SCSIDEV::eCmdUnknownPowerViewC8, "Unknown PowerViewC8", &SCSIPowerView::UnknownCommandC8);
	AddCommand(SCSIDEV::eCmdUnknownPowerViewC9, "Unknown PowerViewC9", &SCSIPowerView::UnknownCommandC9);
	AddCommand(SCSIDEV::eCmdWriteFrameBuffer, "Unknown PowerViewCA", &SCSIPowerView::CmdWriteFramebuffer);
	AddCommand(SCSIDEV::eCmdUnknownPowerViewCB, "Unknown PowerViewCB", &SCSIPowerView::UnknownCommandCB);
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

	LOGWARN("SCSIVideo drawing on %dx%d %d bpp framebuffer\n",
	    this->fbwidth, this->fbheight, this->fbbpp);

	this->fb = (char *)mmap(0, this->fbsize, PROT_READ | PROT_WRITE, MAP_SHARED,
		this->fbfd, 0);
	if ((int)this->fb == -1)
		err(1, "mmap");

	memset(this->fb, 0, this->fbsize);

	this->m_powerview_resolution_x = 624;
	this->m_powerview_resolution_y = 840;

}

SCSIPowerView::~SCSIPowerView()
{
	// // TAP driver release
	// if (m_tap) {
	// 	m_tap->Cleanup();
	// 	delete m_tap;
	// }

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
//	Unknown Command C8
//
//---------------------------------------------------------------------------
void SCSIPowerView::UnknownCommandC8(SASIDEV *controller)
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

	ctrl->buffer[0] = 0x01;
	ctrl->buffer[1] = 0x09;
	ctrl->buffer[2] = 0x08;

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
void SCSIPowerView::UnknownCommandC9(SASIDEV *controller)
{

	// Set transfer amount
	ctrl->length = ctrl->cmd[6];
	LOGWARN("%s Message Length %d", __PRETTY_FUNCTION__, (int)ctrl->length);
	dump_command(controller);
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
	LOGWARN("%s Message Length %d [%08X]", __PRETTY_FUNCTION__, (int)ctrl->length, (unsigned int)ctrl->length);
	LOGWARN("                %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X [%02X %02X]\n",
				ctrl->cmd[0],
				ctrl->cmd[1],
				ctrl->cmd[2],
				ctrl->cmd[3],
				ctrl->cmd[4],
				ctrl->cmd[5],
				ctrl->cmd[6],
				ctrl->cmd[7],
				ctrl->cmd[8],
				ctrl->cmd[9],
				ctrl->cmd[10],
				ctrl->cmd[11],
				ctrl->cmd[12]);


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
void SCSIPowerView::UnknownCommandCB(SASIDEV *controller)
{

	// Set transfer amount
	ctrl->length = ctrl->cmd[6];
	ctrl->length = ctrl->length * 4;
	LOGWARN("%s Message Length %d", __PRETTY_FUNCTION__, (int)ctrl->length);
	dump_command(controller);
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
	LOGWARN("%s Message Length %d", __PRETTY_FUNCTION__, (int)ctrl->length);
	dump_command(controller);
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
//   Command:  08 00 00 LL LL XX (LLLL is data length, XX = c0 or 80)
//   Function: Read a packet at a time from the device (standard SCSI Read)
//   Type:     Input; the following data is returned:
//             LL LL NN NN NN NN XX XX XX ... CC CC CC CC
//   where:
//             LLLL      is normally the length of the packet (a 2-byte
//                       big-endian hex value), including 4 trailing bytes
//                       of CRC, but excluding itself and the flag field.
//                       See below for special values
//             NNNNNNNN  is a 4-byte flag field with the following meanings:
//                       FFFFFFFF  a packet has been dropped (?); in this case
//                                 the length field appears to be always 4000
//                       00000010  there are more packets currently available
//                                 in SCSI/Link memory
//                       00000000  this is the last packet
//             XX XX ... is the actual packet
//             CCCCCCCC  is the CRC
//
//   Notes:
//    - When all packets have been retrieved successfully, a length field
//      of 0000 is returned; however, if a packet has been dropped, the
//      SCSI/Link will instead return a non-zero length field with a flag
//      of FFFFFFFF when there are no more packets available.  This behaviour
//      seems to continue until a disable/enable sequence has been issued.
//    - The SCSI/Link apparently has about 6KB buffer space for packets.
//
//---------------------------------------------------------------------------
int SCSIPowerView::Read(const DWORD *cdb, BYTE *buf, uint64_t block)
{
	int rx_packet_size = 0;
    return rx_packet_size;

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

		LOGDEBUG("%s act length %ul offset:%06X (%f) wid:%06X height:%06X", __PRETTY_FUNCTION__, length, offset, ((float)offset/(screen_width*screen_height)), update_width_x_bytes, update_height_y_bytes);

		uint32_t offset_row = (uint32_t)((offset*8) / this->screen_width)/2;
		uint32_t offset_col = (uint32_t)((offset*8) % this->screen_width);
 
		LOGDEBUG("WriteFrameBuffer: Update x:%06X y:%06X width:%06X height:%06X", offset_col, offset_row, update_width_x_bytes * 8, update_height_y_bytes )
		

		// For each row
		for (DWORD idx_row_y = 0; idx_row_y < (update_height_y_bytes); idx_row_y++){


			// For each column
			for (DWORD idx_col_x = 0; idx_col_x < (update_width_x_bytes*8); idx_col_x++){

				DWORD pixel_byte_idx = (idx_row_y * update_width_x_bytes) + (idx_col_x / 8);
				BYTE pixel_byte = reverse_table[buf[pixel_byte_idx]];
				// BYTE pixel_byte =buf[pixel_byte_idx];
				DWORD pixel_bit = idx_col_x % 8;

				BYTE pixel = (pixel_byte & (1 << pixel_bit) ? 255 : 0);

			// int loc = (col * (this->fbbpp / 8)) + (row * this->fblinelen);
		 		uint32_t loc = ((idx_col_x + offset_col) * (this->fbbpp / 8)) + ((idx_row_y + offset_row) * fblinelen);
				
				// LOGDEBUG("!!! x:%d y:%d !! pix_idx:%06X pix_byte:%04X pix_bit:%02X pixel: %02X, loc:%08X",idx_col_x, idx_row_y, pixel_byte_idx, pixel_byte, pixel_bit, pixel, loc);

				for(int i=0 ; i< (this->fbbpp/8); i++){
					*(this->fb + loc + i) = pixel;
				}
			}
		}

		
		// LOGWARN("%s start row: %06X col: %06X", __PRETTY_FUNCTION__, (unsigned int)offset_row, (unsigned int)offset_col);

		// for (DWORD pixel_x = offset_col; pixel_x < (offset_col + (update_width_x_bytes * 8)); offset_col++){
		// 	for (DWORD pixel_y = offset_row; pixel_y < offset_row + (update_height_y_bytes); offset_row++){

		// 		BYTE pixel_byte = (pixel_y * update_height_y_bytes) + (pixel_x/8);
		// 		int pixel_bit = (pixel_x % 8);
		// 		pixel_value = ((buf[pixel_byte]) << pixel_bit) != 0;

		// 		int loc = (pixel_x * (this->fbbpp / 8)) + (pixel_y * m_powerview_resolution_x);

		// 		// int idx = (pixel_y * update_width_x_bytes * 8);
		// 		// pixel_value = buf[]

		// 		*(this->fb + loc) = (pixel_bit) ? 0 : 255;
		// 		*(this->fb + loc + 1) = (pixel_bit) ? 0 : 255;
		// 		// *(this->fb + loc + 2) = (j & (1 << bit)) ? 0 : 255;


		// 	}


		// }




		// for (DWORD i = 0; i < length; i++) {
		// 	unsigned char j = reverse_table[buf[i]];

		// 	for (int bit = 0; bit < 8; bit++) {
		// 		int loc = (offset_col * (this->fbbpp / 8)) + (offset_row * this->fblinelen);
		// 		offset_col++;
		// 		// if (col % this->screen_width == 0) {
		// 		if (offset_col % update_width_x == 0) {
		// 			offset_col = 0;
		// 			offset_row++;
		// 		}

		// 		*(this->fb + loc) = (j & (1 << bit)) ? 0 : 255;
		// 		*(this->fb + loc + 1) = (j & (1 << bit)) ? 0 : 255;
		// 		*(this->fb + loc + 2) = (j & (1 << bit)) ? 0 : 255;
		// 	}
		// }
	// }

	// catch(const exception& e) {
	// 	LOGWARN("Exception!!!!!!!!!");
	// }

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