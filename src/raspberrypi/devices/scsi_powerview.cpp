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

unsigned char SCSIPowerView::reverse_table[256];

const BYTE SCSIPowerView::m_inquiry_response[] = {
0x03, 0x00, 0x01, 0x01, 0x46, 0x00, 0x00, 0x00, 0x52, 0x41, 0x44, 0x49, 0x55, 0x53, 0x20, 0x20,
0x50, 0x6F, 0x77, 0x65, 0x72, 0x56, 0x69, 0x65, 0x77, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
0x56, 0x31, 0x2E, 0x30, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x20, 0x00, 0x01, 0x00, 0x00, 0x00,
0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00,
0x05, 0x00, 0x00, 0x00, 0x00, 0x06, 0x43, 0xF9, 0x00, 0x00, 0xFF,
};

SCSIPowerView::SCSIPowerView() : Disk("SCPV")
{
	LOGTRACE("Entering %s", __PRETTY_FUNCTION__);
	AddCommand(SCSIDEV::eCmdPvReadConfig, "Unknown PowerViewC8", &SCSIPowerView::CmdReadConfig);
	AddCommand(SCSIDEV::eCmdPvWriteConfig, "Unknown PowerViewC9", &SCSIPowerView::CmdWriteConfig);
	AddCommand(SCSIDEV::eCmdPvWriteFrameBuffer, "Unknown PowerViewCA", &SCSIPowerView::CmdWriteFramebuffer);
	AddCommand(SCSIDEV::eCmdPvWriteColorPalette, "Unknown PowerViewCB", &SCSIPowerView::CmdWriteColorPalette);
	AddCommand(SCSIDEV::eCmdUnknownPowerViewCC, "Unknown PowerViewCC", &SCSIPowerView::UnknownCommandCC);
	
	LOGTRACE("%s - creating lookup table", __PRETTY_FUNCTION__);

	// create lookup table
	for (int i = 0; i < 256; i++) {
		unsigned char b = i;
		b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
		b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
		b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
		reverse_table[i] = b;
	}

	framebuffer_black = 0;
	framebuffer_blue =  (/*red*/ 0 << fbinfo.red.offset) |
						(/*green*/ 0 << fbinfo.green.offset)|
						(/*blue*/ 0xFF << fbinfo.blue.offset) |
						(/*alpha*/ 0 << fbinfo.transp.offset);
	framebuffer_yellow =  (/*red*/ 0 << fbinfo.red.offset) |
						(/*green*/ 0xFF << fbinfo.green.offset)|
						(/*blue*/ 0x0 << fbinfo.blue.offset) |
						(/*alpha*/ 0 << fbinfo.transp.offset);
	framebuffer_red =  (/*red*/ 0xFF << fbinfo.red.offset) |
						(/*green*/ 0 << fbinfo.green.offset)|
						(/*blue*/ 0 << fbinfo.blue.offset) |
						(/*alpha*/ 0 << fbinfo.transp.offset);

	// Default to one bit color (black & white) if we don't know any better
	color_depth = eColorsBW;
	LOGTRACE("Done with %s", __PRETTY_FUNCTION__);
}

//---------------------------------------------------------------------------
//
//	Log a message to the framebuffer display
//
//---------------------------------------------------------------------------
void SCSIPowerView::fbcon_text(char* message)
{
	int fd = open("/dev/tty1", O_RDWR);
	if (0 < fd)
	{
		write(fd, message, strlen(message));
	}
	else
	{
		LOGWARN("%s Unable to open /dev/tty1", __PRETTY_FUNCTION__);
	}
	close(fd);
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

	munmap(this->fb, fbfixinfo.smem_len);
	close(this->fbfd);

	for (auto const& command : commands) {
		delete command.second;
	}
}

void SCSIPowerView::AddCommand(SCSIDEV::scsi_command opcode, const char* name, void (SCSIPowerView::*execute)(SASIDEV *))
{
	commands[opcode] = new command_t(name, execute);
}


