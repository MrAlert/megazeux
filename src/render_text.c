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

static Uint8 *const text_vb[4] =
{
  (void *)0x000B8000,
  (void *)0x000B9000,
  (void *)0x000BA000,
  (void *)0x000BB000
};
static const Uint16 text_addr[4] =
{
  0x0000,
  0x1000,
  0x2000,
  0x3000
};

struct text_render_data
{
  unsigned char page;
  unsigned char smzx;
  unsigned char vga;
  unsigned char oldmode;
};

static void text_set_14p(void)
{
  union REGS reg;
  reg.w.ax = 0x1201;
  reg.h.bl = 0x30;
  int386(0x10, &reg, &reg);
}

static void text_set_16p(void)
{
  union REGS reg;
  reg.w.ax = 0x1202;
  reg.h.bl = 0x30;
  int386(0x10, &reg, &reg);
}

static void text_blink_on(void)
{
  union REGS reg;
  reg.w.ax = 0x1003;
  reg.h.bl = 0x01;
  int386(0x10, &reg, &reg);
}

static void text_blink_off(void)
{
  union REGS reg;
  reg.w.ax = 0x1003;
  reg.h.bl = 0x00;
  int386(0x10, &reg, &reg);
}

static unsigned char text_get_mode(void)
{
  union REGS reg;

  reg.h.ah = 0x0F;
  int386(0x10, &reg, &reg);
  return reg.h.al & 0x7F;
}

static void text_set_mode(unsigned char mode)
{
  union REGS reg;
  reg.w.ax = mode;
  int386(0x10, &reg, &reg);
}

static void text_vsync(void)
{
  while (inp(0x3DA) & 0x08);
  while (!inp(0x03DA) & 0x08);
}

static bool text_init_video(struct graphics_data *graphics,
 struct config_info *conf)
{
  struct text_render_data *render_data = (void *)&graphics->render_data;
  int display;

  display = detect_graphics();
  if(display < DISPLAY_ADAPTER_EGA_MONO)
  {
    warn("Insufficient display adapter detected: %s\n", disp_names[display]);
    return false;
  }

  graphics->resolution_width = 640;
  graphics->resolution_height = 350;
  graphics->window_width = 640;
  graphics->window_height = 350;

  render_data->vga = (display >= DISPLAY_ADAPTER_VGA_MONO);
  render_data->oldmode = text_get_mode();

  return set_video_mode();
}

static void text_free_video(struct graphics_data *graphics)
{
  struct text_render_data *render_data = (void *)&graphics->render_data;
  if(render_data->vga)
    text_set_16p();
  text_set_mode(render_data->oldmode);
  text_blink_on();
}

static bool text_check_video_mode(struct graphics_data *graphics,
 int width, int height, int depth, bool fullscreen, bool resize)
{
  return true;
}

static bool text_set_video_mode(struct graphics_data *graphics,
 int width, int height, int depth, bool fullscreen, bool resize)
{
  struct text_render_data *render_data = (void *)&graphics->render_data;
  int i;

  render_data->page = 0;
  render_data->smzx = 0;

  if(render_data->vga)
    text_set_14p();
  // 80x25 color text mode
  text_set_mode(0x03);
  text_blink_off();

  // If VGA, set the EGA palette to point to first 16 VGA palette entries
  if(render_data->vga)
  {
    text_vsync();
    for(i = 0; i < 16; i++)
    {
      outp(0x03C0, i);
      outp(0x03C0, i);
    }
    // Get attribute controller back to normal
    outp(0x03C0, 0x20);
  }

  return true;
}

static void text_update_colors(struct graphics_data *graphics,
 struct rgb_color *palette, Uint32 count)
{
  struct text_render_data *render_data = (void *)&graphics->render_data;
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
      c = (palette[j].b >> 7) & 0x01;
      c |= (palette[j].g >> 6) & 0x02;
      c |= (palette[j].r >> 5) & 0x04;
      c |= (palette[j].b >> 3) & 0x08;
      c |= (palette[j].g >> 2) & 0x10;
      c |= (palette[j].r >> 1) & 0x20;
      outp(0x03C0, i);
      outp(0x03C0, c);
    }
    // Get attribute controller back to normal
    outp(0x03C0, 0x20);
  }
}

static void text_render_graph(struct graphics_data *graphics)
{
  struct text_render_data *render_data = (void *)&graphics->render_data;
  struct char_element *src = graphics->text_video;
  Uint8 *dest = text_vb[render_data->page];
  int i;
  for(i = 0; i < (80 * 25); i++, src++, dest += 2)
  {
    dest[1] = (src->bg_color << 4) | (src->fg_color & 0x0F);
    dest[0] = src->char_value;
  }
}

static void text_render_cursor(struct graphics_data *graphics,
 Uint32 x, Uint32 y, Uint8 color, Uint8 lines, Uint8 offset)
{
  // FIXME: Add cursor
}

static void text_render_mouse(struct graphics_data *graphics,
 Uint32 x, Uint32 y, Uint8 w, Uint8 h)
{
  struct text_render_data *render_data = (void *)&graphics->render_data;
  Uint8 *dest = text_vb[render_data->page];
  dest += x / 8;
  dest += y / 14 * 80;
  *dest ^= 0xFF;
}

static void text_sync_screen(struct graphics_data *graphics)
{
  // FIXME: Page flip!
}

void render_text_register(struct renderer *renderer)
{
  memset(renderer, 0, sizeof(struct renderer));
  renderer->init_video = text_init_video;
  renderer->free_video = text_free_video;
  renderer->check_video_mode = text_check_video_mode;
  renderer->set_video_mode = text_set_video_mode;
  renderer->update_colors = text_update_colors;
  renderer->resize_screen = resize_screen_standard;
  renderer->get_screen_coords = get_screen_coords_centered;
  renderer->set_screen_coords = set_screen_coords_centered;
  renderer->render_graph = text_render_graph;
  renderer->render_cursor = text_render_cursor;
  renderer->render_mouse = text_render_mouse;
  renderer->sync_screen = text_sync_screen;
}
