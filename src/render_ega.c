/* MegaZeux
 *
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

#include <string.h>
#include <i86.h>
#include <conio.h>
#include "graphics.h"
#include "render.h"
#include "renderers.h"
#include "platform_dos.h"
#include "util.h"

static Uint8 *ega_fb[2] = { (void *)0x000A0000, (void *)0x000A8000 };

struct ega_render_data
{
  unsigned char page;
  unsigned char smzx;
  unsigned char vga;
  unsigned char oldmode;
};

static int ega_get_mem_size(void)
{
  union REGS reg;

  reg.h.ah = 0x12;
  reg.h.bl = 0x10;
  int386(0x10, &reg, &reg);
  return (reg.h.bl + 1) * 64;
}

static unsigned char ega_get_mode(void)
{
  union REGS reg;

  reg.h.ah = 0x0F;
  int386(0x10, &reg, &reg);
  return reg.h.al & 0x7F;
}

static void ega_set_mode(unsigned char mode)
{
  union REGS reg;
  reg.w.ax = mode;
  int386(0x10, &reg, &reg);
}

static void ega_vsync(void)
{
  while (inp(0x03DA) & 0x08);
  while (!(inp(0x03DA) & 0x08));
}

static bool ega_init_video(struct graphics_data *graphics,
 struct config_info *conf)
{
  struct ega_render_data *render_data = (void *)&graphics->render_data;
  int display;
  int ega_mem;

  display = detect_graphics();
  if(display < DISPLAY_ADAPTER_EGA_MONO || display >= DISPLAY_ADAPTER_MCGA_MONO)
  {
    warn("Insufficient display adapter detected: %s\n", disp_names[display]);
    return false;
  }

  ega_mem = ega_get_mem_size();
  if(ega_mem < 256)
  {
    warn("Insufficient display adapter memory: %dk\n", ega_mem);
    return false;
  }

  graphics->resolution_width = 640;
  graphics->resolution_height = 350;
  graphics->window_width = 640;
  graphics->window_height = 350;

  render_data->vga = (display >= DISPLAY_ADAPTER_VGA_MONO);
  render_data->oldmode = ega_get_mode();

  return set_video_mode();
}

static void ega_free_video(struct graphics_data *graphics)
{
  struct ega_render_data *render_data = (void *)&graphics->render_data;
  ega_set_mode(render_data->oldmode);
}

static bool ega_check_video_mode(struct graphics_data *graphics,
 int width, int height, int depth, bool fullscreen, bool resize)
{
  return true;
}

static bool ega_set_video_mode(struct graphics_data *graphics,
 int width, int height, int depth, bool fullscreen, bool resize)
{
  struct ega_render_data *render_data = (void *)&graphics->render_data;
  int i;

  render_data->page = 0;
  render_data->smzx = 0;

  // EGA hi-res color mode
  ega_set_mode(0x10);

  // If VGA, set the EGA palette to point to first 16 VGA palette entries
  if(render_data->vga)
  {
    ega_vsync();
    for (i = 0; i < 16; i++)
    {
      outp(0x03C0, i);
      outp(0x03C0, i);
    }
    // Get attribute controller back to normal
    outp(0x03C0, 0x20);
  }

  return true;
}

static void ega_update_colors(struct graphics_data *graphics,
 struct rgb_color *palette, Uint32 count)
{
  struct ega_render_data *render_data = (void *)&graphics->render_data;
  int i, j, c, step;

  if(render_data->vga)
  {
    for(i = 0; i < count; i++)
    {
      outp(0x03C8, i);
      outp(0x03C9, palette[i].r >> 2);
      outp(0x03C9, palette[i].g >> 2);
      outp(0x03C9, palette[i].b >> 2);
    }
  }
  else
  {
    if(count > 64)
      step = 17;
    else
      step = 1;
    // Reset index/data flip-flop
    inp(0x03DA);
    for(i = j = 0; i < 16 && j < count; i++, j += step)
    {
      c = (palette[i].b >> 7) & 0x01;
      c |= (palette[i].g >> 6) & 0x02;
      c |= (palette[i].r >> 5) & 0x04;
      c |= (palette[i].b >> 3) & 0x08;
      c |= (palette[i].g >> 2) & 0x10;
      c |= (palette[i].r >> 1) & 0x20;
      outp(0x03C0, i);
      outp(0x03C0, c);
    }
    // Get attribute controller back to normal
    outp(0x03C0, 0x20);
  }
}

static void ega_set_plane(int plane)
{
  // Set plane mask for writing
  outp(0x03C4, 0x02);
  outp(0x03C5, 1 << plane);
  // Disable set/reset for all planes
  outp(0x03CE, 0x01);
  outp(0x03CF, 0x00);
  // Disable logical operation and rotation
  outp(0x03CE, 0x03);
  outp(0x03CF, 0x00);
  // Select plane for reading
  outp(0x03CE, 0x04);
  outp(0x03CF, plane);
  // Set bit mask register
  outp(0x03CE, 0x08);
  outp(0x03CF, 0xFF);
}

static void ega_render_graph4(struct graphics_data *graphics, Uint8 *fb)
{
  struct char_element *src;
  Uint8 *dest, *ldest, *ldest2;
  Uint8 *char_ptr;
  int plane, x, y, i;
  for (plane = 0; plane < 4; plane++)
  {
    src = graphics->text_video;
    dest = fb;
    ega_set_plane(plane);
    for (y = 0; y < 25; y++)
    {
      ldest2 = dest;
      for(x = 0; x < 80; x++)
      {
        ldest = dest;
        char_ptr = graphics->charset + (src->char_value * 14);
        if(src->bg_color & (1 << plane))
        {
          if(src->fg_color & (1 << plane))
          {
            for(i = 0; i < 14; i++)
            {
              *dest = 0xFF;
              dest += 80;
            }
          }
          else
          {
            for(i = 0; i < 14; i++)
            {
              *dest = ~*char_ptr;
              dest += 80;
              char_ptr++;
            }
          }
        }
        else
        {
          if(src->fg_color & (1 << plane))
          {
            for(i = 0; i < 14; i++)
            {
              *dest = *char_ptr;
              dest += 80;
              char_ptr++;
            }
          }
          else
          {
            for(i = 0; i < 14; i++)
            {
              *dest = 0;
              dest += 80;
            }
          }
        }
        src++;
        dest = ldest + 1;
      }
      dest = ldest2 + 80 * 14;
    }
  }
}

static void ega_render_graph(struct graphics_data *graphics)
{
  struct ega_render_data *render_data = (void *)&graphics->render_data;

  if(render_data->vga && graphics->screen_mode != render_data->smzx)
  {
    // FIXME: Implement 256-color mode
    render_data->smzx = graphics->screen_mode;
  }
  ega_render_graph4(graphics, ega_fb[render_data->page]);
}

static void ega_render_cursor(struct graphics_data *graphics,
 Uint32 x, Uint32 y, Uint8 color, Uint8 lines, Uint8 offset)
{
  // FIXME: Implement cursor
}

static void ega_render_mouse(struct graphics_data *graphics,
 Uint32 x, Uint32 y, Uint8 w, Uint8 h)
{
  // FIXME: Implement mouse
}

static void ega_sync_screen(struct graphics_data *graphics)
{
  ega_vsync();
  // FIXME: Implement page flipping
}

void render_ega_register(struct renderer *renderer)
{
  memset(renderer, 0, sizeof(struct renderer));
  renderer->init_video = ega_init_video;
  renderer->free_video = ega_free_video;
  renderer->check_video_mode = ega_check_video_mode;
  renderer->set_video_mode = ega_set_video_mode;
  renderer->update_colors = ega_update_colors;
  renderer->resize_screen = resize_screen_standard;
  renderer->get_screen_coords = get_screen_coords_centered;
  renderer->set_screen_coords = set_screen_coords_centered;
  renderer->render_graph = ega_render_graph;
  renderer->render_cursor = ega_render_cursor;
  renderer->render_mouse = ega_render_mouse;
  renderer->sync_screen = ega_sync_screen;
}
