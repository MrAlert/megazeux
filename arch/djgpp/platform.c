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

#include <pc.h>
#include <dpmi.h>
#include "platform.h"
#include "platform_djgpp.h"

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
  __dpmi_regs reg;

  // Try calling VGA Identity Adapter function
  reg.x.ax = 0x1A00;
  __dpmi_int(0x10, &reg);

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
    reg.x.bx = 0x0010;
    __dpmi_int(0x10, &reg);
    // Is EGA there?
    if(reg.x.bx != 0x0010)
    {
      // Since we have EGA BIOS, get details
      reg.h.ah = 0x12;
      reg.h.bl = 0x10;
      __dpmi_int(0x10, &reg);
      // Do we have color EGA?
      if(!reg.h.bh)
        return DISPLAY_ADAPTER_EGA_COLOR;
      else
        return DISPLAY_ADAPTER_EGA_MONO;
    }
    else
    {
      // Let's try equipment determination service
      __dpmi_int(0x11, &reg);
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
