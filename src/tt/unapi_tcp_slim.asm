; MSXTT slim UNAPI TCP client (subset of MSXgl unapi_tcp.asm)
; Kept: enumerate, impl_getinfo, dns_q/s, tcp open/state/send/rcv/abort
; GR8NET internetworking adapter
; Main assembler source code implementing API for Fusion-C based on SDCC
; MSX TCP-IP UNAPI implementation version 1.0
; Initial release by Eugeny Brychkov 2019-05-31
; License: keeps the same as TCP/IP UNAPI v.1.0 by Konamiman, plus requirement
;	   to cite original author when this work or its derivatives are used

CALSLT	.equ	#0x1c		; CALSLT BIOS call
EXTBIO	.equ	#0xffca		; EXTBIO call hook address
BUF	.equ	#0xf55e		; BUF are to put temporary data into
RDSLT	.equ	#0xc		; RDSLT BIOS call
UNAPIID	.equ	#0x2222		; UNAPI EXTBIO identifier
ARG	.equ	#0xf847		; location of ARG

.macro CALL_EXTBIO
	push	ix
	call	EXTBIO
	pop	ix
.endm

; argument stack locations
C_ARG0	.equ	#2+#2		; +2 for calling get_pointer_to_var, +2 for calling caller
C_ARG1	.equ	#6
C_ARG2	.equ	#8
C_ARG3	.equ	#10

TCPIP_S:
	.ascii	"TCP/IP"
	.db	0		; null terminated UNAPI identification string

; Variable area in the RAM
	.area _INITIALIZED
_active_implementation::
	.ds	2		; currently active implementation minus one, this
				; value is used to identify which implementation
				; to call to
_implementation_count::
	.ds	2		; number of implemenations. Populated by tcpip_enumerate
_ram_helper_call_address::
	.ds	2		; RAM helper jump table address in CPU bank 3
_unapi_raw_count::
	.ds	1		; B from EXTBIO count (before helper filter)
unapi_enum_slot:
	.ds	1
unapi_enum_segment:
	.ds	1
unapi_enum_entry:
	.ds	2
implementation_initlist:	; space for 4 implementations
	.ds	#8*#4

; Code starts here
	.area _CODE

	.globl	_TT_DbgImpl

;------------------------------------------------------------------------
; CALL_UNAPI: call the UNAPI - page3 direct, CALSLT (ROM), or RAM helper
;------------------------------------------------------------------------
; in:	IX=call address
;	IYh=slot ID
;	IYl=segment (memory mapper page) number
;	all other registers are input to routine being called
; out:	all registers: output from the routine called
; UNAPI: if entry >= C000h, call directly (ignore slot/segment). Pico+ and
; many ROM stacks install a page-3 jump; without this we skip them when no
; RAM helper is present (bare cart boot).
call_unapi:
	push	af			; preserve AF
	.db	#0xdd,#0x7c		; "ld a,ixh"
	cp	#0xc0
	jr	nc,call_unapi_page3
	.db	#0xfd,#0x7d		; "ld a,iyl" -> segment number info A
	inc	a
	jr	nz,use_ram_helper_for_call

	pop	af			; restore AF
	jp	CALSLT			; go to CALSLT and return to the caller

call_unapi_page3:
	pop	af			; UNAPI AF in
	push	ix
	ld	ix,#call_unapi_page3_ret
	ex	(sp),ix			; (sp)=return, IX=entry
	jp	(ix)
call_unapi_page3_ret:
	ret

use_ram_helper_for_call:
	; we are here to use RAM helper as RAM segment 	is not 0ffh
	pop	af			; restore AF
	push	hl			; preserve argument in HL
	push	de			; preserve argument in DE
	ld	hl,(_active_implementation)
	add	hl,hl			; *2
	add	hl,hl			; *4
	add 	hl,hl			; *8
	ld	de,#implementation_initlist
	add	hl,de			; now points to JP instruction
	pop	de			; restore argument into DE
	ex	(sp),hl			; now HL=argument in HL, (stack)=call address
	ret				; jump to jump table, and return to caller on completion

; Copy the canonical API identifier to ARG without changing discovery inputs.
prime_tcpip_arg:
	push	af
	push	bc
	push	de
	push	hl
	ld	hl,#TCPIP_S
	ld	de,#ARG
	ld	bc,#7
	ldir
	pop	hl
	pop	de
	pop	bc
	pop	af
	ret

