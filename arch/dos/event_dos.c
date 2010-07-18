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
// TODO: Implement XT-to-keycode conversion better
// TODO: Try to take advantage of the BIOS or something for ASCII codes rather
// than trying to figure it out on our own

#include "event.h"
#include "graphics.h"
#include "platform_dos.h"

extern struct input_status input;

static enum keycode convert_xt_internal(Uint32 key)
{
  switch(key)
  {
    case 0x01: return IKEY_ESCAPE;
    case 0x3B: return IKEY_F1;
    case 0x3C: return IKEY_F2;
    case 0x3D: return IKEY_F3;
    case 0x3E: return IKEY_F4;
    case 0x3F: return IKEY_F5;
    case 0x40: return IKEY_F6;
    case 0x41: return IKEY_F7;
    case 0x42: return IKEY_F8;
    case 0x43: return IKEY_F9;
    case 0x44: return IKEY_F10;
    case 0x57: return IKEY_F11;
    case 0x58: return IKEY_F12;
    case 0x29: return IKEY_BACKQUOTE;
    case 0x02: return IKEY_1;
    case 0x03: return IKEY_2;
    case 0x04: return IKEY_3;
    case 0x05: return IKEY_4;
    case 0x06: return IKEY_5;
    case 0x07: return IKEY_6;
    case 0x08: return IKEY_7;
    case 0x09: return IKEY_8;
    case 0x0A: return IKEY_9;
    case 0x0B: return IKEY_0;
    case 0x0C: return IKEY_MINUS;
    case 0x0D: return IKEY_EQUALS;
    case 0x2B: return IKEY_BACKSLASH;
    case 0x0E: return IKEY_BACKSPACE;
    case 0x0F: return IKEY_TAB;
    case 0x10: return IKEY_q;
    case 0x11: return IKEY_w;
    case 0x12: return IKEY_e;
    case 0x13: return IKEY_r;
    case 0x14: return IKEY_t;
    case 0x15: return IKEY_y;
    case 0x16: return IKEY_u;
    case 0x17: return IKEY_i;
    case 0x18: return IKEY_o;
    case 0x19: return IKEY_p;
    case 0x1A: return IKEY_LEFTBRACKET;
    case 0x1B: return IKEY_RIGHTBRACKET;
    case 0x3A: return IKEY_CAPSLOCK;
    case 0x1E: return IKEY_a;
    case 0x1F: return IKEY_s;
    case 0x20: return IKEY_d;
    case 0x21: return IKEY_f;
    case 0x22: return IKEY_g;
    case 0x23: return IKEY_h;
    case 0x24: return IKEY_j;
    case 0x25: return IKEY_k;
    case 0x26: return IKEY_l;
    case 0x27: return IKEY_SEMICOLON;
    case 0x28: return IKEY_QUOTE;
    case 0x1C: return IKEY_RETURN;
    case 0x2A: return IKEY_LSHIFT;
    case 0x2C: return IKEY_z;
    case 0x2D: return IKEY_x;
    case 0x2E: return IKEY_c;
    case 0x2F: return IKEY_v;
    case 0x30: return IKEY_b;
    case 0x31: return IKEY_n;
    case 0x32: return IKEY_m;
    case 0x33: return IKEY_COMMA;
    case 0x34: return IKEY_PERIOD;
    case 0x35: return IKEY_SLASH;
    case 0x36: return IKEY_RSHIFT;
    case 0x1D: return IKEY_LCTRL;
    case 0x5B: return IKEY_LSUPER;
    case 0x38: return IKEY_LALT;
    case 0x39: return IKEY_SPACE;
    case 0x5C: return IKEY_RSUPER;
    case 0x5D: return IKEY_MENU;
    case 0x37: return IKEY_SYSREQ;
    case 0x46: return IKEY_SCROLLOCK;
    case 0xC5: return IKEY_BREAK;
    case 0x52: return IKEY_INSERT;
    case 0x47: return IKEY_HOME;
    case 0x49: return IKEY_PAGEUP;
    case 0x53: return IKEY_DELETE;
    case 0x4F: return IKEY_END;
    case 0x51: return IKEY_PAGEDOWN;
    case 0x45: return IKEY_NUMLOCK;
    case 0x4A: return IKEY_KP_MINUS;
    case 0x48: return IKEY_UP;
    case 0x4B: return IKEY_LEFT;
    case 0x4C: return IKEY_KP5;
    case 0x4D: return IKEY_RIGHT;
    case 0x4E: return IKEY_KP_PLUS;
    case 0x50: return IKEY_DOWN;
    default: return IKEY_UNKNOWN;
  }
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

#define UNICODE_SHIFT(A, B) \
  ((status->keymap[IKEY_LSHIFT] || status->keymap[IKEY_RSHIFT]) ? B : A)
#define UNICODE_CAPS(A, B) \
  (status->caps_status ? UNICODE_SHIFT(B, A) : UNICODE_SHIFT(A, B))
#define UNICODE_NUM(A, B) \
  (status->numlock_status ? B : A)

// TODO: Support non-US keyboard layouts
static Uint32 convert_internal_unicode(struct buffered_status *status,
 enum keycode key)
{
  switch(key)
  {
    case IKEY_SPACE: return ' ';
    case IKEY_QUOTE: return UNICODE_SHIFT('\'', '"');
    case IKEY_COMMA: return UNICODE_SHIFT(',', '<');
    case IKEY_MINUS: return UNICODE_SHIFT('-', '_');
    case IKEY_PERIOD: return UNICODE_SHIFT('.', '>');
    case IKEY_SLASH: return UNICODE_SHIFT('/', '?');
    case IKEY_0: return UNICODE_SHIFT('0', ')');
    case IKEY_1: return UNICODE_SHIFT('1', '!');
    case IKEY_2: return UNICODE_SHIFT('2', '@');
    case IKEY_3: return UNICODE_SHIFT('3', '#');
    case IKEY_4: return UNICODE_SHIFT('4', '$');
    case IKEY_5: return UNICODE_SHIFT('5', '%');
    case IKEY_6: return UNICODE_SHIFT('6', '^');
    case IKEY_7: return UNICODE_SHIFT('7', '&');
    case IKEY_8: return UNICODE_SHIFT('8', '*');
    case IKEY_9: return UNICODE_SHIFT('9', '(');
    case IKEY_SEMICOLON: return UNICODE_SHIFT(';', ':');
    case IKEY_EQUALS: return UNICODE_SHIFT('=', '+');
    case IKEY_LEFTBRACKET: return UNICODE_SHIFT('[', '{');
    case IKEY_BACKSLASH: return UNICODE_SHIFT('\\', '|');
    case IKEY_RIGHTBRACKET: return UNICODE_SHIFT(']', '}');
    case IKEY_BACKQUOTE: return UNICODE_SHIFT('`', '~');
    case IKEY_a: return UNICODE_CAPS('a', 'A');
    case IKEY_b: return UNICODE_CAPS('b', 'B');
    case IKEY_c: return UNICODE_CAPS('c', 'C');
    case IKEY_d: return UNICODE_CAPS('d', 'D');
    case IKEY_e: return UNICODE_CAPS('e', 'E');
    case IKEY_f: return UNICODE_CAPS('f', 'F');
    case IKEY_g: return UNICODE_CAPS('g', 'G');
    case IKEY_h: return UNICODE_CAPS('h', 'H');
    case IKEY_i: return UNICODE_CAPS('i', 'I');
    case IKEY_j: return UNICODE_CAPS('j', 'J');
    case IKEY_k: return UNICODE_CAPS('k', 'K');
    case IKEY_l: return UNICODE_CAPS('l', 'L');
    case IKEY_m: return UNICODE_CAPS('m', 'M');
    case IKEY_n: return UNICODE_CAPS('n', 'N');
    case IKEY_o: return UNICODE_CAPS('o', 'O');
    case IKEY_p: return UNICODE_CAPS('p', 'P');
    case IKEY_q: return UNICODE_CAPS('q', 'Q');
    case IKEY_r: return UNICODE_CAPS('r', 'R');
    case IKEY_s: return UNICODE_CAPS('s', 'S');
    case IKEY_t: return UNICODE_CAPS('t', 'T');
    case IKEY_u: return UNICODE_CAPS('u', 'U');
    case IKEY_v: return UNICODE_CAPS('v', 'V');
    case IKEY_w: return UNICODE_CAPS('w', 'W');
    case IKEY_x: return UNICODE_CAPS('x', 'X');
    case IKEY_y: return UNICODE_CAPS('y', 'Y');
    case IKEY_z: return UNICODE_CAPS('z', 'Z');
    case IKEY_KP0: return UNICODE_NUM(0, '0');
    case IKEY_KP1: return UNICODE_NUM(0, '1');
    case IKEY_KP2: return UNICODE_NUM(0, '2');
    case IKEY_KP3: return UNICODE_NUM(0, '3');
    case IKEY_KP4: return UNICODE_NUM(0, '4');
    case IKEY_KP5: return UNICODE_NUM(0, '5');
    case IKEY_KP6: return UNICODE_NUM(0, '6');
    case IKEY_KP7: return UNICODE_NUM(0, '7');
    case IKEY_KP8: return UNICODE_NUM(0, '8');
    case IKEY_KP9: return UNICODE_NUM(0, '9');
    case IKEY_KP_PERIOD: return UNICODE_NUM(0, '.');
    case IKEY_KP_DIVIDE: return '/';
    case IKEY_KP_MULTIPLY: return '*';
    case IKEY_KP_MINUS: return '-';
    case IKEY_KP_PLUS: return '+';
    default: return 0;
  }
}

static bool process_key(int key)
{
  struct buffered_status *status = store_status();
  enum keycode ikey = convert_xt_internal(key & 0x7F);

  if(!ikey)
    return false;

  if(key & 0x80)
  {
    // Key release
    status->keymap[ikey] = 0;
    if(status->key_repeat == ikey)
    {
      status->key_repeat = IKEY_UNKNOWN;
      status->unicode_repeat = 0;
    }
    status->key_release = ikey;
    return true;
  }
  else
  {
    // Key press
    if(ikey == IKEY_NUMLOCK)
      status->numlock_status ^= 1;
    if(ikey == IKEY_CAPSLOCK)
      status->caps_status ^= 1;

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

    key_press(status, ikey, convert_internal_unicode(status, ikey));
    return true;
  }
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
  set_int_vector(0x09, keyboard_handler);
  keyboard_init = 1;
}
