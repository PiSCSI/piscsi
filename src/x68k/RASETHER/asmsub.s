*	asmsub.s

	.xdef	_DI, _EI											* ne2000.c nagai.

	.text
	.even



_DI:
	move.w	sr,sendp_sr
	ori.w	#$700,sr
	rts
_EI:
	move.w	sendp_sr,sr
	rts

sendp_sr:
	.ds.w	1


	.even