; Show a hardware breadcrumb while preserving every main/index register.
; The debug implementation is a no-op outside TT_ROM_DBG builds.
dbg_show:
	push	af
	push	bc
	push	de
	push	hl
	push	ix
	push	iy
	call	_TT_DbgImpl
	pop	iy
	pop	ix
	pop	hl
	pop	de
	pop	bc
	pop	af
	ret

;------------------------------------------------------------------------
; GET_IMPLEMENTATION_CALL: get calling address to implementation
;------------------------------------------------------------------------
; in:	(active_implementation) set accordingly
; out:	IY=slot ID/mem-mapper page#, IX=calling address
; regs:	AF,DE,IX,IY,HL
; The routine assumes that it was called just after call from C is performed
; and IX is pushed. It pops call address and IX out, and returns in case
; active implementation is incorrectly set
get_implementation_call:
	ld	a,(_active_implementation)
	ld	hl,(_implementation_count)
	cp	l			; check given connection number to be in range 0..impl_count-1
	jr	nc,wrong_implementation

	add	a,a
	add	a,a
	add	a,a			; table entry size is 8 bytes
	ld	e,a
	ld	d,#0
	ld	hl,#implementation_initlist
	add	hl,de			; now points to respective entry in the implementation table
	inc	hl
	inc	hl
	inc	hl			; skip to the mapper page #
	ld	e,(hl)			; get mapper page #
	inc	hl
	ld	d,(hl)			; get slot ID
	push	de
	pop	iy			; slot ID/mapper page
	inc	hl
	ld	e,(hl)
	inc	hl
	ld	d,(hl)
	push	de
	pop	ix			; prepare calling address
	ret

wrong_implementation:
	pop	hl			; restore return address
	pop	ix			; restore IX from the stack
	ld	hl,#0xff		; ERR_INV_IMPL error
	ret				; and return to original caller

;------------------------------------------------------------------------
; GET_POINTER_TO_VAR: get pointer to the variable in the stack
;------------------------------------------------------------------------
; in:	L=stack offset (forward)
; out:	HL=pointer to the variable
; regs:	AF
get_pointer_to_var:
	ld	h,#0			; always forward addressing
	add	hl,sp			; now points to variable pointer in the stack
	ld	a,(hl)
	inc	hl
	ld	h,(hl)
	ld	l,a
	ret

;------------------------------------------------------------------------
; TCPIP_ENUMERATE: discover up to four TCP/IP UNAPI implementations.
; Order follows the UNAPI specification: prime ARG, count, then query indices.
; A RAM helper is requested lazily, only if an indexed result is mapped RAM.
;------------------------------------------------------------------------
; in:	nothing
; out:	number of implementations, integer
_tcpip_enumerate::
	; Clear prior results so repeated enumeration cannot reuse stale state.
	xor	a
	ld	h,a
	ld	l,a
	ld	(_active_implementation),hl
	ld	(_implementation_count),hl
	ld	(_ram_helper_call_address),hl
	ld	(_unapi_raw_count),a

	; Count: A=0, B=0, DE=2222h at the EXTBIO hook.
	call	prime_tcpip_arg
	xor	a
	ld	b,a
	ld	de,#UNAPIID
	CALL_EXTBIO
	ld	a,b
	ld	(_unapi_raw_count),a
	ld	a,#0x31
	call	dbg_show
	ld	a,(_unapi_raw_count)

	cp	#5
	jr	c,less_than_4
	ld	a,#4
less_than_4:
	or	a
	jp	z,enumerate_ret0

	; we are here if we have found at least one implementation
	ld	b,a			; number of implementations is in B now
	ld	c,#0x1			; starting with implementation #1
	ld	de,#0			; E=0 initial number of supported implementations counted
	ld	hl,#implementation_initlist	; pointer to implementation array
