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

MOUSEVT	STRUCT
	cond		WORD ?
	button		WORD ?
	x		WORD ?
	y		WORD ?
MOUSEVT	ENDS

MOUSE	STRUCT
	buffer		MOUSEVT 256 DUP (?)
	read		BYTE ?
	write		BYTE ?
MOUSE	ENDS

_DATA	SEGMENT	BYTE PUBLIC USE32 'DATA'
	; Defined in event_dos.c
	EXTERN	_mouse:MOUSE
_DATA	ENDS

DGROUP	GROUP	_DATA

_TEXT	SEGMENT	BYTE PUBLIC USE32 'CODE'
	ASSUME	cs:_TEXT

	PUBLIC	mouse_handler_
mouse_handler_:
	push	ds
	mov	cx, DGROUP
	mov	ds, cx
	xor	ecx, ecx
	mov	cl, [_mouse.write]
	mov	ch, [_mouse.read]
	inc	cl
	cmp	cl, ch
	je	full_buffer
	dec	cl
	xor	ch, ch
	mov	[_mouse.buffer+ecx*8].cond, ax
	mov	[_mouse.buffer+ecx*8].button, bx
	mov	[_mouse.buffer+ecx*8].x, si
	mov	[_mouse.buffer+ecx*8].y, di
	inc	[_mouse.write]
full_buffer:
	pop	ds
	retf
	PUBLIC mouse_handler_end_
keyboard_handler_end_:
_TEXT	ENDS
	END
