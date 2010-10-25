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
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
* General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/
// TODO: Implement everything!

#include <bios.h>
#include <dpmi.h>
#include <sys/segments.h>
#include "event.h"
#include "graphics.h"

extern struct input_status input;

// Defined in interrupt.S
extern int kbd_handler;
extern __dpmi_paddr kbd_old_handler;
extern volatile Uint8 kbd_buffer[256];
extern volatile Uint8 kbd_read;
extern volatile Uint8 kbd_write;

static enum
{
  KBD_RELEASED,
  KBD_PRESSING,
  KBD_PRESSED
} kbd_statmap[0x80] = {0};
static Uint16 kbd_unicode[0x80] = {0};
static bool kbd_init = false;

static int read_kbd(void)
{
  int ret;
  if(kbd_read == kbd_write)
    return -1;
  ret = kbd_buffer[kbd_read++];
  return ret;
}

static const enum keycode xt_to_internal[0x80] =
{
  // 0x
  IKEY_UNKNOWN, IKEY_ESCAPE, IKEY_1, IKEY_2,
  IKEY_3, IKEY_4, IKEY_5, IKEY_6,
  IKEY_7, IKEY_8, IKEY_9, IKEY_0,
  IKEY_MINUS, IKEY_EQUALS, IKEY_BACKSPACE, IKEY_TAB,
  // 1x
  IKEY_q, IKEY_w, IKEY_e, IKEY_r,
  IKEY_t, IKEY_y, IKEY_u, IKEY_i,
  IKEY_o, IKEY_p, IKEY_LEFTBRACKET, IKEY_RIGHTBRACKET,
  IKEY_RETURN, IKEY_RCTRL, IKEY_a, IKEY_s,
  // 2x
  IKEY_d, IKEY_f, IKEY_g, IKEY_h,
  IKEY_j, IKEY_k, IKEY_l, IKEY_SEMICOLON,
  IKEY_QUOTE, IKEY_BACKQUOTE, IKEY_LSHIFT, IKEY_BACKSLASH,
  IKEY_z, IKEY_x, IKEY_c, IKEY_v,
  // 3x
  IKEY_b, IKEY_n, IKEY_m, IKEY_COMMA,
  IKEY_PERIOD, IKEY_SLASH, IKEY_RSHIFT, IKEY_KP_MULTIPLY,
  IKEY_RALT, IKEY_SPACE, IKEY_CAPSLOCK, IKEY_F1,
  IKEY_F2, IKEY_F3, IKEY_F4, IKEY_F5,
  // 4x
  IKEY_F6, IKEY_F7, IKEY_F8, IKEY_F9,
  IKEY_F10, IKEY_NUMLOCK, IKEY_SCROLLOCK, IKEY_KP7,
  IKEY_KP8, IKEY_KP9, IKEY_KP_MINUS, IKEY_KP4,
  IKEY_KP5, IKEY_KP6, IKEY_KP_PLUS, IKEY_KP1,
  // 5x
  IKEY_KP2, IKEY_KP3, IKEY_KP0, IKEY_KP_PERIOD,
  IKEY_SYSREQ, IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_F11,
  IKEY_F12, IKEY_UNKNOWN
};

static const enum keycode extended_xt_to_internal[0x80] =
{
  // 0x
  IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN,
  IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN,
  IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN,
  IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN,
  // 1x
  IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN,
  IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN,
  IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN,
  IKEY_KP_ENTER, IKEY_LCTRL, IKEY_UNKNOWN, IKEY_UNKNOWN,
  // 2x
  IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN,
  IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN,
  IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN,
  IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN,
  // 3x
  IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN,
  IKEY_UNKNOWN, IKEY_KP_DIVIDE, IKEY_UNKNOWN, IKEY_SYSREQ,
  IKEY_LALT, IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN,
  IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN,
  // 4x
  IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN,
  IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_BREAK, IKEY_HOME,
  IKEY_UP, IKEY_PAGEUP, IKEY_UNKNOWN, IKEY_LEFT,
  IKEY_UNKNOWN, IKEY_RIGHT, IKEY_UNKNOWN, IKEY_END,
  // 5x
  IKEY_DOWN, IKEY_PAGEDOWN, IKEY_INSERT, IKEY_DELETE,
  IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN,
  IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_UNKNOWN, IKEY_LSUPER,
  IKEY_RSUPER, IKEY_MENU, IKEY_UNKNOWN
};

