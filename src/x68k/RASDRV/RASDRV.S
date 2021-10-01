*---------------------------------------------------------------------------
*
*  SCSI Target Emulator RaSCSI (*^..^*)
*  for Raspberry Pi
*
*  Powered by XM6 TypeG Technology.
*  Copyright (C) 2016-2019 GIMONS
*	[ Host Filesystem Bridge Driver ]
*
* Based on
*	X68k Emulator Host Filesystem Driver version 0.27
*	Programmed By co
*
*---------------------------------------------------------------------------

	CPU	68010

	INCLUDE	DOSCALL.MAC
	INCLUDE IOCSCALL.MAC

.XREF	_Init, _Process		;bridge.c  

COMMAND	EQU	2	Command number
ERRLOW	EQU	3	Error code lower 8 bits
ERRHIGH	EQU	4	Error code upper 8 bits
MXUNIT	EQU	13	Number of units
DEVEND	EQU	14	Driver exit address
BDEVNO	EQU	22	Base drive number

DDHEADER:
	DC.L	-1		+$00	Link pointer
	DC.W	$2040		+$04	Device attribute
	DC.L	DDSTRATEGY	+$06
	DC.L	DDENTRY_FIRST	+$0A
	DC.B	'*EMUHOST'	+$0E	Device name

DDREQUEST:
	DC.L	0

DDSTRATEGY:
	MOVE.L	A5,DDREQUEST
	RTS

*Device driver entry
DDENTRY:
	MOVE.L	SP,(STACK_BUFF)
	LEA	(DEF_STACK),SP
	MOVEM.L	D1-D3/A0-A2,-(SP)
	MOVEM.L	DDREQUEST(PC),D0	D0.L: Request header address
	MOVE.L	D0,-(SP)
	BSR		_Process
	ADDQ.L	#4,SP
	MOVEM.L	(SP)+,D1-D3/A0-A2
	MOVEA.L	(STACK_BUFF),SP
	RTS

KEEP_HERE:

*First time only device driver entry
DDENTRY_FIRST:
	MOVEM.L	D1-D3/A0-A2/A5,-(SP)
	MOVEA.L	DDREQUEST(PC),A5	A5.L: Request header address
*	CMPI.B	#$40,COMMAND(A5)	No need to check when Human68k is running
*	BNE	UNSUPPORTED		Error:	Not supported

*Show title
	PEA	MESSAGE_TITLE(PC)
	DOS	_PRINT
	ADDQ.L	#4,SP

*Device check
DEVICE_CHECK:
	BSR _Init

	TST.L	D0
	BEQ	NOTFOUND		Error: Device not found

*Resident program start
	LEA	DDENTRY(PC),A1

	LEA	DDHEADER+$0A(PC),A0
	MOVE.L	A1,(A0)			Configure driver entry
	MOVE.L	#PROG_END,DEVEND(A5)		Configure resident exit address

	ST.B	MXUNIT(A5)		Set negative number since port is considered unimplemented
	JSR	(A1)			Execute driver entry

	TST.B	MXUNIT(A5)		Verify number of units
	BLE	NODRIVE			Error: No drive

	MOVE.B	BDEVNO(A5),D0
	MOVE.B	MXUNIT(A5),D1
	LEA	FIRSTDRIVE(PC),A0
	ADD.B	D0,(A0)			Overwrite start drive name
	ADD.B	D1,D0			Overwrite stop drive name
	ADD.B	D0,LASTDRIVE-FIRSTDRIVE(A0)

	PEA	MESSAGE_DRIVENAME(PC)	Normal:  Drive A:
	DOS	_PRINT

	PEA	MESSAGE_DRIVENAME2(PC)	Normal:  to Z:
	SUBQ.B	#2,D1
	BCS	@F
	DOS	_PRINT
@@	ADDQ.L	#8,SP

	PEA	MESSAGE_DRIVENAME3(PC)	Normal:  was registered
	BRA	QUIT

NOTFOUND:
	PEA	MESSAGE_NOTFOUND(PC)	Error:  Device not found
	BRA	ABORT

UNSUPPORTED:
	PEA	MESSAGE_UNSUPPORTED(PC)	Error:  Not supported
	BRA	ABORT

NODRIVE:
	PEA	MESSAGE_NODRIVE(PC)	Error:  No drive

ABORT:
	MOVE.B	#$0D,ERRLOW(A5)
	MOVE.B	#$70,ERRHIGH(A5)

QUIT:
	DOS	_PRINT
	ADDQ.L	#4,SP

	MOVEM.L	(SP)+,D1-D3/A0-A2/A5
	RTS

SECRET:
	PEA	MESSAGE_TITLE2(PC)
	DOS	_PRINT
	PEA	CREDIT(PC)
	DOS	_PRINT
	ADDQ.L	#8,SP
	DOS	_EXIT

	DATA

*Error messages
MESSAGE_NOTFOUND:
	DC.B	'Device not found',$0D,$0A,0

MESSAGE_UNSUPPORTED:
	DC.B	'Not supported',$0D,$0A,0

MESSAGE_NODRIVE:
	DC.B	'No drive',$0D,$0A,0

*Registration messages
MESSAGE_DRIVENAME:
	DC.B	'Drive'
FIRSTDRIVE:
	DC.B	'A:',0
MESSAGE_DRIVENAME2:
	DC.B	' to '
LASTDRIVE:
	DC.B	'@:',0
MESSAGE_DRIVENAME3:
	DC.B	' was registered',$0D,$0A,0

*Title
MESSAGE_TITLE:
	DC.B	$0D,$0A
MESSAGE_TITLE2:
	DC.B	'RaSCSI FileSystem Driver version 1.42',$0D,$0A,0

*Thanks!
CREDIT:
	DC.B	'Coded by GIMONS',$0D,$0A
	DC.B	'Special thanks to',$0D,$0A
	DC.B	9,'co',$0D,$0A
	DC.B	9,'PI.',$0D,$0A
	DC.B	9,'Tachibana@Kuwashima Giken',$0D,$0A
	DC.B	0

	BSS
	QUAD

STACK_BUFF:
	.DS.L	1

	STACK
	QUAD

	.DS.B	1024*2
DEF_STACK:


PROG_END:
	END	SECRET