get_calls:
	; The specification requires ARG to be refreshed for each indexed query.
	ld	a,#0x32
	call	dbg_show
	call	prime_tcpip_arg
	push	bc			; preserve counters
	push	de			; preserve number of supported implementations in E
	push	hl			; preserve pointer to implementation array
	ld	a,c
	ld	de,#UNAPIID		; TCP/IP unapi EXTBIO function ID
	CALL_EXTBIO

	; Save the complete indexed result before any optional helper query.
	ld	(unapi_enum_slot),a
	ld	a,b
	ld	(unapi_enum_segment),a
	ld	(unapi_enum_entry),hl
	ld	a,#0x33
	call	dbg_show

	; Page-3 entry (>=C000h): call directly without a RAM helper.
	ld	a,h
	cp	#0xc0
	jr	nc,page3_impl

	; Segment FFh identifies a ROM implementation; call it through CALSLT.
	ld	a,b
	inc	a			; test mapper page # for 0ffh
	jr	z,rom_impl

	; Mapped RAM needs the standard helper. Request it only now.
	ld	hl,(_ram_helper_call_address)
	ld	a,h
	or	l
	jr	nz,mapped_helper_ready
	ld	de,#UNAPIID
	ld	hl,#0
	ld	a,#0xff
	CALL_EXTBIO
	ld	(_ram_helper_call_address),hl

mapped_helper_ready:
	ld	de,(_ram_helper_call_address)
	ld	a,d
	or	e
	jr	z,skip_implementation
	jr	store_implementation

page3_impl:
	ld	de,#CALSLT		; placeholder; call_unapi uses IX direct
	jr	store_implementation

rom_impl:
	ld	de,#CALSLT

store_implementation:
	pop	hl			; restore pointer to implementation array
	ld	(hl),#0xc3		; JP opcode (safe if BSS was zeroed)
	inc	hl
	ld	(hl),e
	inc	hl
	ld	(hl),d			; populate calling address
	inc	hl
	ld	a,(unapi_enum_segment)
	ld	(hl),a			; mapper page # (will go into IYl)
	inc	hl
	ld	a,(unapi_enum_slot)
	ld	(hl),a			; preserve slot # (will go into IYh)
	inc	hl
	ld	de,(unapi_enum_entry)
	ld	(hl),e
	inc	hl
	ld	(hl),d			; call address (will go into IX)
	inc	hl
	inc	hl			; skip dummy byte
	pop	de
	inc	e			; +1 to number of supported implementations
	jr	next_implementation

skip_implementation:
	pop	hl			; discard saved array pointer
	pop	de			; restore number of supported implementations

next_implementation:
	pop	bc			; implementation counter and current implementation #
	inc	c
	djnz	get_calls

	ex	de,hl			; now number of supported implementatios found is in HL
	ld	(_implementation_count),hl	; and store number of supported inplementations
						; into dedicated variable
	ret

enumerate_ret0:
	ld	hl,#0
	ld	(_implementation_count),hl
	ret

;------------------------------------------------------------------------
; UNAPI_GET_INFO: get current information about the specific implementation
;------------------------------------------------------------------------
; in:	nothing
; out:	pointers to +0:impl_version, +2:unapi_version, +4:impl_string
_tcpip_impl_getinfo::
	push	ix			; preserve IX
	call	get_implementation_call
	xor	a			; UNAPI function #0
	call	call_unapi		; perform call to CALSLT
	pop	ix			; restore IX
	or	a
	jr	nz,error_returned

	push	hl			; preserve returned pointer to implementation name string
	ld	l,#C_ARG1+#2		; +2 for pushed HL
	call	get_pointer_to_var
	ld	(hl),e
	inc	hl
	ld	(hl),d			; API specification version
	ld	l,#C_ARG2+#2		; +2 for pushed HL
	call	get_pointer_to_var
	ld	(hl),c
	inc	hl
	ld	(hl),b			; API implementation version
	ld	l,#C_ARG0+#2		; +2 for pushed HL
	call	get_pointer_to_var

	; now copy implementation name string into BUF. We must do it as strig may be
	; located in other slot, or in mapped RA< which is not easily reachable from C
	ld	de,#BUF			; ugly, but I do not know how to make
					; HIGH and LOW out of 16-bit value in the SDASZ80
	ld	(hl),e
	inc	hl
	ld	(hl),d			; set BUF address to return as pointer to string

	; Name in page 3 → direct copy (Pico+/Konamiman page-3 impl)
	pop	hl			; pointer to string
	push	hl
	ld	a,h
	cp	#0xc0
	jr	nc,use_direct_to_read

	.db	#0xfd,#0x7d		; "ld a,iyl" -> segment number info A
	inc	a			; check for 0ffh
	jr	z,use_rdslt_to_read_slot

	; we are here to read using RAM helper
	pop	de			; restore pointer to target string into DE
	push	ix
	ld	hl,(_ram_helper_call_address)
	inc	hl
	inc	hl
	inc	hl			; skip to data read routine
	push	hl
	pop	ix			; now RAM read call address is in IX
	ex	de,hl			; now pointer to target string is in HL
	ld	de,#BUF			; target location to copy string to
