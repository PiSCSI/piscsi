//---------------------------------------------------------------------------
//
//  SCSI Target Emulator RaSCSI (*^..^*)
//  for Raspberry Pi
//
//  Powered by XM6 TypeG Technology.
//  Copyright (C) 2016-2017 GIMONS
//	[ RaSCSI Ethernet SCSI Control Department ]
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
#include <sys/scsi.h>
#include <iocslib.h>
#include "main.h"
#include "scsictl.h"

typedef struct
{
	char DeviceType;
	char RMB;
	char ANSI_Ver;
	char RDF;
	char AddLen;
	char RESV0;
	char RESV1;
	char OptFunc;
	char VendorID[8];
	char ProductID[16];
	char FirmRev[4];
} INQUIRY_T;

typedef struct
{
	INQUIRY_T info;
	char options[8];
} INQUIRYOPT_T;

#define MFP_AEB		0xe88003
#define MFP_IERB	0xe88009
#define MFP_IMRB	0xe88015

// asmsub.s ���̃T�u���[�`��
extern void DI();
extern void EI();

volatile short* iocsexec = (short*)0xa0e;	// IOCS���s�����[�N
struct trans_counter trans_counter;			// ����M�J�E���^
unsigned char rx_buff[2048];				// ��M�o�b�t�@
int imr;									// ���荞�݋��t���O
int scsistop;								// SCSI��~���t���O
int intr_type;								// ���荞�ݎ��(0:V-DISP 1:TimerA)
int poll_interval;							// �|�[�����O�Ԋu(�ݒ�)
int poll_current;							// �|�[�����O�Ԋu(����)
int idle;									// �A�C�h���J�E���^

#define POLLING_SLEEP	255	// 4-5s

/************************************************
 *   MAC�A�h���X�擾���ߔ��s                    *
 ************************************************/