static enum keycode convert_ext_internal(Uint8 key)
{
  if(key & 0x80)
    return extended_xt_to_internal[key & 0x7F];
  else
    return xt_to_internal[key];
}

static void update_lock_status(struct buffered_status *status)
{
  unsigned short res;
  res = bioskey(2);
  status->numlock_status = !!(res & 0x20);
  status->caps_status = !!(res & 0x40);
}

static void poll_keyboard_bios(void)
{
  unsigned short res;
  Uint8 scancode;
  Uint16 unicode;

  while(bioskey(1))
  {
    res = bioskey(0);
    scancode = res >> 8 & 0x7F;
    unicode = res & 0xFF;
    kbd_unicode[scancode] = unicode;
    if(kbd_statmap[scancode] == KBD_RELEASED)
      kbd_statmap[scancode] = KBD_PRESSING;
  }
}

static Uint16 convert_ext_unicode(Uint8 key)
{
  poll_keyboard_bios();
  return kbd_unicode[key & 0x7F];
}

static int get_keystat(int key)
{
  return kbd_statmap[key & 0x7F];
}

static void set_keystat(int key, int stat)
{
  kbd_statmap[key & 0x7F] = stat;
}

static bool process_keypress(int key)
{
  struct buffered_status *status = store_status();
  enum keycode ikey = convert_ext_internal(key);
  Uint16 unicode = convert_ext_unicode(key);

  if(get_keystat(key) != KBD_PRESSING)
    return false;
  set_keystat(key, KBD_PRESSED);

  if(!ikey)
  {
    if(unicode)
      ikey = IKEY_UNICODE;
    else
      return false;
  }

  if(status->keymap[ikey])
    return false;

  if((ikey == IKEY_CAPSLOCK) || (ikey == IKEY_NUMLOCK))
    update_lock_status(status);

  if((ikey == IKEY_RETURN) &&
   get_alt_status(keycode_internal) &&
   get_ctrl_status(keycode_internal))
  {
    toggle_fullscreen();
    return true;
  }

  if(ikey == IKEY_F12)
  {
    dump_screen();
    return true;
  }

  if(status->key_repeat &&
   (status->key_repeat != IKEY_LSHIFT) &&
   (status->key_repeat != IKEY_RSHIFT) &&
   (status->key_repeat != IKEY_LALT) &&
   (status->key_repeat != IKEY_RALT) &&
   (status->key_repeat != IKEY_LCTRL) &&
   (status->key_repeat != IKEY_RCTRL))
  {
    // Stack current repeat key if it isn't shift, alt, or ctrl
    if(input.repeat_stack_pointer != KEY_REPEAT_STACK_SIZE)
    {
      input.key_repeat_stack[input.repeat_stack_pointer] =
       status->key_repeat;
      input.unicode_repeat_stack[input.repeat_stack_pointer] =
       status->unicode_repeat;
      input.repeat_stack_pointer++;
    }
  }

  key_press(status, ikey, unicode);
  return true;
}

static bool process_keyrelease(int key)
{
  struct buffered_status *status = store_status();
  enum keycode ikey = convert_ext_internal(key);

  if(get_keystat(key) != KBD_PRESSED)
    return false;
  set_keystat(key, KBD_RELEASED);

  if(!ikey)
  {
    if(status->keymap[IKEY_UNICODE])
      ikey = IKEY_UNICODE;
    else
      return false;
  }

  status->keymap[ikey] = 0;
  if(status->key_repeat == ikey)
  {
    status->key_repeat = IKEY_UNKNOWN;
    status->unicode_repeat = 0;
  }
  status->key_release = ikey;
  return true;
}

static bool process_key(int key)
{
  static int extended = 0;
  bool ret;

  if(key == 0xE0)
  {
    extended = 0x80;
    return false;
  }

  if(key & 0x80)
    ret = process_keyrelease((key & 0x7F) | extended);
  else
    ret = process_keypress(key | extended);
  extended = 0;

  return ret;
}

