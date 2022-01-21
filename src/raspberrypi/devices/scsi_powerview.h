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
#pragma once

#include "os.h"
#include "disk.h"
#include <map>
#include <string>
#include "../rascsi.h"
#include <linux/fb.h>

//===========================================================================
//
//	Radius PowerView
//
//===========================================================================
class SCSIPowerView: public Disk
{

private:
	typedef struct _command_t {
		const char* name;
		void (SCSIPowerView::*execute)(SASIDEV *);

		_command_t(const char* _name, void (SCSIPowerView::*_execute)(SASIDEV *)) : name(_name), execute(_execute) { };
	} command_t;
	std::map<SCSIDEV::scsi_command, command_t*> commands;

	SASIDEV::ctrl_t *ctrl;

	enum eColorDepth_t : uint16_t {eColorsNone=0x0000, eColorsBW=0x0001, eColors16=0x0010, eColors256=0x0100};


	void AddCommand(SCSIDEV::scsi_command, const char*, void (SCSIPowerView::*)(SASIDEV *));

	// Largest framebuffer supported by the Radius PowerView is 800x600 at 256 colors (1 bytes per pixel)
	// Double it for now, because memory is cheap.... 
	// TODO: Remove the "*2" from this
	const int POWERVIEW_BUFFER_SIZE = (800 * 600 * 2); 

	void fbcon_cursor(bool blank);
	void fbcon_blank(bool blank);
	void fbcon_text(char* message);

public:
	SCSIPowerView();
	~SCSIPowerView();

	bool Init(const map<string, string>&) override;

	// // Commands
	int Inquiry(const DWORD *cdb, BYTE *buffer) override;
	bool WriteFrameBuffer(const DWORD *cdb, const BYTE *buf, const DWORD length);
	bool WriteColorPalette(const DWORD *cdb, const BYTE *buf, const DWORD length);
	bool WriteConfiguration(const DWORD *cdb, const BYTE *buf, const DWORD length);
	bool WriteUnknownCC(const DWORD *cdb, const BYTE *buf, const DWORD length);
	
	void CmdReadConfig(SASIDEV *controller);
	void CmdWriteConfig(SASIDEV *controller);
	void CmdWriteFramebuffer(SASIDEV *controller);
	void CmdWriteColorPalette(SASIDEV *controller);
	void UnknownCommandCC(SASIDEV *controller);
	
	bool Dispatch(SCSIDEV *) override;

private:

	void ClearFrameBuffer(DWORD blank_color);

	// Default to the lowest resolution supported by the PowerView (640x400)
    uint32_t screen_width_px = 640;
	uint32_t screen_height_px = 400;
	eColorDepth_t color_depth = eColorsNone;
	
	struct fb_var_screeninfo fbinfo;
	struct fb_fix_screeninfo fbfixinfo;

	// The maximum color depth is 16 bits
	BYTE color_palette[0x10000];
	int color_palette_length = 0;

	BYTE unknown_cc_data[0x10000];
	int unknown_cc_data_length = 0;

	static const BYTE default_color_palette_bw[8];
	static const BYTE default_color_palette_16[64];
	static const BYTE default_color_palette_256[1024];

	int fbfd;
	char *fb;
	int fblinelen;
	int fbbpp;

	// Hard-coded inquiry response to match the real PowerView
	static const BYTE m_inquiry_response[];

	static unsigned char reverse_table[256];

	DWORD framebuffer_black;
	DWORD framebuffer_blue;
	DWORD framebuffer_yellow;
	DWORD framebuffer_red;
};