loop_copy_using_ramhelper:
	push	de			; preserve taret location pointer
	ld	bc,#return_to_loop
	push	bc			; prepare return address in the stack
	.db	#0xfd,#0x7c		; "ld a,iyh" -> slot ID
	.db	#0xfd,#0x45		; "ld b,iyl" -> segment number
	jp	(ix)			; jump to data read, will return to return_to_loop,
					; HL/DE are preserved
return_to_loop:
	pop	de			; restore target location pointer
	ld	(de),a
	inc	de
	inc	hl
	or	a
	jr	nz,loop_copy_using_ramhelper
	pop	ix
	jr	error_returned		; return with A=0 (HL will be 0)

use_direct_to_read:
	pop	hl
	ld	de,#BUF
loop_copy_direct:
	ld	a,(hl)
	ld	(de),a
	inc	hl
	inc	de
	or	a
	jr	nz,loop_copy_direct
	jr	error_returned

use_rdslt_to_read_slot:
	; we are here to copy implementation string to BUF using RDSLT
	pop	hl			; restore pointer to the string in target slot
	ld	de,#BUF			; target location
loop_copy_using_rdslt:
	.db	#0xfd,#0x7c		; "ld a,iyh" -> slot ID
	push	de
	call	RDSLT			; read byte into A, preserves DE
	pop	de
	ld	(de),a
	inc	de
	inc	hl
	or	a
	jr	nz,loop_copy_using_rdslt
	; we are here with A=0

error_returned:
	ld	l,a
	ld	h,#0			; return value
	ret


;------------------------------------------------------------------------
; Shared return helpers (kept after TT slim of unused UDP/RAW/ICMP APIs)
;------------------------------------------------------------------------
no_err_return:
	ld	hl,#0
	ret

no_err_ret_ix:
	pop	ix
	jp	no_err_return

error_returned_ix:
	pop	ix
	jp	error_returned

go_calslt_return_status:
	call	call_unapi
	pop	ix
	jp	error_returned

; TCPIP_DNS_Q: start name resolution process
;------------------------------------------------------------------------
; in:	pointer to +0:host name string, +2:pointer to tcpip_unapi_dns_q strcuture
; out:	HL=completion code
_tcpip_dns_q::
	push	ix			; preserve IX
	call	get_implementation_call
	ld	l,#C_ARG1+#2		; +2 for pushed IX
	call	get_pointer_to_var	; get pointer to the structure into HL
	ld	b,(hl)			; get flags from the structure
	inc	hl
	push	hl			; preserve address to the state field in the struct
	xor	a
	ld	b,#6
clear_dns_q_struct:
	ld	(hl),a
	inc	hl
	djnz	clear_dns_q_struct

	ld	l,#C_ARG0+#2+#2		; +2 for pushed IX, +2 for pushed HL
	call	get_pointer_to_var	; get pointer to the host name string
	ld	a,#6			; UNAPI function #6
	call	call_unapi
	ex	(sp),hl			; now HL has address to the struct, and (stack)=part of IP address
	ld	(hl),b			; put state in in any case
	inc	hl
	ld	(hl),#0			; (substate is meaningless for DNS_Q)
	inc	hl			; now HL points to IP address field in the struct
	ex	af,af'
	ld	a,b
	ex	af,af'			; A' is state now
	pop	bc			; restore high bytes of IP address into BC
	pop	ix			; restore IX
	or	a			; test for error returned
	jp	nz,error_returned	; and return this value if error condition

	ex	af,af'			; now A=state
	or	a
	jp	z,no_err_return		; state is zero, no update to struct,
					; and return ERR_OK
	; we are here if state (A) is not zero, update the IP address

from_dns_s:
	ld	(hl),c
	inc	hl
	ld	(hl),b
	inc	hl
	ld	(hl),e
	inc	hl
	ld	(hl),d			; put IP address into struct
	jp	no_err_return		; and return with state ERR_OK

