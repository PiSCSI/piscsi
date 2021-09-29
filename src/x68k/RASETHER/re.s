**---------------------------------------------------------------------------
**
**  SCSI Target Emulator RaSCSI (*^..^*)
**  for Raspberry Pi
**
**  Powered by XM6 TypeG Technology.
**  Copyright (C) 2016-2017 GIMONS
**	[ RaSCSI Ethernet Driver ]
**
**  Based on
**    Neptune-X board driver for  Human-68k(ESP-X)   version 0.03
**    Programed 1996-7 by Shi-MAD.
**    Special thanks to  Niggle, FIRST, yamapu ...
**
**---------------------------------------------------------------------------

* Include Files ----------------------- *

	.include	doscall.mac
	.include	iocscall.mac


* Global Symbols ---------------------- *

*
* �b����p �O���錾
*
	.xref	_Initialize, _AddList, _SearchList, _DeleteList		;main.c  
	.xref	_GetMacAddr, _SetMacAddr							;scsictl.c
	.xref	_SendPacket, _SetPacketReception 					;scsictl.c
*	.xref	_AddMulticastAddr, _DelMulticastAddr				;������

*
* �b����p �O���ϐ�
*
	.xref	_num_of_prt		;main.c �o�^�v���g�R����
	.xref	_trap_no		;�g�ptrap�i���o�[
	.xref	_trans_counter	;scsictl.c ���M/��M�o�C�g��
	.xref	_intr_type		;scsictl.c ���荞�ݎ��
	.xref	_poll_interval	;scsictl.c �|�[�����O�Ԋu


* Text Section -------------------------------- *

	.cpu	68000
	.text


*
* �f�o�C�X�w�b�_�[
*
device_header:
	.dc.l	-1					;�����N�|�C���^�[
	.dc	$8000					;device att.
	.dc.l	strategy_entry		;stategy entry
	.dc.l	interupt_entry		;interupt entry
	.dc.b	'/dev/en0'			;device name
	.dc.b	'EthD'				;for etherlib.a
	.dc.b	'RASC'				;driver name (���̌�ɃG���g���[��u��)

* 'RASC' ���� superjsr_entry �̊Ԃɂ͉����u���Ȃ�����

*
* �C�[�T�h���C�o �֐� �G���g���[ ( for  DOS _SUPERJSR )
*   in: d0:	command number
*	a0:	args
*
superjsr_entry:
	movem.l	d1-d7/a1-a7,-(sp)

*	move.l	d0,-(sp)
*	pea	(mes11,pc)
*	DOS	_PRINT
*	addq.l	#4,sp
*	move.l	(sp)+,d0
*	.data
*mes11:	.dc.b	'en0:spjEntry',13,10,0
*	.text

	bsr	do_command
	movem.l	(sp)+,d1-d7/a1-a7
	rts				;���ʂ̃��^�[��


*
* �C�[�T�h���C�o �֐� �G���g���[ ( for  trap #n )
*   in: d0:	command number
*	a0:	args
*
_trap_entry::
	movem.l	d1-d7/a1-a7,-(sp)

*	move.l	d0,-(sp)
*	pea	(mes10,pc)
*	DOS	_PRINT
*	addq.l	#4,sp
*	move.l	(sp)+,d0
*	.data
*mes10:	.dc.b	'en0:trapEntry',13,10,0
*	.text

	bsr	do_command
	movem.l	(sp)+,d1-d7/a1-a7
	rte				;���荞�݃��^�[��


*
* �e�����ӂ�킯
*
do_command:
	moveq	#FUNC_MIN,d1
	cmp.l	d0,d1
	bgt	error			;d0<-2 �Ȃ疢�Ή��R�}���h�ԍ�
	moveq	#FUNC_MAX,d1
	cmp.l	d1,d0
	bgt	error			;9<d0 �Ȃ疢�Ή��R�}���h�ԍ�

	add	d0,d0
	move	(jumptable,pc,d0.w),d0
	jmp	(jumptable,pc,d0.w)	;���� a0 �����W�X�^�n��
**	rts
error:
	moveq	#-1,d0
	rts

*
* �W�����v�e�[�u��
*

FUNC_MIN:	.equ	($-jumptable)/2
	.dc	get_cnt_addr-jumptable			;-2 ... ����M�J�E���^�̃A�h���X�擾
	.dc	driver_entry-jumptable			;-1 ... �g�ptrap�ԍ��̎擾
jumptable:
	.dc	get_driver_version-jumptable	;00 ... �o�[�W�����̎擾
	.dc	get_mac_addr-jumptable			;01 ... ���݂�MAC�A�h���X�擾
	.dc	get_prom_addr-jumptable			;02 ... PROM�ɏ����ꂽMAC�A�h���X�擾
	.dc	set_mac_addr-jumptable			;03 ... MAC�A�h���X�̐ݒ�
	.dc	send_packet-jumptable			;04 ... �p�P�b�g���M
	.dc	set_int_addr-jumptable			;05 ... �p�P�b�g��M�n���h���ݒ�
	.dc	get_int_addr-jumptable			;06 ... �p�P�b�g��M�n���h���̃A�h���X�擾
	.dc	del_int_addr-jumptable			;07 ... �n���h���폜
	.dc	set_multicast_addr-jumptable	;08 ... (�}���`�L���X�g�̐ݒ聃���Ή���)
	.dc	get_statistics-jumptable		;09 ... (���v�ǂݏo�������Ή���)
FUNC_MAX:	.equ	($-jumptable)/2-1


*
* �R�}���h -2: ����M�J�E���^�̃A�h���X��Ԃ�
*  return: address
*
get_cnt_addr:
	pea	(_trans_counter,pc)
	move.l	(sp)+,d0
	rts


*
* �R�}���h -1: �g�ptrap�ԍ���Ԃ�
*  return: trap number to use (-1:use SUPERJSR)
*
driver_entry:
*	pea	(mesff,pc)
*	DOS	_PRINT
*	addq.l	#4,sp
*	.data
*mesff:	.dc.b	'DriverEntry',13,10,0
*	.text

	move.l	(_trap_no,pc),d0	;trap_no ... main.c �ϐ�
	rts


*
* �R�}���h 00: �h���C�o�[�̃o�[�W������Ԃ�
*  return: version number (��... ver1.00 �Ȃ� 100 ��Ԃ�)
*
get_driver_version:
*	pea	(mes00,pc)
*	DOS	_PRINT
*	addq.l	#4,sp
*	.data
*mes00:	.dc.b	'GetDriverVersion',13,10,0
*	.text

	moveq	#3,d0			;ver 0.03
	rts


*
* �R�}���h 01: ���݂� MAC �A�h���X�̎擾
*  return: same as *dst
*
get_mac_addr:
*	pea	(mes01,pc)
*	DOS	_PRINT
*	addq.l	#4,sp
*	.data
*mes01:	.dc.b	'GetMacAddr',13,10,0
*	.text

	pea	(a0)
	pea	(a0)
	bsr	_GetMacAddr		;scsictl.c �֐�
	addq.l	#4,sp
	move.l	(sp)+,d0		;������ a0 �� d0 �ɂ��̂܂ܕԂ�
	rts

*
* �R�}���h 02: EEPROM �ɏ����ꂽ MAC �A�h���X�̎擾
*  return: same as *dst
*
get_prom_addr:
*	pea	(mes02,pc)
*	DOS	_PRINT
*	addq.l	#4,sp
*	.data
*mes02:	.dc.b	'GePromAddr',13,10,0
*	.text

	pea	(a0)
	pea	(a0)
	bsr	_GetMacAddr		;scsictl.c �֐�
	addq.l	#4,sp
	move.l	(sp)+,d0		;������ a0 �� d0 �ɂ��̂܂ܕԂ�
	rts

*
* �R�}���h 03: MAC�A�h���X�̐ݒ�
*  return: 0 (if no errors)
*
set_mac_addr:
*	pea	(mes03,pc)
*	DOS	_PRINT
*	addq.l	#4,sp
*	.data
*mes03:	.dc.b	'SetMacAddr',13,10,0
*	.text

	pea	(a0)
	bsr	_SetMacAddr		;scsictl.c �֐�
	addq.l	#4,sp
	rts


*
* �R�}���h 04: �p�P�b�g���M
*   packet contents:
*	Distination MAC: 6 bytes
*	Source(own) MAC: 6 bytes
*	Protcol type:    2 bytes
*	Data:            ? bytes
*  return: 0 (if no errors)
*
send_packet:
*	pea	(mes04,pc)
*	DOS	_PRINT
*	addq.l	#4,sp

	move.l	(a0)+,d0		;�p�P�b�g�T�C�Y
	move.l	(a0),-(sp)		;�p�P�b�g�A�h���X
	move.l	d0,-(sp)
	bsr	_SendPacket		;scsictl.c �֐�
	addq.l	#8,sp

*	move.l	d0,-(sp)
*	pea	(mes04e,pc)
*	DOS	_PRINT
*	addq.l	#4,sp
*	move.l	(sp)+,d0
*	.data
*mes04:	.dc.b	13,10,'SendPacket,13,10',0
*mes04e:.dc.b	13,10,'SendPacket�����',13,10,0
*	.text

	rts


*
* �R�}���h 05: ��M���荞�݃n���h���ǉ��E�ݒ�
*   type: 0x00000800 IP packet
*         0x00000806 ARP packet
*  return: 0 (if no errors)
*
set_int_addr:
*	pea	(mes05,pc)
*	DOS	_PRINT
*	addq.l	#4,sp
*	.data
*mes05:	.dc.b	'SetIntAddr',13,10,0
*	.text

	move.l	(a0)+,d0		;�v���g�R���ԍ�
	move.l	(a0),-(sp)		;�n���h���֐��̃A�h���X
	move.l	d0,-(sp)
	bsr	_AddList		;main.c �֐�
	addq.l	#8,sp
	tst.l	d0
	bmi	set_int_addr_rts	;�o�^���s

	cmpi.l	#1,(_num_of_prt)	;�n���h�������P�Ȃ犄�荞�݋���
	bne	set_int_addr_rts

	pea	(1)			;1=<����>
	bsr	_SetPacketReception	;���荞�݋��� ... scsictl.c
	addq.l	#4,sp

*	moveq	#0,d0			;SetPacketReception() �ŏ�� 0 ���Ԃ�̂ŏȗ�
set_int_addr_rts:
	rts


*
* �R�}���h 06: ���荞�݃n���h���̃A�h���X�擾
*  return: interupt address
*
get_int_addr:
*	pea	(mes07,pc)
*	DOS	_PRINT
*	addq.l	#4,sp
*	.data
*mes07:	.dc.b	'GetIntAddr',13,10,0
*	.text

	pea	(a0)
	bsr	_SearchList
	addq.l	#4,sp
	rts


*
* �R�}���h 07: ���荞�݃n���h���̍폜
*  return: 0 (if no errors)
*
del_int_addr:
*	pea	(mes06,pc)
*	DOS	_PRINT
*	addq.l	#4,sp
*	.data
*mes06:	.dc.b	'DelIntAddr',13,10,0
*	.text

	pea	(a0)
	bsr	_DeleteList		;main.c �֐�
	move.l	d0,(sp)+
	bmi	del_int_addr_ret	;�폜���s

	tst.l	(_num_of_prt)		;�n���h��������Ȃ��Ȃ�Ί��荞�݂��֎~����
	bne	del_int_addr_ret

	clr.l	-(sp)			;0=<�֎~>
	bsr	_SetPacketReception	;���荞�݋֎~ ... scsictl.c
	addq.l	#4,sp

*	moveq	#0,d0			;SetPacketReception() �ŏ�� 0 ���Ԃ�̂ŏȗ�
del_int_addr_ret:
	rts


*
* �R�}���h 08: �}���`�L���X�g�A�h���X�̐ݒ�
*
set_multicast_addr:
*	pea	(mes08,pc)
*	.data
*	DOS	_PRINT
*	addq.l	#4,sp
*mes08:	.dc.b	'SetMulticastAddr',13,10,0
*	.text

	moveq	#0,d0
	rts


*
* �R�}���h 09: ���v�ǂݏo��
*
get_statistics:
*	pea	(mes09,pc)
*	DOS	_PRINT
*	addq.l	#4,sp
*	.data
*mes09:	.dc.b	'GetStatistics',13,10,0
*	.text

	moveq	#0,d0
	rts

*
* �f�o�C�X�h���C�o�G���g���[
*
strategy_entry:
	move.l	a5,(request_buffer)
	rts


interupt_entry:
	move.l	sp,(stack_buff)		;���O�̃X�^�b�N�G���A���g��
	lea	(def_stack),sp		;

	movem.l	d1-d7/a0-a5,-(sp)
	movea.l	(request_buffer,pc),a5
	tst.b	(2,a5)
	bne	errorret

	pea	(mestitle,pc)
	DOS	_PRINT
	addq.l	#4,sp
	movea.l	(18,a5),a4
@@:
	tst.b	(a4)+
	bne	@b

	moveq	#0,d0
	move.l	d0,(_trap_no)
	move.l	d0,(_intr_type)
	moveq	#1,d0
	move.l	d0,(_poll_interval)

arg_loop:
	move.b	(a4)+,d0
	beq	arg_end
	cmpi.b	#'-',d0
	beq	@f
	cmpi.b	#'/',d0
	bne	param_errret
@@:
	move.b	(a4)+,d0
	beq	param_errret
opt_loop:
	andi.b	#$df,d0
	cmpi.b	#'I',d0
	beq	opt_i
	cmpi.b	#'P',d0
	beq	opt_p
	cmpi.b	#'T',d0
	beq	opt_t
	cmpi.b	#'N',d0
	beq	opt_n
	bra	param_errret

opt_n:
	moveq	#-1,d0
	move.l	d0,(_trap_no)
	move.b	(a4)+,d0
	beq	arg_loop
	bra	opt_loop

opt_t:
	bsr	get_num
	tst.b	d0
	bne	param_errret
	cmpi	#6,d2
	bgt	param_errret
	move.l	d2,(_trap_no)
	move.b	(a4)+,d0
	beq	arg_loop
	bra	opt_loop

opt_p:
	bsr	get_num
	tst.b	d0
	bne	param_errret
	cmpi	#1,d2
	blt	param_errret
	cmpi	#8,d2
	bgt	param_errret
	move.l	d2,(_poll_interval)
	move.b	(a4)+,d0
	beq	arg_loop
	bra	opt_loop

opt_i:
	bsr	get_num
	tst.b	d0
	bne	param_errret
	cmpi	#1,d2
	bgt	param_errret
	move.l	d2,(_intr_type)
	move.b	(a4)+,d0
	beq	arg_loop
	bra	opt_loop

arg_end:
	bsr	_Initialize		;main.c �֐�
				;I/O�A�h���X�ݒ�
				;MAC�A�h���X�擾
				;�v���g�R�����X�g������
				;SCSICTL������
				;���荞�݃n���h���i�x�N�^�ݒ�j
				;trap�T�[�r�X�i�x�N�^�ݒ�j
	tst.l	d0
	bne	errorret

*	moveq	#0,d0
	move.l	#prog_end,(14,a5)
	bra	intret


param_errret:
	pea	(mesparam_err,pc)
	DOS	_PRINT
	addq.l	#4,sp
errorret:
	move	#$5003,d0
intret:
	move.b	d0,(4,a5)
	lsr	#8,d0
	move.b	d0,(3,a5)
	movem.l	(sp)+,d1-d7/a0-a5

	movea.l	(stack_buff,pc),sp	;�X�^�b�N�|�C���^�����ɂ��ǂ�
	rts

get_num:
	moveq	#1,d0
	moveq	#0,d1
	moveq	#0,d2
@@:
	move.b	(a4),d1
	subi.b	#'0',d1
	bcs	@f
	cmpi.b	#9,d1
	bgt	@f
	move.b	#0,d0
	andi	#$f,d1
	mulu	#10,d2
	add	d1,d2
	addq.l	#1,a4
	bra	@b
@@:
	rts


* Data Section ------------------------ *

	.data
mestitle:
	.dc.b	13,10
	.dc.b	'RaSCSI Ethernet Driver version 1.20 / Based on ether_ne.sys+M01L12',13,10
	.dc.b	0
mesparam_err:
	.dc.b	'�p�����[�^���ُ�ł�',13,10,0
	.even


* BSS --------------------------------- *

	.bss
	.quad

stack_buff:
	.ds.l	1
request_buffer:
	.ds.l	1
stack_buff_i:	
	.ds.l	1


* Stack Section ----------------------- *

	.stack
	.quad

*
* �X�^�b�N�G���A
*
	.ds.b	1024*8
def_stack:

prog_end:
	.end


* EOF --------------------------------- *
