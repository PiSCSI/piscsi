**---------------------------------------------------------------------------
**
**  SCSI Target Emulator RaSCSI (*^..^*)
**  for Raspberry Pi
**
**  Powered by XM6 TypeG Technorogy.
**  Copyright (C) 2016-2017 GIMONS
**	[ RaSCSI イーサーネット ドライバ ]
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
* Ｃ言語用 外部宣言
*
	.xref	_Initialize, _AddList, _SearchList, _DeleteList		;main.c  
	.xref	_GetMacAddr, _SetMacAddr							;scsictl.c
	.xref	_SendPacket, _SetPacketReception 					;scsictl.c
*	.xref	_AddMulticastAddr, _DelMulticastAddr				;未実装

*
* Ｃ言語用 外部変数
*
	.xref	_num_of_prt		;main.c 登録プロトコル数
	.xref	_trap_no		;使用trapナンバー
	.xref	_trans_counter	;scsictl.c 送信/受信バイト数
	.xref	_intr_type		;scsictl.c 割り込み種別
	.xref	_poll_interval	;scsictl.c ポーリング間隔


* Text Section -------------------------------- *

	.cpu	68000
	.text


*
* デバイスヘッダー
*
device_header:
	.dc.l	-1					;リンクポインター
	.dc	$8000					;device att.
	.dc.l	strategy_entry		;stategy entry
	.dc.l	interupt_entry		;interupt entry
	.dc.b	'/dev/en0'			;device name
	.dc.b	'EthD'				;for etherlib.a
	.dc.b	'RASC'				;driver name (この後にエントリーを置く)

* 'RASC' から superjsr_entry の間には何も置かないこと

*
* イーサドライバ 関数 エントリー ( for  DOS _SUPERJSR )
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
	rts				;普通のリターン