;------------------------------------------------------------------------
; TCPIP_DNS_S: obtain name resolution process results
;------------------------------------------------------------------------
; in:	pointer to +0:host name string, +2:pointer to tcpip_unapi_dns_q strcuture
; out:	HL=completion code
_tcpip_dns_s::
	push	ix			; preserve IX
	call	get_implementation_call
	ld	l,#C_ARG0+#2		; +2 for pushed IX
	call	get_pointer_to_var	; get pointer to the structure into HL
	ld	b,(hl)			; get flags from the structure
	inc	hl
	push	hl			; preserve address to the state field in the struct

	ld	a,#7			; UNAPI function #7
	call	call_unapi
	ex	(sp),hl			; now HL has address to the struct, and (stack)=part of IP address
	ld	(hl),b			; put state in in any case
	inc	hl
	ld	(hl),c			; put substate in
	inc	hl			; now HL points to IP address field in the struct
	ex	af,af'
	ld	a,b
	ex	af,af'			; A' is state now
	pop	bc			; restore high bytes of IP address into BC
	pop	ix			; restore IX
	or	a			; test for error returned
	jp	nz,error_returned	; and return this value if error condition

	ex	af,af'			; now A=state
	cp	#2
	jp	nz,no_err_return		; state is zero, no update to struct,
					; and return ERR_OK
	jr	from_dns_s		; go perform CALSLT and process return values
;------------------------------------------------------------------------
; in:	pointer to +0:pointer to tcpip_unapi_tcp_conn_parms struct,
;	+2:pointer to connection number
; out:	HL=completion code
_tcpip_tcp_open::
	push	ix			; preserve IX
	call	get_implementation_call
	ld	l,#C_ARG0+#2		; +2 for pushed IX
	call	get_pointer_to_var	; get structure pointer
	ld	a,#13			; UNAPI function #13
	call	call_unapi
	ld	l,#C_ARG1

put_conn_number_arg:
	pop	ix			; restore IX
	or	a
	jp	nz,error_returned

	; we are here if return code is ERR_OK (0)
	call	get_pointer_to_var	; get pointer to connection number variable (L)
	xor	a
	ld	(hl),b
	inc	hl
	ld	(hl),a			; set connection number
	ld	l,a
	ld	h,a
	ret				; return with exit code 0

;------------------------------------------------------------------------
; TCPIP_TCP_ABORT: abort TCP socket
;------------------------------------------------------------------------
; in:	pointer to +0:connection number
; out:	HL=completion code
_tcpip_tcp_abort::
	push	ix			; preserve IX
	call	get_implementation_call
	ld	l,#C_ARG0+#2		; +2 for pushed IX
	call	get_pointer_to_var	; get connection value
	ld	b,l
	ld	a,#15			; UNAPI function #15
	jp	go_calslt_return_status	; perform CALSLT and return status popping IX

;------------------------------------------------------------------------
; TCPIP_TCP_STATE: get TCP socket state
;------------------------------------------------------------------------
; in:	pointer to +0:connection number, +2:pointer to tcpip_unapi_tcp_conn_parms struct
; out:	HL=completion code
_tcpip_tcp_state::
	push	ix			; preserve IX
	call	get_implementation_call
	ld	l,#C_ARG0+#2		; +2 for pushed IX
	call	get_pointer_to_var	; get connection value
	ld	b,l
	ld	l,#C_ARG1+#2		; +2 for pushed IX
	call	get_pointer_to_var	; get pointer to the parameters struct
	ld	a,#16			; UNAPI function #16
	call	call_unapi
	or	a
	jp	nz,error_returned_ix	; return with A as error code popping IX out

	ex	de,hl			; now DE=total incoming bytes, and HL=urgent incoming bytes
	push	hl			; preserve urgent incoming bytes
	ld	l,#C_ARG1+#2+#2		; +2 for pushed IX, and +2 for pushed HL
	call	get_pointer_to_var	; get pointer to struct
	push	de
	ld	de,#11
	add	hl,de			; now points to the connection state field in the struct
	pop	de
	ld	(hl),b			; connection state
	inc	hl
	ld	(hl),c			; close reason
	inc	hl
	ld	(hl),e
	inc	hl
	ld	(hl),d			; number of total incoming bytes
	inc	hl
	pop	de
	ld	(hl),e
	inc	hl
	ld	(hl),d			; number of urgent incoming bytes
	inc	hl
	push	ix
	pop	de
	ld	(hl),e
	inc	hl
	ld	(hl),d			; available free space
	jp	no_err_ret_ix		; return HL=0 (ERR_OK) restoring IX

