*---------------------------------------------------------------------------
*
*  SCSI Target Emulator RaSCSI (*^..^*)
*  for Raspberry Pi
*
*  Powered by XM6 TypeG Technorogy.
*  Copyright (C) 2016-2019 GIMONS
*	[ ホストファイルシステム ブリッジドライバ ]
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

COMMAND	EQU	2	コマンド番号
ERRLOW	EQU	3	エラーコード下位8ビット
ERRHIGH	EQU	4	エラーコード上位8ビット
MXUNIT	EQU	13	ユニット数
DEVEND	EQU	14	ドライバ終了アドレス
BDEVNO	EQU	22	ベースドライブ番号

DDHEADER:
	DC.L	-1		+$00	リンクポインタ
	DC.W	$2040		+$04	デバイス属性
	DC.L	DDSTRATEGY	+$06
	DC.L	DDENTRY_FIRST	+$0A
	DC.B	'*EMUHOST'	+$0E	デバイス名

DDREQUEST:
	DC.L	0

DDSTRATEGY:
	MOVE.L	A5,DDREQUEST
	RTS

*デバイスドライバエントリ
DDENTRY:
	MOVE.L	SP,(STACK_BUFF)
	LEA	(DEF_STACK),SP
	MOVEM.L	D1-D3/A0-A2,-(SP)
	MOVEM.L	DDREQUEST(PC),D0	D0.L: リクエストヘッダアドレス
	MOVE.L	D0,-(SP)
	BSR		_Process
	ADDQ.L	#4,SP
	MOVEM.L	(SP)+,D1-D3/A0-A2
	MOVEA.L	(STACK_BUFF),SP
	RTS

KEEP_HERE:

*初回のみのデバイスドライバエントリ
DDENTRY_FIRST:
	MOVEM.L	D1-D3/A0-A2/A5,-(SP)
	MOVEA.L	DDREQUEST(PC),A5	A5.L: リクエストヘッダアドレス
*	CMPI.B	#$40,COMMAND(A5)	現行のHuman68kではチェック不要
*	BNE	UNSUPPORTED		エラー:	未対応

*タイトル表示
	PEA	MESSAGE_TITLE(PC)
	DOS	_PRINT
	ADDQ.L	#4,SP

*デバイスチェック
DEVICE_CHECK:
	BSR _Init

	TST.L	D0
	BEQ	NOTFOUND		エラー: デバイスが存在しません

*常駐開始
	LEA	DDENTRY(PC),A1

	LEA	DDHEADER+$0A(PC),A0
	MOVE.L	A1,(A0)			ドライバエントリを設定
	MOVE.L	#PROG_END,DEVEND(A5)		常駐終了アドレスを設定

	ST.B	MXUNIT(A5)		ポート未実装の判定のため負の値を設定しておく
	JSR	(A1)			ドライバエントリを実行

	TST.B	MXUNIT(A5)		ユニット数確認
	BLE	NODRIVE			エラー: ドライブがありません

	MOVE.B	BDEVNO(A5),D0
	MOVE.B	MXUNIT(A5),D1
	LEA	FIRSTDRIVE(PC),A0
	ADD.B	D0,(A0)			開始ドライブ名書き換え
	ADD.B	D1,D0			終了ドライブ名書き換え
	ADD.B	D0,LASTDRIVE-FIRSTDRIVE(A0)

	PEA	MESSAGE_DRIVENAME(PC)	正常: ドライブA:
	DOS	_PRINT

	PEA	MESSAGE_DRIVENAME2(PC)	正常: からZ:
	SUBQ.B	#2,D1
	BCS	@F
	DOS	_PRINT
@@	ADDQ.L	#8,SP

	PEA	MESSAGE_DRIVENAME3(PC)	正常: を登録しました
	BRA	QUIT

NOTFOUND:
	PEA	MESSAGE_NOTFOUND(PC)	エラー: デバイスが存在しません
	BRA	ABORT

UNSUPPORTED:
	PEA	MESSAGE_UNSUPPORTED(PC)	エラー: 未対応
	BRA	ABORT

NODRIVE:
	PEA	MESSAGE_NODRIVE(PC)	エラー: ドライブがありません

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

*エラーメッセージ
MESSAGE_NOTFOUND:
	DC.B	'デバイスが存在しません',$0D,$0A,0

MESSAGE_UNSUPPORTED:
	DC.B	'未対応',$0D,$0A,0

MESSAGE_NODRIVE:
	DC.B	'ドライブがありません',$0D,$0A,0

*登録メッセージ
MESSAGE_DRIVENAME:
	DC.B	'ドライブ'
FIRSTDRIVE:
	DC.B	'A:',0
MESSAGE_DRIVENAME2:
	DC.B	'から'
LASTDRIVE:
	DC.B	'@:',0
MESSAGE_DRIVENAME3:
	DC.B	'を登録しました',$0D,$0A,0

*タイトル
MESSAGE_TITLE:
	DC.B	$0D,$0A
MESSAGE_TITLE2:
	DC.B	'RaSCSI FileSystem Driver version 1.42',$0D,$0A,0

*ありがとう！
CREDIT:
	DC.B	'Coded by GIMONS',$0D,$0A
	DC.B	'Special thanks to',$0D,$0A
	DC.B	9,'co',$0D,$0A
	DC.B	9,'ＰＩ．',$0D,$0A
	DC.B	9,'立花＠桑島技研',$0D,$0A
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
