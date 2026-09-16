; MSX1TTE — Screen 2 teletext row expand (Z80)
; void ExpandRowScr2(const u8* cells)  — cells in HL (sdcccall 1)
;
; Tile-centric: tiles 1..30 each pull ink from at most two 6px cells.
; Left-wins clash; left space / solid (fg==bg) defers to first ink cell.

	.module scr2_expand
	.area _CODE

	.globl _ExpandRowScr2
	.globl _g_Scr2Pat
	.globl _g_Scr2Col
	.globl _g_MapAttr
	.globl _g_GlyphPat
	.globl _g_TileCell
	.globl _g_TileLeftMask
	.globl _g_TCell0
	.globl _g_TCell1
	.globl _g_THow0
	.globl _g_THow1

	.area _DATA
_er_glyphs:
	.ds	40
_er_attrs:
	.ds	40
_er_pats:
	.ds	80
_er_lines:
	.ds	8
er_cells:
	.ds	2
er_i:
	.ds	1
er_tidx:
	.ds	1
er_c0:
	.ds	1
er_c1:
	.ds	1
er_how0:
	.ds	1
er_how1:
	.ds	1
er_leftm:
	.ds	1
er_a0:
	.ds	1
er_a1:
	.ds	1
er_g0:
	.ds	1
er_oa:
	.ds	1
er_same:
	.ds	1
er_asamef:
	.ds	1
er_or:
	.ds	1
er_tmp:
	.ds	1
er_p0:
	.ds	2
er_p1:
	.ds	2

	.area _CODE