void SCSIPowerView::ClearFrameBuffer(DWORD blank_color){

		// For each row
		for (DWORD idx_row_y = 0; idx_row_y < fbinfo.yres-1; idx_row_y++){

			// For each column
			for (DWORD idx_col_x = 0; idx_col_x < fbinfo.xres-1; idx_col_x++){
		 		uint32_t loc = ((idx_col_x) * (this->fbbpp / 8)) + ((idx_row_y) * fblinelen);
				 uint8_t temp_color = blank_color;

				for(uint32_t i=0; i < fbinfo.bits_per_pixel/8; i++){
					temp_color = (blank_color >> 8*i) & 0xFF;
					*(this->fb + loc + i) = temp_color;
				}
			}
		}


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
//	Unknown Command C9 - Write a configuration value?
//
//---------------------------------------------------------------------------
void SCSIPowerView::CmdWriteConfig(SASIDEV *controller)
{

	// Set transfer amount
	ctrl->length = ctrl->cmd[6];
	LOGTRACE("%s Message Length %d", __PRETTY_FUNCTION__, (int)ctrl->length);

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
//	Command to Write a Frame Buffer update
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

	LOGTRACE("%s Message Length %d [%08X]", __PRETTY_FUNCTION__, (int)ctrl->length, (unsigned int)ctrl->length);

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
//	Receive a new color palette from the host
//
//---------------------------------------------------------------------------
void SCSIPowerView::CmdWriteColorPalette(SASIDEV *controller)
{

	eColorDepth_t received_color_depth = (eColorDepth_t)((uint16_t)ctrl->cmd[4] + ((uint16_t)ctrl->cmd[3] << 8));
	ctrl->length = (uint16_t)received_color_depth * 4;
	LOGTRACE("%s Message Length %d", __PRETTY_FUNCTION__, (int)ctrl->length);

	// The driver sends "1" for black and white, which is really 
	// TWO colors: Black and White
	if(received_color_depth == eColorsBW){
		ctrl->length = 8;
	}

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
	ctrl->length = 0x8bb;
	LOGTRACE("%s Message Length %d", __PRETTY_FUNCTION__, (int)ctrl->length);
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
	LOGTRACE("Entering %s", __PRETTY_FUNCTION__);
	SetParams(params.empty() ? GetDefaultParams() : params);

	this->fbfd = open("/dev/fb0", O_RDWR);
	if (this->fbfd == -1){
		LOGWARN("Unable to open /dev/fb0. Do you have a monitor connected?");
		return false;
	}
	LOGTRACE("%s - /dev/fb0 opened", __PRETTY_FUNCTION__);

	if (ioctl(this->fbfd, FBIOGET_VSCREENINFO, &fbinfo)){
		LOGERROR("Unable to retrieve framebuffer v screen info. Are you running as root?");
		return false;
	}

	if (ioctl(this->fbfd, FBIOGET_FSCREENINFO, &fbfixinfo)){
		LOGERROR("Unable to retrieve framebuffer f screen info. Are you running as root?");
		return false;
	}

	if (fbinfo.bits_per_pixel != 16){
		LOGERROR("PowerView emulation only supports 16 bits per pixel framebuffer. Currently set to %d", fbinfo.bits_per_pixel);
		LOGERROR("Try running \"fbset -depth 16\"");
		return false;
	}

	LOGTRACE("%s Done capturing FB info", __PRETTY_FUNCTION__);

	fbbpp = fbinfo.bits_per_pixel;
	fblinelen = fbfixinfo.line_length;

	fb = (char *)mmap(0, fbfixinfo.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
	if (fb == (char*)-1){
		LOGERROR("Unable to mmap the framebuffer memory. Are you running as root?");
		return false;
	}
	LOGTRACE("%s mmap complete", __PRETTY_FUNCTION__);

	// Clear the framebuffer
	memset(this->fb, 0, fbfixinfo.smem_len);
	LOGTRACE("%s done clearing framebuffer", __PRETTY_FUNCTION__);

	// Hide the flashing cursor on the screen
	fbcon_cursor(false);

	// Disable the automatic screen blanking (screen saver)
	fbcon_blank(false);
	LOGTRACE("%s Done disable blanking", __PRETTY_FUNCTION__);

	// Draw a blue frame around the display area
	// This indicates that the PowerView simulation is running
	ClearFrameBuffer(framebuffer_blue);

	// Report status to the screen
	char status_string[256];
	sprintf(status_string, "PowerView initialized on %dx%d %d bpp framebuffer\r", fbinfo.xres, fbinfo.yres, fbinfo.bits_per_pixel);
	fbcon_text(status_string);
	LOGINFO(status_string);

	return true;
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
		LOGERROR("%s Received Unknown CC data that is larger (%d bytes) than allocated size (%d bytes)", __PRETTY_FUNCTION__, length, (int)sizeof(unknown_cc_data));
		return false;
	}

	LOGINFO("%s Unknown CC of size %ul received", __PRETTY_FUNCTION__, length);
	memcpy(unknown_cc_data, buf, length);
	unknown_cc_data_length = length;
	return true;
}


bool SCSIPowerView::WriteColorPalette(const DWORD *cdb, const BYTE *buf, const DWORD length)
{

	if(length <= 1){
		LOGDEBUG("%s Received Color Palette with depth of %u", __PRETTY_FUNCTION__, length);
		// This is still a valid condition.
		return true;
	}
	if(length > sizeof(color_palette)){
		LOGERROR("%s Received Color Palette that is larger (%d bytes) than allocated size (%d bytes)", __PRETTY_FUNCTION__, length, (int)sizeof(color_palette));
		return false;
	}

	eColorDepth_t new_color = (eColorDepth_t)((uint16_t)cdb[4] + ((uint16_t)cdb[3] << 8));

	if(new_color == eColorDepth_t::eColorsNone){
		// This is currently unknown functionality. So, just ignore it.
		return true;
	}
	
	ClearFrameBuffer(framebuffer_red);

	color_depth = new_color;

	switch(color_depth){
		case eColorsBW:
	memcpy(color_palette, default_color_palette_bw, sizeof(default_color_palette_bw));
		break;
		case eColors16:
	memcpy(color_palette, default_color_palette_16, sizeof(default_color_palette_16));
		break;
		case eColors256:
	memcpy(color_palette, default_color_palette_256, sizeof(default_color_palette_256));
		break;
		default:
		LOGWARN("UNHANDLED COLOR DEPTH: %04X", color_depth);
		break;
	}

	// memcpy(color_palette, buf, length);
	// color_palette_length = length;
	// memcpy(color_palette, default_color_palette_256, sizeof(default_color_palette_256));

	LOGINFO("%s Color type: %04x Palette of size %ul received", __PRETTY_FUNCTION__, (uint16_t)cdb[4] + ((uint16_t)cdb[3] << 8), length);

#ifdef DUMP_COLOR_PALETTE
	FILE *fp;
	char newstring[1024];
	static int dump_idx = 0;

	switch(color_depth){
		case(eColorsBW):
			sprintf(newstring, "/tmp/eColorsBW_%04X.txt",dump_idx++);
			break;
		case(eColors16):
			sprintf(newstring, "/tmp/eColors16_%04X.txt",dump_idx++);
			break;
		case(eColors256):
			sprintf(newstring, "/tmp/eColors256_%04X.txt",dump_idx++);
			break;
		default:
			return false;
			break;
	}
	//LOGWARN(newstring);
	fp = fopen(newstring,"w");

	sprintf(newstring, "length: %u\n", length);

	fputs(newstring, fp);

	for(DWORD i = 0; i < length; i+=4){
		if(i % 16 == 0){
			sprintf(newstring, "%u: ", i);
			fputs(newstring,fp);
		}

		sprintf(newstring, "[%02X %02X%02X%02X]  ", buf[i+0],buf[i+1],buf[i+2],buf[i+3]);
		fputs(newstring, fp);

		if(i % 16 == 12){

			fputs("\n", fp);
		}
	}
	fclose(fp);
#endif

	return true;
}


bool SCSIPowerView::WriteFrameBuffer(const DWORD *cdb, const BYTE *buf, const DWORD length)
{
	char newstring[1024];
    // uint32_t new_screen_width_px =0;
	// uint32_t new_screen_height_px=0;
	uint32_t update_width_px=0;
	uint32_t update_height_px=0;
	uint32_t offset_row_px = 0;
	uint32_t offset_col_px = 0;

		// Set transfer amount
	uint32_t update_width_x_bytes = ctrl->cmd[5] + (ctrl->cmd[4] << 8);
	uint32_t update_height_y_bytes = ctrl->cmd[7] + (ctrl->cmd[6] << 8);
	uint32_t offset = (uint32_t)ctrl->cmd[3] + ((uint32_t)ctrl->cmd[2] << 8) + ((uint32_t)ctrl->cmd[1] << 16);

	bool full_framebuffer_refresh = (offset == 0) && (cdb[9] == 0x00);


	switch(color_depth){
		case(eColorsBW):
			// One bit per pixel
			update_width_px = update_width_x_bytes * 8;
			offset_row_px = (uint32_t)((offset*8) / screen_width_px)/2;
			offset_col_px = (uint32_t)(((offset*8)/2) % screen_width_px);
			break;
		case(eColors16):
			// One byte per pixel
			update_width_px = update_width_x_bytes * 2;
			offset_row_px = (uint32_t)((offset*2) / screen_width_px)/2;
			offset_col_px = (uint32_t)(((offset*2)/2) % screen_width_px);
			break;
		case(eColors256):
			// Two Bytes per pixel
			update_width_px = update_width_x_bytes;
			offset_row_px = (uint32_t)(offset / screen_width_px);
			offset_col_px = (uint32_t)(offset % screen_width_px);
			break;
		default:
			update_width_px = 0;
			offset_row_px = 0;
			offset_col_px = 0;
			LOGINFO("No color depth available! %04X", color_depth);
			break;
	}
	update_height_px = update_height_y_bytes;


	if(full_framebuffer_refresh){
		screen_width_px = update_width_px;
		screen_height_px = update_height_px;

		switch(color_depth){
			case eColorDepth_t::eColorsBW:
				sprintf(newstring, "  Black & White %04X - %u x %u                              \r", color_depth, screen_width_px, screen_height_px);
				break;
			case eColorDepth_t::eColors16:
				sprintf(newstring, "  16 colors/grays %04X - %u x %u                              \r", color_depth, screen_width_px,screen_height_px );
				break;
			case eColorDepth_t::eColors256:
				sprintf(newstring, "  256 colors/grays %04X - %u x %u                              \r", color_depth, screen_width_px, screen_height_px);
				break;
			default:
				sprintf(newstring, "  UNKNOWN COLOR DEPTH!! %04X - %u x %u                              \r", color_depth, screen_width_px, screen_height_px);
				break;
		}
		fbcon_text(newstring);

	}

	LOGTRACE("Calculate Offset: %u (%08X) Screen width: %u height: %u Update X: %u (%06X) Y: %u (%06X)", offset, offset, screen_width_px, screen_height_px, offset_row_px, offset_row_px, offset_col_px, offset_col_px);

	if(update_width_px == 0){
		// This is some weird error condition. For now, just return.
		LOGWARN("update of size 0 received, or there is no color depth");
		return true;
	}

	if(update_height_px + offset_row_px > 800){
		LOGWARN("%s Height: (%d + %d) = %d is larger than the limit", __PRETTY_FUNCTION__, update_height_px, offset_row_px, update_height_px + offset_row_px);
		return true;
	}
	if(update_width_px + offset_col_px > 800){
		// We can get into this condition when the color depth has not been set by the host. We can try
		// to derive it based upon the screen width
		if(color_depth == eColorsBW){
			if((update_width_px == (800 * 8)) || (update_width_px == (640 * 8)) || (update_width_px == (600 * 8))){
				color_depth = eColors256;
				// We don't know if we're 256 grays or 256 colors. So, this could be goofy looking
				// We're down in an error condition anyway.
				LOGINFO("Based upon size of screen update, switching to 256 colors. This may be incorrect if the screen is supposed to be 256 grays");
				memcpy(color_palette, default_color_palette_256, sizeof(default_color_palette_256));
				// Try it again.... (Re-call this function)
				return WriteFrameBuffer(cdb, buf, length);
			}
			if((update_width_px == (800 * 2)) || (update_width_px == (640 * 2)) || (update_width_px == (600 * 2))){
				color_depth = eColors16;
				LOGINFO("Based upon size of screen update, switching to 16 colors. This may be incorrect if the screen is supposed to be 16 grays");
				memcpy(color_palette, default_color_palette_16, sizeof(default_color_palette_16));
				// Try it again.... (Re-call this function)
				return WriteFrameBuffer(cdb, buf, length);
			}
		}

		LOGWARN("%s width: (%d + %d) = %d is larger than the limit", __PRETTY_FUNCTION__, update_width_px, offset_col_px, update_width_px + offset_col_px);
		return true;
	}

	LOGDEBUG("Update Position: %d:%d Offset: %d Screen Width: %d Width px: %d:%d (bytes: %d, %d)", offset_col_px, offset_row_px, offset, screen_width_px, update_width_px, update_height_px, update_width_x_bytes, update_height_y_bytes);

	DWORD random_print = rand() % (update_height_px * update_width_px);

	// For each row
	for (DWORD idx_row_y = 0; idx_row_y < (update_height_px); idx_row_y++){

		// For each column
		for (DWORD idx_col_x = 0; idx_col_x < (update_width_px); idx_col_x++){
			BYTE pixel_color_idx=0;
			DWORD pixel_buffer_idx = 0;
			BYTE pixel_buffer_byte = 0;
			DWORD pixel_bit_number = 0;
			uint32_t loc = 0;

			switch(color_depth){
				case eColorDepth_t::eColorsBW:
					pixel_buffer_idx = (idx_row_y * update_width_x_bytes) + (idx_col_x / 8);
					pixel_buffer_byte = reverse_table[buf[pixel_buffer_idx]];
					pixel_bit_number = idx_col_x % 8;
					pixel_color_idx = ((pixel_buffer_byte >> pixel_bit_number) & 0x1) ? 0 : 1;
					break;

				case eColorDepth_t::eColors16:
					pixel_buffer_idx = (idx_row_y * update_width_x_bytes) + (idx_col_x/2);
					if(idx_col_x % 2){
						pixel_color_idx = buf[pixel_buffer_idx] & 0x0F;
					}
					else{
						pixel_color_idx = buf[pixel_buffer_idx] >> 4;
					}
					break;
				case eColorDepth_t::eColors256:
					pixel_buffer_idx = (idx_row_y * update_width_x_bytes) + (idx_col_x);
					pixel_color_idx = buf[pixel_buffer_idx];
					break;
				default:
					break;
			}

			if(pixel_color_idx > (sizeof(color_palette) / sizeof (color_palette[0]))){
				LOGWARN("Calculated pixel color that is out of bounds: %04X", pixel_buffer_idx);
				return false;
			}

			loc = ((idx_col_x + offset_col_px) * (this->fbbpp / 8)) + ((idx_row_y + offset_row_px) * fblinelen);

			uint32_t data_idx, red, green, blue;

			data_idx = (uint32_t)color_palette[(pixel_color_idx*4)];
			red = (uint32_t)color_palette[(pixel_color_idx*4)+1];
			green = (uint32_t)color_palette[(pixel_color_idx*4)+2];
			blue = (uint32_t)color_palette[(pixel_color_idx*4)+3];

			blue >>= (8 - fbinfo.red.length);
			green >>= (8 - fbinfo.green.length);
			red >>= (8 - fbinfo.blue.length);

			uint32_t fb_pixel = (red << fbinfo.red.offset) |
								(green << fbinfo.green.offset)|
								(blue << fbinfo.blue.offset);

			if(random_print == (idx_col_x * idx_row_y)){
				LOGDEBUG("idx:%d (%02X) [%02X %02X %02X %02X] fb_pixel:%08X ", pixel_color_idx, pixel_color_idx, data_idx, red, green, blue, fb_pixel);
			}

			*(this->fb + loc + 1) = (BYTE)((fb_pixel >> 8) & 0xFF);
			*(this->fb + loc + 0) = (BYTE)((fb_pixel) & 0xFF);

			}
		}

#ifdef DUMP_FRAME_BUFFER
	//******************************************************************************
	FILE *fp;

	static int dumpfb_idx = 0;

	switch(color_depth){
		case(eColorsBW):
			sprintf(newstring, "/tmp/fb_eColorsBW_%04X_%ux%u.txt",dumpfb_idx++, update_width_x_bytes, update_height_y_bytes);
			break;
		case(eColors16):
			sprintf(newstring, "/tmp/fb_eColors16_%04X_%ux%u.txt",dumpfb_idx++, update_width_x_bytes, update_height_y_bytes);
			break;
		case(eColors256):
			sprintf(newstring, "/tmp/fb_eColors256_%04X_%ux%u.txt",dumpfb_idx++, update_width_x_bytes, update_height_y_bytes);
			break;
		default:
			return false;
			break;
	}
	//LOGWARN(newstring);
	fp = fopen(newstring,"w");

	sprintf(newstring, "length: %u\n", length);

	fputs(newstring, fp);

	for(DWORD i = 0; i < length; i+=4){
		if(i % 16 == 0){
			sprintf(newstring, "%u: ", i);
			fputs(newstring,fp);
		}

		sprintf(newstring, "[%02X %02X %02X %02X] ", buf[i+0],buf[i+1],buf[i+2],buf[i+3]);
		fputs(newstring, fp);

		if(i % 16 == 12){

			fputs("\n", fp);
		}
	}
	fclose(fp);
#endif


	//******************************************************************************

	return true;
}