// Defined in interrupt.S
extern void mouse_handler(__dpmi_regs *reg);
extern _go32_dpmi_registers mouse_regs;
extern volatile struct mouse_event
{
  Uint16 cond;
  Uint16 button;
  Sint16 dx;
  Sint16 dy;
} mouse_buffer[256];
extern volatile Uint8 mouse_read;
extern volatile Uint8 mouse_write;

static struct mouse_event mouse_last = { 0, 0, 0, 0 };
static bool mouse_init = false;

static bool read_mouse(struct mouse_event *mev)
{
  if(mouse_read == mouse_write)
    return false;
  *mev = mouse_buffer[mouse_read++];
  return true;
}

static bool process_mouse(struct mouse_event *mev)
{
  struct buffered_status *status = store_status();
  bool rval = false;

  if(mev->cond & 0x01)
  {
    int mx = status->real_mouse_x + mev->dx - mouse_last.dx;
    int my = status->real_mouse_y + mev->dy - mouse_last.dy;

    if(mx < 0)
      mx = 0;
    if(my < 0)
      my = 0;
    if(mx >= 640)
      mx = 639;
    if(my >= 350)
      my = 349;

    status->real_mouse_x = mx;
    status->real_mouse_y = my;
    status->mouse_x = mx / 8;
    status->mouse_y = my / 14;
    status->mouse_moved = true;
    rval = true;

    mouse_last.dx = mev->dx;
    mouse_last.dy = mev->dy;
  }

  if(mev->cond & 0x7E)
  {
    const Uint32 buttons[] =
    {
      MOUSE_BUTTON_LEFT,
      MOUSE_BUTTON_RIGHT,
      MOUSE_BUTTON_MIDDLE
    };
    int changed = mev->button ^ mouse_last.button;
    int i, j;
    for(i = 0, j = 1; i < 3; i++, j <<= 1)
    {
      if(changed & j)
      {
        if(mev->button & j)
        {
          status->mouse_button = buttons[i];
          status->mouse_repeat = buttons[i];
          status->mouse_button_state |= MOUSE_BUTTON(buttons[i]);
          status->mouse_repeat_state = 1;
          status->mouse_drag_state = -1;
          status->mouse_time = get_ticks();
        }
        else
        {
          status->mouse_button_state &= ~MOUSE_BUTTON(buttons[i]);
          status->mouse_repeat = 0;
          status->mouse_drag_state = 0;
          status->mouse_repeat_state = 0;
        }
        rval = true;
      }
    }
    mouse_last.button = mev->button;
  }
  return rval;
}

bool __update_event_status(void)
{
  struct mouse_event mev;
  bool rval = false;
  int key;

  while((key = read_kbd()) != -1)
    rval |= process_key(key);
  while(read_mouse(&mev))
    rval |= process_mouse(&mev);

  return rval;
}

void __wait_event(void)
{
  struct mouse_event mev;
  bool ret;
  int key;

  if(!kbd_init && !mouse_init)
    return;

  while((key = read_kbd()) == -1 && !(ret = read_mouse(&mev)));
  if(key != -1)
    process_key(key);
  if(ret)
    process_mouse(&mev);
}

void real_warp_mouse(Uint32 x, Uint32 y)
{
}

static void init_kbd(void)
{
  __dpmi_paddr handler;
  __dpmi_get_protected_mode_interrupt_vector(0x09, &kbd_old_handler);
  handler.offset32 = (unsigned long)&kbd_handler;
  handler.selector = _my_cs();
  if(!__dpmi_set_protected_mode_interrupt_vector(0x09, &handler))
    kbd_init = true;
}

static void init_mouse(void)
{
  __dpmi_regs reg;
  _go32_dpmi_seginfo cb;

  reg.x.ax = 0;
  __dpmi_int(0x33, &reg);
  if(reg.x.ax != 0xFFFF)
    return;

  // TODO: Free this callback on quit
  cb.pm_offset = (unsigned long)mouse_handler;
  if(_go32_dpmi_allocate_real_mode_callback_retf(&cb, &mouse_regs))
    return;
  reg.x.ax = 0x000C;
  reg.x.cx = 0x007F;
  reg.x.dx = cb.rm_offset;
  reg.x.es = cb.rm_segment;
  __dpmi_int(0x33, &reg);

  mouse_init = true;
}

void initialize_joysticks(void)
{
  init_kbd();
  init_mouse();
}
