//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2014-2018 GIMONS
//
//	[ SCSI共通 ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "scsi.h"

//---------------------------------------------------------------------------
//
//	フェーズ取得
//
//---------------------------------------------------------------------------
BUS::phase_t FASTCALL BUS::GetPhase()
{
	DWORD mci;

	ASSERT(this);

	// セレクションフェーズ
	if (GetSEL()) {
		return selection;
	}

	// バスフリーフェーズ
	if (!GetBSY()) {
		return busfree;
	}

	// バスの信号線からターゲットのフェーズを取得
	mci = GetMSG() ? 0x04 : 0x00;
	mci |= GetCD() ? 0x02 : 0x00;
	mci |= GetIO() ? 0x01 : 0x00;
	return GetPhase(mci);
}

//---------------------------------------------------------------------------
//
//	フェーズテーブル
//
//---------------------------------------------------------------------------
const BUS::phase_t BUS::phase_table[8] = {
	dataout,
	datain,
	command,
	status,
	reserved,
	reserved,
	msgout,
	msgin
};
