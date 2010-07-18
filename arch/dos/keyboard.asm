; MegaZeux
;
; Copyright (C) 2010 Alan Williams <mralert@gmail.com>
;
; This program is free software; you can redistribute it and/or
; modify it under the terms of the GNU General Public License as
; published by the Free Software Foundation; either version 2 of
; the License, or (at your option) any later version.
;
; This program is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
; General Public License for more details.
;
; You should have received a copy of the GNU General Public License
; along with this program; if not, write to the Free Software
; Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

_DATA	SEGMENT	BYTE PUBLIC USE32 'DATA'
	; Defined in platform.c
	EXTERN	_keyboard_buffer:BYTE
	EXTERN	_keyboard_read:BYTE
	EXTERN	_keyboard_write:BYTE
_DATA	ENDS

DGROUP GROUP _DATA

_TEXT	SEGMENT	BYTE PUBLIC USE32 'CODE'
	ASSUME	cs:_TEXT

	PUBLIC	keyboard_handler_
keyboard_handler_:
	push	ds
	push	ebx
	mov	bx, DGROUP
	mov	ds, bx
	push	eax
key_loop:
	in	al, 60h				; Get scan code
	xor	ebx, ebx
	mov	bl, [_keyboard_write]
	mov	bh, [_keyboard_read]
	inc	bl
	cmp	bl, bh
	je	discard_key
	dec	bl
	xor	bh, bh
	mov	[ebx+_keyboard_buffer], al
	inc	[_keyboard_write]
discard_key:
	in	al, 61h
	or	al, 80h
	out	61h, al
	and	al, 7Fh
	out	61h, al
	mov	al, 020h
	out	020h, al			; Send EOI to 8259 PIC
	pop	eax
	pop	ebx
	pop	ds
	iretd
	PUBLIC keyboard_handler_end_
keyboard_handler_end_:
_TEXT	ENDS
	END
