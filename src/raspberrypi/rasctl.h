//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//	Copyright (C) akuker
//
//	[ Send Control Command ]
//
//---------------------------------------------------------------------------


enum rasctl_dev_type : int {
	rasctl_dev_invalid      = -1,
	rasctl_dev_sasi_hd      =  0,
	rasctl_dev_scsi_hd      =  1,
	rasctl_dev_mo           =  2,
	rasctl_dev_cd           =  3,
	rasctl_dev_br           =  4,
	rasctl_dev_nuvolink     =  5,
	rasctl_dev_daynaport    =  6,	
};
