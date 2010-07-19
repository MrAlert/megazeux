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
// TODO: Implement mouse

#include <i86.h>
#include <dos.h>
#include <bios.h>
#include "event.h"
#include "graphics.h"
#include "platform.h"
#include "platform_dos.h"

extern struct input_status input;

static int keyboard_extended = 0;
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

static const enum keycode extended_xt_to_internal[0x80] = {
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

static enum keycode convert_xt_internal(Uint8 key)
{
  if(keyboard_extended)
    return extended_xt_to_internal[key];
  else
    return xt_to_internal[key];
}

static int keyboard_init = 0;

volatile Uint8 keyboard_buffer[256];
volatile Uint8 keyboard_read = 0;
volatile Uint8 keyboard_write = 0;

static int read_buffer(void)
{
  int ret;
  if(keyboard_read == keyboard_write)
    return -1;
  ret = keyboard_buffer[keyboard_read++];
  return ret;
}

static Uint32 convert_xt_unicode(Uint8 key)
{
  static Uint16 xt_to_unicode[0x100] = {0};
  unsigned short res;

  while(_bios_keybrd(_KEYBRD_READY))
  {
    res = _bios_keybrd(_KEYBRD_READ);
    xt_to_unicode[res >> 8] = res & 0xFF;
  }

  return xt_to_unicode[key];
}

static void update_lock_status(struct buffered_status *status)
{
  unsigned short res;
  res = _bios_keybrd(_KEYBRD_SHIFTSTATUS);
  status->numlock_status = !!(res & 0x20);
  status->caps_status = !!(res & 0x40);
}

static bool process_keypress(int key)
{
  struct buffered_status *status = store_status();
  enum keycode ikey = convert_xt_internal(key);

  if(!ikey)
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

  key_press(status, ikey, convert_xt_unicode(key));
  return true;
}

static bool process_keyrelease(int key)
{
  struct buffered_status *status = store_status();
  enum keycode ikey = convert_xt_internal(key);

  if(!ikey)
    return false;

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
  bool ret;

  if(key == 0xE0)
  {
    keyboard_extended = 1;
    return false;
  }

  if(key & 0x80)
    ret = process_keyrelease(key & 0x7F);
  else
    ret = process_keypress(key);

  keyboard_extended = 0;

  return ret;
}

bool __update_event_status(void)
{
  bool rval = false;
  int key;

  while((key = read_buffer()) != -1)
    rval |= process_key(key);

  return rval;
}

void __wait_event(void)
{
  int key;

  if(!keyboard_init)
    return;

  while((key = read_buffer()) == -1);
  process_key(key);
}

void real_warp_mouse(Uint32 x, Uint32 y)
{
}

// Implemented in keyboard.asm
void __interrupt keyboard_handler (void);
void keyboard_handler_end (void);
volatile void (__interrupt __far *keyboard_oldhandler)(void);

void initialize_joysticks(void)
{
  if(!lock_region(&keyboard_buffer, sizeof(keyboard_buffer)))
    return;
  if(!lock_region(&keyboard_read, sizeof(keyboard_read)))
    return;
  if(!lock_region(&keyboard_write, sizeof(keyboard_write)))
    return;
  if(!lock_region(&keyboard_handler, (char *)keyboard_handler_end -
   (char *)keyboard_handler))
    return;

  // TODO: DOS/4GW probably resets the vectors on exit,
  // (protected mode vectors don't translate too well to real mode)
  // but we probably still want to do something about this.
  keyboard_oldhandler = _dos_getvect(0x09);
  _dos_setvect(0x09, keyboard_handler);
  delay(1000);
  keyboard_init = 1;
}
