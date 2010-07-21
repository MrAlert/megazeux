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

KEYBD	STRUCT
	buffer		BYTE 256 DUP (?)
	read		BYTE ?
	write		BYTE ?
	pad1		WORD ?
	pad2		DWORD ?
	oldhandler	FWORD ?
KEYBD	ENDS

_DATA	SEGMENT	BYTE PUBLIC USE32 'DATA'
	; Defined in event_dos.c
	EXTERN	_keyboard:KEYBD
_DATA	ENDS

DGROUP	GROUP	_DATA

_TEXT	SEGMENT	BYTE PUBLIC USE32 'CODE'
	ASSUME	cs:_TEXT

	PUBLIC	keyboard_handler_
keyboard_handler_:
	push	ds
	push	ebx
	mov	bx, DGROUP
	mov	ds, bx
	push	eax
	in	al, 60h				; Get scan code
	xor	ebx, ebx
	mov	bl, [_keyboard.write]
	mov	bh, [_keyboard.read]
	inc	bl
	cmp	bl, bh
	je	full_buffer
	dec	bl
	xor	bh, bh
	mov	[ebx+_keyboard.buffer], al
	inc	[_keyboard.write]
full_buffer:
	;in	al, 61h
	;or	al, 80h
	;out	61h, al
	;and	al, 7Fh
	;out	61h, al
	;mov	al, 020h
	;out	020h, al
	pushfd
	call	fword ptr [_keyboard.oldhandler]
	pop	eax
	pop	ebx
	pop	ds
	iretd
	PUBLIC keyboard_handler_end_
keyboard_handler_end_:
_TEXT	ENDS
	END
