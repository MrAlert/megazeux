/* MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
 * Copyright (C) 2010 Alan Williams <mralert@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <i86.h>
#include <conio.h>
#include "platform.h"
#include "platform_dos.h"

static const int ps2_cards[] = {
  DISPLAY_ADAPTER_NONE,
  DISPLAY_ADAPTER_MDA,
  DISPLAY_ADAPTER_CGA,
  DISPLAY_ADAPTER_CGA,
  DISPLAY_ADAPTER_EGA_COLOR,
  DISPLAY_ADAPTER_EGA_MONO,
  DISPLAY_ADAPTER_CGA,
  DISPLAY_ADAPTER_VGA_MONO,
  DISPLAY_ADAPTER_VGA_COLOR,
  DISPLAY_ADAPTER_CGA,
  DISPLAY_ADAPTER_MCGA_COLOR,
  DISPLAY_ADAPTER_MCGA_MONO,
  DISPLAY_ADAPTER_MCGA_COLOR
};

const char *disp_names[] = {
  "None",
  "MDA",
  "CGA",
  "EGA mono",
  "EGA",
  "VGA mono",
  "VGA+",
  "MCGA mono",
  "MCGA"
};

int detect_graphics(void)
{
  union REGS reg;

  // Try calling VGA Identity Adapter function
  reg.w.ax = 0x1A00;
  int386(0x10, &reg, &reg);

  // Do we have PS/2 video BIOS?
  if(reg.h.al == 0x1A)
  {
    // BL > 0x0C => CGA hardware
    if(reg.h.bl > 0x0C)
      return DISPLAY_ADAPTER_CGA;
    return ps2_cards[reg.h.bl];
  }
  else
  {
    // Set alternate function service
    reg.h.ah = 0x12;
    // Set to return EGA information
    reg.w.bx = 0x0010;
    int386(0x10, &reg, &reg);
    // Is EGA there?
    if(reg.w.bx != 0x0010)
    {
      // Since we have EGA BIOS, get details
      reg.h.ah = 0x12;
      reg.h.bl = 0x10;
      int386(0x10, &reg, &reg);
      // Do we have color EGA?
      if(!reg.h.bh)
        return DISPLAY_ADAPTER_EGA_COLOR;
      else
        return DISPLAY_ADAPTER_EGA_MONO;
    }
    else
    {
      // Let's try equipment determination service
      int386(0x11, &reg, &reg);
      switch(reg.h.al & 0x30)
      {
        // No graphics card at all? This is a stupid machine!
        case 0x00: return DISPLAY_ADAPTER_NONE;
        case 0x30: return DISPLAY_ADAPTER_MDA;
        default: return DISPLAY_ADAPTER_CGA;
      }
    }
  }
}

#define CONFIG_SET_TIMER

#ifdef CONFIG_SET_TIMER
#define TIMER_TICK 2
#define TIMER_CLK 3579545
#define TIMER_COUNT (TIMER_TICK * TIMER_CLK / 3000)
#define TIMER_NORMAL 65536
#else
#define TIMER_COUNT 65536
#define TIMER_CLK 3579545
#define TIMER_TICK (TIMER_COUNT * 3000 / TIMER_CLK)
#define TIMER_NORMAL 65536
#endif

// Implemented in timer.asm
void __interrupt tick_handler (void);

volatile Uint32 ticks = 0;
volatile Uint32 tick_offset = 0;
volatile void (__interrupt __far *tick_oldhandler)(void);
const Uint32 tick_len = TIMER_TICK;
const Uint32 tick_count = TIMER_COUNT;
const Uint32 tick_normal = TIMER_NORMAL;

void delay(Uint32 ms)
{
  ms += ticks;
  while(ticks < ms);
}

Uint32 get_ticks(void)
{
  return ticks;
}

bool platform_init(void)
{
  union REGS reg;
  void far *handler_ptr;

  // DPMI get protected mode vector
  reg.x.eax = 0x0204;
  // IRQ0 - system timer
  reg.h.bl = 0x08;
  int386(0x31, &reg, &reg);
  tick_oldhandler = MK_FP(reg.w.cx, reg.x.edx);

  // DPMI set protected mode vector
  reg.x.eax = 0x0205;
  reg.h.bl = 0x08;
  handler_ptr = (void far *)tick_handler;
  reg.w.cx = FP_SEG(handler_ptr);
  reg.x.edx = FP_OFF(handler_ptr);
  int386(0x31, &reg, &reg);

  // If carry flag set, then failed
  if(reg.x.cflag & 0x01)
    return false;

#ifdef CONFIG_SET_TIMER
  // Counter 0, read lsb-msb
  outp(0x43, 0x34);
  outp(0x40, tick_count & 0xFF);
  outp(0x40, tick_count >> 8);
#endif
  delay(1000);
  return true;
}

void platform_quit(void)
{
  union REGS reg;

#ifdef CONFIG_SET_TIMER
  // Counter 0, read lsb-msb
  outp(0x43, 0x34);
  outp(0x40, tick_normal & 0xFF);
  outp(0x40, tick_normal >> 8);
#endif

  // DPMI set protected mode vector
  reg.x.eax = 0x0205;
  reg.h.bl = 0x08;
  reg.w.cx = FP_SEG(tick_oldhandler);
  reg.x.edx = FP_OFF(tick_oldhandler);
  int386(0x31, &reg, &reg);
  // If it fails, I guess we just have to hope
  // it doesn't explode when we exit
}