*
* イーサドライバ 関数 エントリー ( for  trap #n )
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
	rte				;割り込みリターン


*
* 各処理ふりわけ
*
do_command:
	moveq	#FUNC_MIN,d1
	cmp.l	d0,d1
	bgt	error			;d0<-2 なら未対応コマンド番号
	moveq	#FUNC_MAX,d1
	cmp.l	d1,d0
	bgt	error			;9<d0 なら未対応コマンド番号

	add	d0,d0
	move	(jumptable,pc,d0.w),d0
	jmp	(jumptable,pc,d0.w)	;引数 a0 をレジスタ渡し
**	rts
error:
	moveq	#-1,d0
	rts

*
* ジャンプテーブル
*

FUNC_MIN:	.equ	($-jumptable)/2
	.dc	get_cnt_addr-jumptable			;-2 ... 送受信カウンタのアドレス取得
	.dc	driver_entry-jumptable			;-1 ... 使用trap番号の取得
jumptable:
	.dc	get_driver_version-jumptable	;00 ... バージョンの取得
	.dc	get_mac_addr-jumptable			;01 ... 現在のMACアドレス取得
	.dc	get_prom_addr-jumptable			;02 ... PROMに書かれたMACアドレス取得
	.dc	set_mac_addr-jumptable			;03 ... MACアドレスの設定
	.dc	send_packet-jumptable			;04 ... パケット送信
	.dc	set_int_addr-jumptable			;05 ... パケット受信ハンドラ設定
	.dc	get_int_addr-jumptable			;06 ... パケット受信ハンドラのアドレス取得
	.dc	del_int_addr-jumptable			;07 ... ハンドラ削除
	.dc	set_multicast_addr-jumptable	;08 ... (マルチキャストの設定＜未対応＞)
	.dc	get_statistics-jumptable		;09 ... (統計読み出し＜未対応＞)
FUNC_MAX:	.equ	($-jumptable)/2-1


*
* コマンド -2: 送受信カウンタのアドレスを返す
*  return: address
*
get_cnt_addr:
	pea	(_trans_counter,pc)
	move.l	(sp)+,d0
	rts


*
* コマンド -1: 使用trap番号を返す
*  return: trap number to use (-1:use SUPERJSR)
*
driver_entry:
*	pea	(mesff,pc)
*	DOS	_PRINT
*	addq.l	#4,sp
*	.data
*mesff:	.dc.b	'DriverEntry',13,10,0
*	.text

	move.l	(_trap_no,pc),d0	;trap_no ... main.c 変数
	rts


*
* コマンド 00: ドライバーのバージョンを返す
*  return: version number (例... ver1.00 なら 100 を返す)
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
* コマンド 01: 現在の MAC アドレスの取得
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
	bsr	_GetMacAddr		;scsictl.c 関数
	addq.l	#4,sp
	move.l	(sp)+,d0		;引数の a0 を d0 にそのまま返す
	rts

*
* コマンド 02: EEPROM に書かれた MAC アドレスの取得
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
	bsr	_GetMacAddr		;scsictl.c 関数
	addq.l	#4,sp
	move.l	(sp)+,d0		;引数の a0 を d0 にそのまま返す
	rts

*
* コマンド 03: MACアドレスの設定
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
	bsr	_SetMacAddr		;scsictl.c 関数
	addq.l	#4,sp
	rts


*
* コマンド 04: パケット送信
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

	move.l	(a0)+,d0		;パケットサイズ
	move.l	(a0),-(sp)		;パケットアドレス
	move.l	d0,-(sp)
	bsr	_SendPacket		;scsictl.c 関数
	addq.l	#8,sp

*	move.l	d0,-(sp)
*	pea	(mes04e,pc)
*	DOS	_PRINT
*	addq.l	#4,sp
*	move.l	(sp)+,d0
*	.data
*mes04:	.dc.b	13,10,'SendPacket,13,10',0
*mes04e:.dc.b	13,10,'SendPacketおわり',13,10,0
*	.text

	rts


*
* コマンド 05: 受信割り込みハンドラ追加・設定
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

	move.l	(a0)+,d0		;プロトコル番号
	move.l	(a0),-(sp)		;ハンドラ関数のアドレス
	move.l	d0,-(sp)
	bsr	_AddList		;main.c 関数
	addq.l	#8,sp
	tst.l	d0
	bmi	set_int_addr_rts	;登録失敗

	cmpi.l	#1,(_num_of_prt)	;ハンドラ数が１なら割り込み許可へ
	bne	set_int_addr_rts

	pea	(1)			;1=<許可>
	bsr	_SetPacketReception	;割り込み許可 ... scsictl.c
	addq.l	#4,sp

*	moveq	#0,d0			;SetPacketReception() で常に 0 が返るので省略
set_int_addr_rts:
	rts


*
* コマンド 06: 割り込みハンドラのアドレス取得
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
* コマンド 07: 割り込みハンドラの削除
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
	bsr	_DeleteList		;main.c 関数
	move.l	d0,(sp)+
	bmi	del_int_addr_ret	;削除失敗

	tst.l	(_num_of_prt)		;ハンドラが一つもなくなれば割り込みを禁止する
	bne	del_int_addr_ret

	clr.l	-(sp)			;0=<禁止>
	bsr	_SetPacketReception	;割り込み禁止 ... scsictl.c
	addq.l	#4,sp

*	moveq	#0,d0			;SetPacketReception() で常に 0 が返るので省略
del_int_addr_ret:
	rts


*
* コマンド 08: マルチキャストアドレスの設定
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
* コマンド 09: 統計読み出し
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
* デバイスドライバエントリー
*
strategy_entry:
	move.l	a5,(request_buffer)
	rts


interupt_entry:
	move.l	sp,(stack_buff)		;自前のスタックエリアを使う
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
	bsr	_Initialize		;main.c 関数
				;I/Oアドレス設定
				;MACアドレス取得
				;プロトコルリスト初期化
				;SCSICTL初期化
				;割り込みハンドラ（ベクタ設定）
				;trapサービス（ベクタ設定）
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

	movea.l	(stack_buff,pc),sp	;スタックポインタを元にもどす
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
	.dc.b	'パラメータが異常です',13,10,0
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
* スタックエリア
*
	.ds.b	1024*8
def_stack:

prog_end:
	.end


* EOF --------------------------------- *
