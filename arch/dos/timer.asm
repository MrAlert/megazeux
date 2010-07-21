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

TIMER	STRUCT
	ticks		DWORD ?
	off		DWORD ?
	len		DWORD ?
	count		DWORD ?
	normal		DWORD ?
	pad		DWORD ?
	oldhandler	FWORD ?
TIMER	ENDS

_DATA	SEGMENT	BYTE PUBLIC USE32 'DATA'
	; Defined in platform.c
	EXTERN	_timer:TIMER
_DATA	ENDS

DGROUP GROUP _DATA

_TEXT	SEGMENT	BYTE PUBLIC USE32 'CODE'
	ASSUME	cs:_TEXT

	PUBLIC	timer_handler_
timer_handler_:
	push	ds
	push	ebx
	mov	bx, DGROUP
	mov	ds, bx
	mov	ebx, [_timer.len]
	add	[_timer.ticks], ebx	; timer.ticks += timer.length;
	mov	ebx, [_timer.off]
	add	ebx, [_timer.count]	; timer.offset += timer.count;
	cmp	ebx, [_timer.normal]	; if(timer.count >= timer.normal)
	jae	chain_time		;   goto chain_time;
	mov	[_timer.off], ebx
	pop	ebx
	pop	ds
	push	ax
	mov	al, 020h
	out	020h, al		; Send EOI to 8259 PIC
	pop	ax
	iretd				; return;
chain_time:
	sub	ebx, [_timer.normal]	; timer.offset -= timer.normal;
	mov	[_timer.off], ebx
	pushfd
	call	fword ptr [_timer.oldhandler]
	pop	ebx
	pop	ds
	iretd
	PUBLIC timer_handler_end_
timer_handler_end_:
_TEXT	ENDS
	END