int SCSI_GETMACADDR(unsigned char *mac)
{
	unsigned char cmd[10];
	unsigned char sts;
	unsigned char msg;
	
	if (S_SELECT(scsiid) != 0) {
		return 0;
	}

	cmd[0] = 0x28;
	cmd[1] = 0;
	cmd[2] = 1;
	cmd[3] = 0;
	cmd[4] = 0;
	cmd[5] = 0;
	cmd[6] = 0;
	cmd[7] = 0;
	cmd[8] = 6;
	cmd[9] = 0;
	if (S_CMDOUT(10, cmd) != 0) {
		return 0;
	}

#ifdef USE_DMA
	if (S_DATAIN(6, mac) != 0) {
#else
	if (S_DATAIN_P(6, mac) != 0) {
#endif
		return 0;
	}

	S_STSIN(&sts);
	S_MSGIN(&msg);

	return 1;
}

/************************************************
 *   MAC�A�h���X�ݒ薽�ߔ��s                    *
 ************************************************/
int SCSI_SETMACADDR(const unsigned char *mac)
{
	unsigned char cmd[10];
	unsigned char sts;
	unsigned char msg;
	
	if (S_SELECT(scsiid) != 0) {
		return 0;
	}

	cmd[0] = 0x2a;
	cmd[1] = 0;
	cmd[2] = 1;
	cmd[3] = 0;
	cmd[4] = 0;
	cmd[5] = 0;
	cmd[6] = 0;
	cmd[7] = 0;
	cmd[8] = 6;
	cmd[9] = 0;
	if (S_CMDOUT(10, cmd) != 0) {
		return 0;
	}

#ifdef USE_DMA
	S_DATAOUT(6, mac);
#else
	S_DATAOUT_P(6, mac);
#endif

	S_STSIN(&sts);
	S_MSGIN(&msg);
	
	return 1;
}

/************************************************
 *   �p�P�b�g��M�T�C�Y�擾���ߔ��s             *
 ************************************************/
int SCSI_GETPACKETLEN(int *len)
{
	unsigned char cmd[10];
	unsigned char buf[2];
	unsigned char sts;
	unsigned char msg;
	
	if (S_SELECT(scsiid) != 0) {
		return 0;
	}

	cmd[0] = 0x28;
	cmd[1] = 0;
	cmd[2] = 1;
	cmd[3] = 1;
	cmd[4] = 0;
	cmd[5] = 0;
	cmd[6] = 0;
	cmd[7] = 0;
	cmd[8] = 2;
	cmd[9] = 0;
	if (S_CMDOUT(10, cmd) != 0) {
		return 0;
	}

#ifdef USE_DMA
	if (S_DATAIN(2, buf) != 0) {
#else
	if (S_DATAIN_P(2, buf) != 0) {
#endif
		return 0;
	}

	S_STSIN(&sts);
	S_MSGIN(&msg);

	*len = (int)(buf[0] << 8) + (int)(buf[1]);

	return 1;
}

/************************************************
 *   �p�P�b�g��M���ߔ��s                       *
 ************************************************/
int SCSI_GETPACKETBUF(unsigned char *buf, int len)
{
	unsigned char cmd[10];
	unsigned char sts;
	unsigned char msg;
	
	if (S_SELECT(scsiid) != 0) {
		return 0;
	}

	cmd[0] = 0x28;
	cmd[1] = 0;
	cmd[2] = 1;
	cmd[3] = 1;
	cmd[4] = 0;
	cmd[5] = 0;
	cmd[6] = (unsigned char)(len >> 16);
	cmd[7] = (unsigned char)(len >> 8);
	cmd[8] = (unsigned char)len;
	cmd[9] = 1;
	if (S_CMDOUT(10, cmd) != 0) {
		return 0;
	}

#ifdef USE_DMA
	if (S_DATAIN(len, buf) != 0) {
#else
	if (S_DATAIN_P(len, buf) != 0) {
#endif
		return 0;
	}

	S_STSIN(&sts);
	S_MSGIN(&msg);

	return 1;
}

/************************************************
 *   �p�P�b�g���M���ߔ��s                       *
 ************************************************/
int SCSI_SENDPACKET(const unsigned char *buf, int len)
{
	unsigned char cmd[10];
	unsigned char sts;
	unsigned char msg;
	
	if (S_SELECT(scsiid) != 0) {
		return 0;
	}

	cmd[0] = 0x2a;
	cmd[1] = 0;
	cmd[2] = 1;
	cmd[3] = 1;
	cmd[4] = 0;
	cmd[5] = 0;
	cmd[6] = (unsigned char)(len >> 16);
	cmd[7] = (unsigned char)(len >> 8);
	cmd[8] = (unsigned char)len;
	cmd[9] = 0;
	if (S_CMDOUT(10, cmd) != 0) {
		return 0;
	}

#ifdef USE_DMA
	S_DATAOUT(len, buf);
#else
	S_DATAOUT_P(len, buf);
#endif
	S_STSIN(&sts);
	S_MSGIN(&msg);
	
	return 1;
}

/************************************************
 *   MAC�A�h���X�擾                            *
 ************************************************/
int GetMacAddr(struct eaddr* buf)
{
	if (SCSI_GETMACADDR(buf->eaddr) != 0) {
		return 0;
	} else {
		return -1;
	}
}

/************************************************
 *   MAC�A�h���X�ݒ�                            *
 ************************************************/
int SetMacAddr(const struct eaddr* data)
{
	if (SCSI_SETMACADDR(data->eaddr) != 0) {
		return 0;
	} else {
		return -1;
	}
}

/************************************************
 *  RaSCSI����                                  *
 ************************************************/
int SearchRaSCSI()
{
	int i;
	INQUIRYOPT_T inq;

	for (i = 0; i <= 7; i++) {
		// BRIDGE�f�o�C�X����
		if (S_INQUIRY(sizeof(INQUIRY_T) , i, (struct INQUIRY*)&inq) < 0) {
			continue;
		}

		if (memcmp(&(inq.info.ProductID), "RASCSI BRIDGE", 13) != 0) {
			continue;
		}

		// TAP��������Ԃ��擾
		if (S_INQUIRY(sizeof(INQUIRYOPT_T) , i, (struct INQUIRY*)&inq) < 0) {
			continue;
		}
		
		if (inq.options[1] != '1') {
			continue;
		}

		// SCSI ID�m��
		scsiid = i;
		return 0;
	}

	return -1;
}

/************************************************
 *  RaSCSI������ �֐�                           *
 ************************************************/
int InitRaSCSI(void)
{
#ifdef MULTICAST
	unsigned char multicast_table[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
#endif

	imr = 0;
	scsistop = 0;
	poll_current = -1;
	idle = 0;
	
	return 0;
}

/************************************************
 *  RaSCSI ���荞�ݏ��� �֐�(�|�[�����O)        *
 ************************************************/
void interrupt IntProcess(void)
{
	unsigned char phase;
	unsigned int len;
	int type;
	int_handler func;
	int i;

	// V-DISP GPIP���荞�݂̓A�C�h���J�E���^�Ő���
	if (intr_type == 0) {
		// �A�C�h�����Z
		idle++;
		
		// ����̏����\��ɓ��B���Ă��Ȃ��Ȃ�X�L�b�v
		if (idle < poll_current) {
			return;
		}

		// �A�C�h���J�E���^���N���A
		idle = 0;
	}
	
	// ���荞�݊J�n

	// ���荞�݋��̎�����
	if (imr == 0) {
		return;
	}

	// IOCS���s���Ȃ�΃X�L�b�v
	if (*iocsexec != -1) {
		return;
	}

	// ��M�������͊��荞�݋֎~
	DI ();

	// �o�X�t���[�̎�����
	phase = S_PHASE();
	if (phase != 0) {
		// �I��
		goto ei_exit;
	}	

	// ��M����
	if (SCSI_GETPACKETLEN(&len) == 0) {
		// RaSCSI��~��
		scsistop = 1;

		// �|�[�����O�Ԋu�̍Đݒ�(�Q��)
		UpdateIntProcess(POLLING_SLEEP);

		// �I��
		goto ei_exit;
	}
	
	// RaSCSI�͓��쒆
	if (scsistop) {
		scsistop = 0;

		// �|�[�����O�Ԋu�̍Đݒ�(�}��)
		UpdateIntProcess(poll_interval);
	}
	
	// �p�P�b�g�͓������ĂȂ�����
	if (len == 0) {
		// �I��
		goto ei_exit;
	}

	// ��M�o�b�t�@�������փp�P�b�g�]��
	if (SCSI_GETPACKETBUF(rx_buff, len) == 0) {
		// ���s
		goto ei_exit;
	}

	// ���荞�݋���
	EI ();
	
	// �p�P�b�g�^�C�v�Ńf�[�^����
	type = rx_buff[12] * 256 + rx_buff[13];
	i = 0;
	while ((func = SearchList2(type, i))) {
		i++;
		func(len, (void*)&rx_buff, ID_EN0);
	}
	trans_counter.recv_byte += len;
	return;

ei_exit:
	// ���荞�݋���
	EI ();
}

/************************************************
 *  RaSCSI �p�P�b�g���M �֐� (ne.s����)         *
 ************************************************/
int SendPacket(int len, const unsigned char* data)
{
	if (len < 1) {
		return 0;
	}
	
	if (len > 1514)	{		// 6 + 6 + 2 + 1500
		return -1;			// �G���[
	}
	
	// RaSCSI��~���̂悤�Ȃ�G���[
	if (scsistop) {
		return -1;
	}

	// ���M�������͊��荞�݋֎~
	DI ();

	// ���M�����Ƒ��M�t���O�A�b�v
	if (SCSI_SENDPACKET(data, len) == 0) {
		// ���荞�݋���
		EI ();
		return -1;
	}

	// ���荞�݋���
	EI ();

	// ���M�˗��ς�
	trans_counter.send_byte += len;
	
	return 0;
}

/************************************************
 *  RaSCSI ���荞�݋��A�s���ݒ� �֐�        *
 ************************************************/
int SetPacketReception(int i)
{
	imr = i;
	return 0;
}

/************************************************
 * RaSCSI ���荞�ݏ����o�^                      *
 ************************************************/
void RegisterIntProcess(int n)
{
	volatile unsigned char *p;

	// �|�[�����O�Ԋu(����)�̍X�V�ƃA�C�h���J�E���^�N���A
	poll_current = n;
	idle = 0;
	
	if (intr_type == 0) {
		// V-DISP GPIP���荞�݃x�N�^������������
		// ���荞�݂�L���ɂ���
		B_INTVCS(0x46, (int)IntProcess);
		p = (unsigned char *)MFP_AEB;
		*p = *p | 0x10;
		p = (unsigned char *)MFP_IERB;
		*p = *p | 0x40;
		p = (unsigned char *)MFP_IMRB;
		*p = *p | 0x40;
	} else if (intr_type == 1) {
		// TimerA�̓J�E���g���[�h��ݒ�
		VDISPST(NULL, 0, 0);
		VDISPST(IntProcess, 0, poll_current);
	}
}

/************************************************
 * RaSCSI ���荞�ݏ����ύX                      *
 ************************************************/
void UpdateIntProcess(int n)
{
	// �|�[�����O�Ԋu(����)�Ɠ����Ȃ�X�V���Ȃ�
	if (n == poll_current) {
		return;
	}
	
	// �|�[�����O�Ԋu(����)�̍X�V�ƃA�C�h���J�E���^�N���A
	poll_current = n;
	idle = 0;
	
	if (intr_type == 1) {
		// TimerA�͍ēo�^�K�v
		VDISPST(NULL, 0, 0);
		VDISPST(IntProcess, 0, poll_current);
	}
}

// EOF