;------------------------------------------------------------------------
; TCPIP_TCP_SEND: send TCP data
;------------------------------------------------------------------------
; in:	pointer to +0:connection number, +2:pointer to data location,
;	+4:data length, +6:flags
; out:	HL=completion code
_tcpip_tcp_send::
	push	ix			; preserve IX
	call	get_implementation_call
	ld	l,#C_ARG0+#2		; +2 for pushed IX
	call	get_pointer_to_var	; get connection value
	ld	b,l
	ld	l,#C_ARG3+#2		; +2 for pushed IX
	call	get_pointer_to_var	; get flags value
	ld	c,l
	ld	l,#C_ARG1+#2		; +2 for pushed IX
	call	get_pointer_to_var	; get pointer to the data locaiton
	ex	de,hl
	ld	l,#C_ARG2+#2		; +2 for pushed IX
	call	get_pointer_to_var	; get data length value
	ld	a,#17			; UNAPI function #17
	jp	go_calslt_return_status	; perform CALSLT and return status popping IX

;------------------------------------------------------------------------
; TCPIP_TCP_RCV: retrieve TCP data
;------------------------------------------------------------------------
; in:	pointer to +0:connection number, +2:pointer to data location,
;	+4:max data length, +6:pointer to tcpip_unapi_tcp_conn_parms struct
; out:	HL=completion code
_tcpip_tcp_rcv::
	push	ix			; preserve IX
	call	get_implementation_call
	ld	l,#C_ARG0+#2		; +2 for pushed IX
	call	get_pointer_to_var	; get connection value
	ld	b,l
	ld	l,#C_ARG1+#2		; +2 for pushed IX
	call	get_pointer_to_var	; get pointer to the data locaiton in HL
	ex	de,hl			; and put it into DE
	ld	l,#C_ARG2+#2		; +2 for pushed IX
	call	get_pointer_to_var	; get data length value into HL
	ld	a,#18			; UNAPI function #18
	call	call_unapi
	pop	ix			; restore IX
	or	a
	jp	nz,error_returned	; return with A as error code

	push	hl			; preserve urgent data size
	ld	l,#C_ARG3+#2		; +2 for pushed HL
	call	get_pointer_to_var	; get pointer to struct
	ld	de,#13
	add	hl,de			; skip to incoming bytes field in the struct
	ld	(hl),c
	inc	hl
	ld	(hl),b			; total incoming bytes
	inc	hl
	pop	de
	ld	(hl),e
	inc	hl
	ld	(hl),d			; urgent incoming bytes
	jp	no_err_return		; return HL=0 (ERR_OK)

; Variable initialization area
	.area _INITIALIZER

__xinit__active_implementation:
	.dw	0		; currently active implementation minus one, this
				; value is used to identify which implementation
				; to call to
__xinit__implementation_count:
	.dw	0		; number of implemenations. Populated by tcpip_enumerate
__xinit__ram_helper_call_address:
	.dw	0		; RAM helper jump table address in CPU bank 3
__xinit__unapi_raw_count:
	.db	0
__xinit_unapi_enum_slot:
	.db	0
__xinit_unapi_enum_segment:
	.db	0
__xinit_unapi_enum_entry:
	.dw	0

__xinit_implementation_initlist:	; space for 4 implementations
	; implementaton 0
	.db	#0xc3		; +0 (1): JP instruction code
	.dw	0		; +1 (2): call address for the implementation
	.dw	0		; +3 (1): segment (memory mapper page) number (into IYl)
				; +4 (1): slot ID (into IYh)
	.dw	0		; +5 (2): calling address in target slot and segment (into IX)
	.db	0		; +7 (1): placeholder to get record to 8 bytes
	; implementaton 1
	.db	#0xc3
	.dw	0
	.dw	0
	.dw	0
	.db	0		; +7 (1): placeholder to get record to 8 bytes
	; implementaton 2
	.db	#0xc3
	.dw	0
	.dw	0
	.dw	0
	.db	0		; +7 (1): placeholder to get record to 8 bytes
	; implementaton 3
	.db	#0xc3
	.dw	0
	.dw	0
	.dw	0
	.db	0		; +7 (1): placeholder to get record to 8 bytes

end:
	.area _CABS (ABS)

; End of implementation