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
	EXTERN	_ticks:DWORD
	EXTERN	_tick_offset:DWORD
	EXTERN	_tick_oldhandler:FWORD
	EXTERN	_tick_len:DWORD
	EXTERN	_tick_count:DWORD
	EXTERN	_tick_normal:DWORD
_DATA	ENDS

DGROUP GROUP _DATA

_TEXT	SEGMENT	BYTE PUBLIC USE32 'CODE'
	ASSUME	cs:_TEXT

	PUBLIC	tick_handler_
tick_handler_:
	push	ax
	push	eax
	push	ax
	mov	al, 020h
	out	020h, al			; Send EOI to 8259 PIC
	pop	ax
	push	ebp
	mov	ebp, esp			; [ebp+4] points to retf ptr
	push	ds
	push	ebx
	mov	bx, DGROUP
	mov	ds, bx
	mov	ebx, [_tick_len]
	add	[_ticks], ebx			; ticks += tick_len;
	mov	ebx, [_tick_offset]
	add	[_tick_count], ebx		; tick_offset += tick_count;
	cmp	ebx, [_tick_normal]		; if(tick_count >= tick_normal)
	jae	chain_time			;   goto chain_time;
	mov	[_tick_offset], ebx
	pop	ebx
	pop	ds
	pop	ebp
	pop	eax
	pop	ax
	iretd					; return;
chain_time:
	sub	ebx, [_tick_normal]		; tick_offset -= tick_normal;
	mov	[_tick_offset], ebx
	mov	ebx, DWORD PTR [_tick_oldhandler]
	mov	[ebp+4], ebx
	mov	bx, WORD PTR [_tick_oldhandler+4]
	mov	[ebp+8], bx			; put tick_oldhander on stack
	pop	ebx
	pop	ds
	pop	ebp				; jump to function pointer on
	retf					; stack using retf
_TEXT	ENDS
	END
