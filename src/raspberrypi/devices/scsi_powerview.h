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

	enum eColorDepth_t : uint16_t {eColorsNone=0x0001, eColorsBW=0x0002, eColors16=0x0010, eColors256=0x0100};

	eColorDepth_t color_depth;

	void AddCommand(SCSIDEV::scsi_command, const char*, void (SCSIPowerView::*)(SASIDEV *));

	void dump_command(SASIDEV *controller);

	// Largest framebuffer supported by the Radius PowerView is 800x600 at 16-bit color (2 bytes per pixel)
	const int POWERVIEW_BUFFER_SIZE = (800 * 600 * 2); 

	DWORD m_powerview_resolution_x;
	DWORD m_powerview_resolution_y;

	

	void fbcon_cursor(bool blank);
	void fbcon_blank(bool blank);
	void fbcon_text(char* message);
	void SetPixel(uint32_t x, uint32_t y, uint32_t r, uint32_t g, uint32_t b);


public:
	SCSIPowerView();
	~SCSIPowerView();

	bool Init(const map<string, string>&) override;
	void Open(const Filepath& path) override;

	// // Commands
	int Inquiry(const DWORD *cdb, BYTE *buffer) override;
	int Read(const DWORD *cdb, BYTE *buf, uint64_t block) override;
	bool Write(const DWORD *cdb, const BYTE *buf, const DWORD length);
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

    bool ReceiveBuffer(int len, BYTE *buffer);

	void ClearFrameBuffer(DWORD blank_color);

private:

    uint32_t screen_width_px;
	uint32_t screen_height_px;

	
	struct fb_var_screeninfo fbinfo;
	struct fb_fix_screeninfo fbfixinfo;

	// The maximum color depth is 16 bits
	DWORD color_palette[0x10000];
	int color_palette_length = 0;

	BYTE unknown_cc_data[0x10000];
	int unknown_cc_data_length = 0;


	int fbfd;
	char *fb;
	int fbwidth;
	int fbheight;
	int fblinelen;
	int fbsize;
	int fbbpp;

	static const BYTE m_inquiry_response[];

	DWORD framebuffer_black;
	DWORD framebuffer_blue;
	DWORD framebuffer_yellow;
	DWORD framebuffer_red;
};
