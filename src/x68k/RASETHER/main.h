//---------------------------------------------------------------------------
//
//  SCSI Target Emulator RaSCSI (*^..^*)
//  for Raspberry Pi
//
//  Powered by XM6 TypeG Technorogy.
//  Copyright (C) 2016-2017 GIMONS
//	[ RaSCSI イーサーネット メイン ]
//
//  Based on
//    Neptune-X board driver for  Human-68k(ESP-X)   version 0.03
//    Programed 1996-7 by Shi-MAD.
//    Special thanks to  Niggle, FIRST, yamapu ...
//
//---------------------------------------------------------------------------

#ifndef main_h
#define main_h

#define ID_EN0	(('e'<<24)+('n'<<16)+('0'<<8))
#undef MULTICAST

// number of protocol type we can handle		**EDIT this**
#define	NPRT		(16)

// number of multicast address we can handle		**EDIT this**
#define NMULTICAST	(64)
/* ただし、まだマルチキャストには対応していない */

struct eaddr {
  unsigned char eaddr [6];
};

typedef void (*int_handler) (int, void*, int);
typedef void (*malloc_func) (unsigned int*);

struct prt {
  int type;
  int_handler func;
  malloc_func malloc;
};


// グローバル変数
extern unsigned int scsiid;
extern int trap_no;
extern int num_of_prt;
extern struct prt PRT_LIST [NPRT];

#ifdef MULTICAST
extern int num_of_multicast;
extern struct eaddr multicast_array [NMULTICAST];
#endif


// プロトタイプ宣言
extern int Initialize (void);
extern int InitList (int);
extern int AddList (int, int_handler);
extern int DeleteList (int);
extern int_handler SearchList (int);
extern int_handler SearchList2 (int, int);
extern malloc_func GetMallocFunc (int, int);

#ifdef MULTICAST
extern int AddMulticastArray (struct eaddr*);
extern void DelMulticastArray (struct eaddr*);
extern void MakeMulticastTable (unsigned char*);
#endif


//  ne.s 内関数
extern void trap_entry (void);


#include <sys/iocs.h>
#define Print _iocs_b_print

#define TRAP_VECNO(n)	(0x20 + n)

#endif // main_h
