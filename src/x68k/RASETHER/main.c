//---------------------------------------------------------------------------
//
//  SCSI Target Emulator RaSCSI (*^..^*)
//  for Raspberry Pi
//
//  Powered by XM6 TypeG Technology.
//  Copyright (C) 2016-2017 GIMONS
//	[ RaSCSI Ethernet Main ]
//
//  Based on
//    Neptune-X board driver for  Human-68k(ESP-X)   version 0.03
//    Programed 1996-7 by Shi-MAD.
//    Special thanks to  Niggle, FIRST, yamapu ...
//
//---------------------------------------------------------------------------

#include <stdio.h>
#include <string.h>
#include <sys/dos.h>
#include "main.h"
#include "scsictl.h"

unsigned int scsiid;
int trap_no;
int num_of_prt;
struct prt PRT_LIST[NPRT];

// �}���`�L���X�g�i���Ή��j
#ifdef MULTICAST
int num_of_multicast;
struct eaddr multicast_array[NMULTICAST];
#endif

/************************************************
 *												*
 ************************************************/
static int sprint_eaddr(unsigned char* dst, void* e)
{
	unsigned char* p = e;
	
	return sprintf(
		dst,
		"%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x",
		p[0], p[1], p[2], p[3], p[4], p[5]);
}

/************************************************
 *	TRAP n���g�p�\�����ׂ�					*
 ************************************************/
static int is_valid_trap(int trap_no)
{
	unsigned int addr;

	if (trap_no < 0 || 7 < trap_no) {
		return 0;
	}

	// �g�p�����ǂ����`�F�b�N
	addr = (unsigned int)_dos_intvcg(TRAP_VECNO(trap_no));

	// �����A�h���X�̍ŏ�ʃo�C�g�Ƀx�N�^�ԍ��������Ă���Ζ��g�p
	if (addr & 0xff000000) {
		return -1;
	}

	return 0;
}

/************************************************
 *	���g�p��TRAP n������						*
 ************************************************/
static int search_trap_no(int def)
{
	int i;

	// ����def���g�p�\�Ȃ炻��Ɍ���
	if (is_valid_trap(def)) {
		return def;
	}

	for (i = 0; i <= 6; i++) {
		if (is_valid_trap(i)) {
			return i;
		}
	}

	return -1;
}

/************************************************
 *	vector set									*
 ************************************************/
static void* trap_vector(int trap_no, void *func)
{
	return _dos_intvcs(TRAP_VECNO(trap_no), func);
}

/************************************************
  �������֐��ine.s�C�j�V�����C�Y�ŌĂт܂��j	*
  ************************************************/
int Initialize(void)
{
	unsigned char buff[128];
	unsigned char buff2[32];
	struct eaddr ether_addr;

	if (SearchRaSCSI())
	{
		Print("RaSCSI Ether Adapter �̑��݂��m�F�ł��܂���ł���\r\n");
		return -1;
	}

	if (InitList(NPRT)
		|| InitRaSCSI())
	{
		Print("RaSCSI Ether Adapter Driver �̏������Ɏ��s���܂���\r\n");
		return -1;
	}

	memset(&ether_addr, 0x00, sizeof(ether_addr));
	GetMacAddr(&ether_addr);

	// ���g�ptrap�ԍ��𒲂ׂ�i�w��ԍ��D��j
	if (trap_no >= 0) {
		trap_no = search_trap_no(trap_no);
	}

	if (trap_no >= 0) {
		// trap���t�b�N����
		trap_vector(trap_no, (void*)trap_entry);
		sprintf(buff, " API trap number:%d  ", trap_no);
	} else {
		sprintf(buff, " API trap number:n/a  ");
	}
	Print(buff);

	sprintf(buff, "SCSI ID:%d  ", scsiid);
	Print(buff);

	sprint_eaddr(buff2, ether_addr.eaddr);
	sprintf(buff, "MAC Addr:%s\r\n", buff2);
	Print(buff);

	// �|�[�����O�J�n
	RegisterIntProcess(poll_interval);

	return 0;
}

/************************************************
 * �v���g�R�����X�g������                       *
 ************************************************/
int InitList(int n)
{
	struct prt* p;
	int i;

	p = &PRT_LIST[0];
	for (i = 0; i < NPRT; i++, p++)	{
		p->type = -1;
		p->func = 0;
		p->malloc = (malloc_func)-1;
	}
	num_of_prt = 0;

#ifdef MULTICAST
	num_of_multicast = 0;
#endif

	return 0;
}

/************************************************
 * ��M�n���h���i�v���g�R���j�ǉ�               *
 ************************************************/
int AddList(int type, int_handler handler)
{
	struct prt* p;
	int i, result;

	if (type == -1) {
		return -1;
	}

	result = -1;

	// overwrite if alreay exist
	p = &PRT_LIST[0];
	for (i = 0; i < NPRT; i++, p++) {
		if ((p->type == type && p->malloc != (malloc_func)-1)
			|| p->type == -1)
		{
			if (p->type == -1)
				num_of_prt++;

			p->type = type;
			p->func = handler;
			p->malloc = (malloc_func)-1;
			i++;
			p++;
			result = 0;
			break;
		}
	}

	// clear if exist more
	for (; i < NPRT; i++, p++) {
		if (p->type == type && p->malloc != (malloc_func)-1) {
			p->type = -1;
		}
	}

	return result;
}

/************************************************
 * ��M�n���h���i�v���g�R���j�폜               *
 ************************************************/
int DeleteList(int type)
{
	struct prt* p;
	int i, result;

	if (type == -1) {
		return -1;
	}

	result = -1;
	for (i = 0, p = &PRT_LIST[0]; i < NPRT; i++, p++) {
		if (p->type == type && p->malloc != (malloc_func)-1) {
			p->type = -1;
			result = 0;
			num_of_prt--;
		}
	}

	return result;
}

/************************************************
 * ��M�n���h���i�v���g�R���j�T�[�`             *
 ************************************************/
int_handler SearchList(int type)
{
	struct prt* p;
	int i;

	if (type == -1) {
		return 0;
	}

	for (i = 0, p = &PRT_LIST[0]; i < NPRT; i++, p++) {
		if (p->type == type) {
			return p->func;
		}
	}

	return 0;
}


/************************************************
 * ��M�n���h���i�v���g�R���j�T�[�`             *
 ************************************************/
int_handler SearchList2(int type, int n)
{
	struct prt *p;
	int i, cur;

	if (type == -1) {
		return NULL;
	}

	cur = 0;
	for (i = 0, p = &PRT_LIST[0]; i < NPRT; i++, p++) {
		if (p->type == type && cur++ == n) {
			return p->func;
		}
	}

	return NULL;
}
