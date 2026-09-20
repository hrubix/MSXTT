; MSXTT3 ROM entry: keep the application stack away from HIMEM.
; Pico+ reserves 30 bytes below HIMEM on its first indexed UNAPI query.

	.module rom_entry

	.globl	_TT_RomMain

ROM_STACK_TOP	.equ	#0xF200

	.area	_CODE

_main::
	di
	ld	sp,#ROM_STACK_TOP
	ei
	call	_TT_RomMain
	rst	0