; A=ink, B=how → A (preserves BC/DE/HL except A and uses er_tmp)
; how: 0=>>0 1=>>2 2=>>4 3=>>6 4=<<2 5=<<4 6=<<6
er_apply_how:
	ld	(#er_tmp), a
	ld	a, b
	or	a
	jr	z, er_h0
	dec	a
	jr	z, er_h1
	dec	a
	jr	z, er_h2
	dec	a
	jr	z, er_h3
	dec	a
	jr	z, er_h4
	dec	a
	jr	z, er_h5
	ld	a, (#er_tmp)
	add	a, a
	add	a, a
	add	a, a
	add	a, a
	add	a, a
	add	a, a
	ret
er_h0:
	ld	a, (#er_tmp)
	ret
er_h1:
	ld	a, (#er_tmp)
	srl	a
	srl	a
	ret
er_h2:
	ld	a, (#er_tmp)
	srl	a
	srl	a
	srl	a
	srl	a
	ret
er_h3:
	ld	a, (#er_tmp)
	srl	a
	srl	a
	srl	a
	srl	a
	srl	a
	srl	a
	ret
er_h4:
	ld	a, (#er_tmp)
	add	a, a
	add	a, a
	ret
er_h5:
	ld	a, (#er_tmp)
	add	a, a
	add	a, a
	add	a, a
	add	a, a
	ret

_ExpandRowScr2::
	di
	push	ix
	push	iy
	push	bc
	push	de

	ld	(#er_cells), hl

	; ---- unpack 40 cells ----
	xor	a
	ld	(#er_i), a
er_unpack:
	ld	a, (#er_i)
	cp	#40
	jp	nc, er_margins

	ld	l, a
	ld	h, #0
	add	hl, hl
	ld	de, (#er_cells)
	add	hl, de
	ld	c, (hl)			; glyph
	inc	hl
	ld	b, (hl)			; attr

	ld	a, (#er_i)
	ld	e, a
	ld	d, #0
	ld	hl, #_er_glyphs
	add	hl, de
	ld	(hl), c
	ld	hl, #_er_attrs
	add	hl, de
	ld	(hl), b

	ld	l, c
	ld	h, #0
	add	hl, hl
	ld	de, #_g_GlyphPat
	add	hl, de
	ld	e, (hl)
	inc	hl
	ld	d, (hl)			; DE = pattern ptr
	ld	a, (#er_i)
	ld	l, a
	ld	h, #0
	add	hl, hl
	ld	bc, #_er_pats
	add	hl, bc
	ld	(hl), e
	inc	hl
	ld	(hl), d

	ld	a, (#er_i)
	inc	a
	ld	(#er_i), a
	jp	er_unpack

er_margins:
	ld	a, (#_g_MapAttr + 0)
	ld	b, #8
	ld	hl, #_g_Scr2Pat
	ld	de, #_g_Scr2Col
er_mg0:
	ld	(hl), #0
	inc	hl
	ld	(de), a
	inc	de
	djnz	er_mg0
	ld	b, #8
	ld	hl, #(_g_Scr2Pat + 248)
	ld	de, #(_g_Scr2Col + 248)
er_mg31:
	ld	(hl), #0
	inc	hl
	ld	(de), a
	inc	de
	djnz	er_mg31

	; ---- tiles 1..30 ----
	xor	a
	ld	(#er_tidx), a
er_tile_loop:
	ld	a, (#er_tidx)
	cp	#30
	jp	nc, er_done

	ld	c, a
	ld	b, #0

	ld	hl, #_g_TCell0
	add	hl, bc
	ld	a, (hl)
	ld	(#er_c0), a
	ld	hl, #_g_TCell1
	add	hl, bc
	ld	a, (hl)
	ld	(#er_c1), a
	ld	hl, #_g_THow0
	add	hl, bc
	ld	a, (hl)
	ld	(#er_how0), a
	ld	hl, #_g_THow1
	add	hl, bc
	ld	a, (hl)
	ld	(#er_how1), a
	ld	hl, #_g_TileLeftMask
	add	hl, bc
	ld	a, (hl)
	ld	(#er_leftm), a

	ld	a, (#er_c0)
	ld	e, a
	ld	d, #0
	ld	hl, #_er_attrs
	add	hl, de
	ld	a, (hl)
	ld	(#er_a0), a
	ld	hl, #_er_glyphs
	add	hl, de
	ld	a, (hl)
	ld	(#er_g0), a
	ld	hl, #_er_pats
	add	hl, de
	add	hl, de
	ld	a, (hl)
	inc	hl
	ld	h, (hl)
	ld	l, a
	ld	(#er_p0), hl

	ld	a, (#er_c1)
	ld	e, a
	ld	d, #0
	ld	hl, #_er_attrs
	add	hl, de
	ld	a, (hl)
	ld	(#er_a1), a
	ld	hl, #_er_pats
	add	hl, de
	add	hl, de
	ld	a, (hl)
	inc	hl
	ld	h, (hl)
	ld	l, a
	ld	(#er_p1), hl

	ld	a, (#er_c0)
	ld	hl, #er_c1
	cp	(hl)
	ld	a, #1
	jr	z, er_same_set
	xor	a
er_same_set:
	ld	(#er_same), a

	ld	hl, (#er_p0)
	ld	de, (#er_p1)
	ld	ix, #_er_lines
	xor	a
	ld	(#er_or), a
	ld	b, #8
er_line:
	push	bc
	push	de
	ld	a, (hl)
	and	a, #0xFC
	inc	hl
	push	hl
	ld	c, a
	ld	a, (#er_how0)
	ld	b, a
	ld	a, c
	call	er_apply_how
	ld	c, a
	ld	a, (#er_same)
	or	a
	jr	nz, er_lstore
	pop	hl
	pop	de
	ld	a, (de)
	and	a, #0xFC
	inc	de
	push	de
	push	hl
	ld	l, a
	ld	a, (#er_how1)
	ld	b, a
	ld	a, l
	call	er_apply_how
	or	a, c
	ld	c, a
er_lstore:
	ld	0 (ix), c
	inc	ix
	ld	a, (#er_or)
	or	a, c
	ld	(#er_or), a
	pop	hl
	pop	de
	pop	bc
	djnz	er_line

	ld	a, (#er_a0)
	ld	(#er_oa), a
	ld	a, (#er_or)
	or	a
	jr	z, er_own_done
	ld	a, (#er_g0)
	cp	a, #0x20
	jr	z, er_own_scan
	ld	a, (#er_a0)
	ld	b, a
	rrca
	rrca
	rrca
	rrca
	and	a, #7
	ld	c, a
	ld	a, b
	and	a, #7
	cp	a, c
	jr	nz, er_own_done
er_own_scan:
	ld	a, (#er_or)
	ld	b, #8
	ld	e, #0
er_osbit:
	rlca
	jr	c, er_osgot
	inc	e
	djnz	er_osbit
	jr	er_own_done
er_osgot:
	ld	a, (#er_tidx)
	ld	l, a
	ld	h, #0
	add	hl, hl
	add	hl, hl
	add	hl, hl
	ld	bc, #_g_TileCell
	add	hl, bc
	ld	d, #0
	add	hl, de
	ld	a, (hl)
	ld	e, a
	ld	d, #0
	ld	hl, #_er_attrs
	add	hl, de
	ld	a, (hl)
	ld	(#er_oa), a
er_own_done:

	ld	a, (#er_oa)
	ld	l, a
	ld	h, #0
	ld	bc, #_g_MapAttr
	add	hl, bc
	ld	c, (hl)
	ld	a, (#er_tidx)
	inc	a
	ld	l, a
	ld	h, #0
	add	hl, hl
	add	hl, hl
	add	hl, hl
	ld	de, #_g_Scr2Col
	add	hl, de
	ld	b, #8
	ld	a, c
er_colfill:
	ld	(hl), a
	inc	hl
	djnz	er_colfill

	ld	a, (#er_a0)
	ld	hl, #er_a1
	cp	(hl)
	ld	a, #1
	jr	z, er_asame
	xor	a
er_asame:
	ld	(#er_asamef), a

	ld	a, (#er_tidx)
	inc	a
	ld	l, a
	ld	h, #0
	add	hl, hl
	add	hl, hl
	add	hl, hl
	ld	de, #_g_Scr2Pat
	add	hl, de
	push	hl
	pop	iy
	ld	ix, #_er_lines
	ld	b, #8
er_pemit:
	ld	c, 0 (ix)
	inc	ix
	ld	a, (#er_asamef)
	or	a
	jr	nz, er_pout
	ld	a, (#er_oa)
	ld	hl, #er_a0
	cp	(hl)
	jr	nz, er_pright
	ld	a, c
	ld	hl, #er_leftm
	and	a, (hl)
	ld	c, a
	jr	er_pout
er_pright:
	ld	a, (#er_leftm)
	cpl
	and	a, c
	ld	c, a
er_pout:
	ld	0 (iy), c
	inc	iy
	djnz	er_pemit

	ld	a, (#er_tidx)
	inc	a
	ld	(#er_tidx), a
	jp	er_tile_loop

er_done:
	pop	de
	pop	bc
	pop	iy
	pop	ix
	ei
	ret
