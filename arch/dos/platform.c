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
#include <dos.h>
#include "platform.h"
#include "platform_dos.h"

static const int ps2_cards[] =
{
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

const char *disp_names[] =
{
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

int lock_region(const volatile void *region, size_t length)
{
  union REGS reg;
  unsigned int linear;

  linear = (unsigned int)region;

  // DPMI lock linear region
  reg.w.ax = 0x600;
  reg.w.bx = linear >> 16;
  reg.w.cx = linear & 0xFFFF;
  reg.w.si = length >> 16;
  reg.w.di = length & 0xFFFF;
  int386(0x31, &reg, &reg);
  return !reg.x.cflag;
}

#define CONFIG_SET_TIMER

#define TIMER_NORMAL 65536
#define TIMER_CLK 3579545

#ifdef CONFIG_SET_TIMER
#define TIMER_TICK 4
#define TIMER_COUNT (TIMER_TICK * TIMER_CLK / 3000)
#else
#define TIMER_COUNT TIMER_NORMAL
#define TIMER_TICK (TIMER_COUNT * 3000 / TIMER_CLK)
#endif

// Implemented in timer.asm
void __interrupt timer_handler (void);
void timer_handler_end (void);

struct timer
{
  Uint32 ticks;
  Uint32 off;
  Uint32 len;
  Uint32 count;
  Uint32 normal;
  Uint32 pad;
  void (__interrupt __far *oldhandler)(void);
};

volatile struct timer timer =
 { 0, 0, TIMER_TICK, TIMER_COUNT, TIMER_NORMAL, 0, NULL };

void delay(Uint32 ms)
{
  ms += timer.ticks;
  while(timer.ticks < ms);
}

Uint32 get_ticks(void)
{
  return timer.ticks;
}

bool platform_init(void)
{
  if(!lock_region(&timer, sizeof(timer)))
    return false;
  if(!lock_region((void *)timer_handler, (char *)timer_handler_end -
   (char *)timer_handler))
    return false;

  // IRQ0 - system timer
  timer.oldhandler = _dos_getvect(0x08);
  _dos_setvect(0x08, (void far *)timer_handler);

#ifdef CONFIG_SET_TIMER
  _disable();
  // Counter 0, read lsb-msb
  outp(0x43, 0x34);
  outp(0x40, timer.count & 0xFF);
  outp(0x40, timer.count >> 8);
  _enable();
#endif
  delay(1000);
  return true;
}

void platform_quit(void)
{
#ifdef CONFIG_SET_TIMER
  _disable();
  // Counter 0, read lsb-msb
  outp(0x43, 0x34);
  outp(0x40, timer.normal & 0xFF);
  outp(0x40, timer.normal >> 8);
  _enable();
#endif

  _dos_setvect(0x08, (void far *)timer.oldhandler);
}
