//---------------------------------------------------------------------------
//
//	SCSI Target Emulator PiSCSI for Raspberry Pi
//
//  Copyright (C) 2020-2023 akuker
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
#pragma once

#include "interfaces/byte_writer.h"
#include "primary_device.h"

#include <map>
#include <string>
#include <linux/fb.h>

//===========================================================================
//
//	Radius PowerView
//
//===========================================================================
class SCSIPowerView : public PrimaryDevice, public ByteWriter
{

private:
	// typedef struct _command_t {
	// 	const char* name;
	// 	void (SCSIPowerView::*execute)(SASIDEV *);

	// 	_command_t(const char* _name, void (SCSIPowerView::*_execute)(SASIDEV *)) : name(_name), execute(_execute) { };
	// } command_t;
	// std::map<SCSIDEV::scsi_command, command_t*> commands;

	// SASIDEV::ctrl_t *ctrl;

	enum eColorDepth_t : uint16_t {eColorsNone=0x0000, eColorsBW=0x0001, eColors16=0x0010, eColors256=0x0100};


	// void AddCommand(SCSIDEV::scsi_command, const char*, void (SCSIPowerView::*)(SASIDEV *));

	// Largest framebuffer supported by the Radius PowerView is 800x600 at 256 colors (1 bytes per pixel)
	// Double it for now, because memory is cheap.... 
	// TODO: Remove the "*2" from this
	const int POWERVIEW_BUFFER_SIZE = (800 * 600 * 2); 

	void fbcon_cursor(bool blank);
	void fbcon_blank(bool blank);
	void fbcon_text(const char* message);

public:


	explicit SCSIPowerView(int);
	~SCSIPowerView() override;

	bool Init(const unordered_map<string, string>&) override;
	bool HwInit();


	// Commands
	vector<uint8_t> InquiryInternal() const override;
	// int Read(const vector<int>&, vector<uint8_t>&, uint64_t);
	bool WriteBytes(const vector<int>&, vector<uint8_t>&, uint32_t) override;

	// int RetrieveStats(const vector<int>&, vector<uint8_t>&) const;

	// void TestUnitReady() override;
	// void Read6();
	// void Write6() const;



	// // Commands
	// old..... int Inquiry(const uint32_t *cdb, uint8_t *buffer) override;
	bool WriteFrameBuffer();
	bool WriteColorPalette();
	bool WriteConfiguration();
	bool WriteUnknownCC();
	
	void CmdReadConfig();
	void CmdWriteConfig();
	void CmdWriteFramebuffer();
	void CmdWriteColorPalette();
	void UnknownCommandCC();

private:

	void ClearFrameBuffer(uint32_t blank_color);

	// Default to the lowest resolution supported by the PowerView (640x400)
    uint32_t screen_width_px = 640;
	uint32_t screen_height_px = 400;
	eColorDepth_t color_depth = eColorsNone;
	
	struct fb_var_screeninfo fbinfo;
	struct fb_fix_screeninfo fbfixinfo;

	// The maximum color depth is 16 bits
	uint8_t color_palette[0x10000];
	// int color_palette_length = 0;

	uint8_t unknown_cc_data[0x10000];
	int unknown_cc_data_length = 0;

	static const uint8_t default_color_palette_bw[8];
	static const uint8_t default_color_palette_16[64];
	static const uint8_t default_color_palette_256[1024];

	int fbfd;
	char *fb;
	int fblinelen;
	int fbbpp;

	// Hard-coded inquiry response to match the real PowerView
	static const uint8_t m_inquiry_response[];

	static unsigned char reverse_table[256];

	uint32_t framebuffer_black;
	uint32_t framebuffer_blue;
	uint32_t framebuffer_yellow;
	uint32_t framebuffer_red;
};
