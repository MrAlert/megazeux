/* $Id$
 * MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
 * Copyright (C) 1998 Matthew D. Williams - dbwilli@scsn.net
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "robot.h"
#include "const.h"
#include "board.h"
#include "game.h"
#include "game2.h"
#include "idarray.h"
#include "graphics.h"
#include "event.h"
#include "expr.h"
#include "window.h"
#include "scrdisp.h"

Robot *load_robot_allocate(FILE *fp, int savegame)
{
  Robot *cur_robot = (Robot *)malloc(sizeof(Robot));
  load_robot(cur_robot, fp, savegame);

  return cur_robot;
}

// Most of this stuff does not have to be loaded unless savegame
// is set.
void load_robot(Robot *cur_robot, FILE *fp, int savegame)
{
  int program_size = fgetw(fp);
  int i;

  cur_robot->program_length = program_size;
  // Skip junk
  fseek(fp, 2, SEEK_CUR);
  fread(cur_robot->robot_name, 15, 1, fp);
  cur_robot->robot_char = fgetc(fp);
  cur_robot->cur_prog_line = fgetw(fp);
  cur_robot->pos_within_line = fgetc(fp);
  cur_robot->robot_cycle = fgetc(fp);
  cur_robot->cycle_count = fgetc(fp);
  cur_robot->bullet_type = fgetc(fp);
  cur_robot->is_locked = fgetc(fp);
  cur_robot->can_lavawalk = fgetc(fp);
  cur_robot->walk_dir = fgetc(fp);
  cur_robot->last_touch_dir = fgetc(fp);
  cur_robot->last_shot_dir = fgetc(fp);
  cur_robot->xpos = fgetw(fp);
  cur_robot->ypos = fgetw(fp);
  cur_robot->status = fgetc(fp);
  // Skip local - these are in the save files now
  fseek(fp, 2, SEEK_CUR);
  cur_robot->used = fgetc(fp);
  cur_robot->loop_count = fgetw(fp);

  // If savegame, there's some additional information to get
  if(savegame)
  {
    int stack_size;

    // Get the local counters
    for(i = 0; i < 32; i++)
    {
      cur_robot->local[i] = fgetd(fp);
    }
    // Get the stack size
    stack_size = fgetd(fp);

    // Get the stack pointer
    cur_robot->stack_pointer = fgetd(fp);
    // Allocate the stack
    cur_robot->stack = (int *)malloc(stack_size * sizeof(int));
    for(i = 0; i < stack_size; i++)
    {
      cur_robot->stack[i] = fgetd(fp);
    }
    cur_robot->stack_size = stack_size;
  }
  else
  {
    // Otherwise, allocate some stuff; local counters are 0
    memset(cur_robot->local, 0, sizeof(int) * 32);
    // Start with a minimum stack size
    cur_robot->stack_size = ROBOT_START_STACK;
    cur_robot->stack = (int *)malloc(ROBOT_START_STACK * sizeof(int));
    // Initialize the stack pointer to the bottom
    cur_robot->stack_pointer = 0;
  }

  cur_robot->program = (char *)malloc(program_size);
  fread(cur_robot->program, program_size, 1, fp);


  // Now create a label cache IF the robot is in use
  if(cur_robot->used)
    cur_robot->label_list = cache_robot_labels(cur_robot, &(cur_robot->num_labels));
}

void robot_stack_push(Robot *cur_robot, int value)
{
  int stack_pointer = cur_robot->stack_pointer;
  int stack_size = cur_robot->stack_size;
  int *stack = cur_robot->stack;

  if((stack_pointer + 1) == stack_size)
  {
    // Double the stack. Don't let it get too large though!
    stack_size *= 2;
    if(stack_size > ROBOT_MAX_STACK)
      return;
    cur_robot->stack = (int *)realloc(stack, stack_size * sizeof(int));
    stack = cur_robot->stack;
    cur_robot->stack_size = stack_size;
  }

  stack[stack_pointer] = value;
  cur_robot->stack_pointer = stack_pointer + 1;
}

int robot_stack_pop(Robot *cur_robot)
{
  int stack_pointer = cur_robot->stack_pointer;

  if(stack_pointer)
  {
    stack_pointer--;
    cur_robot->stack_pointer = stack_pointer;
    return cur_robot->stack[stack_pointer];
  }
  else
  {
    return -1;
  }
}

Scroll *load_scroll_allocate(FILE *fp, int savegame)
{
  Scroll *cur_scroll = (Scroll *)malloc(sizeof(Scroll));
  load_scroll(cur_scroll, fp, savegame);

  return cur_scroll;
}

void load_scroll(Scroll *cur_scroll, FILE *fp, int savegame)
{
  int scroll_size;

  cur_scroll->num_lines = fgetw(fp);
  // Skip junk
  fseek(fp, 2, SEEK_CUR);
  scroll_size = fgetw(fp);
  cur_scroll->mesg_size = scroll_size;
  cur_scroll->used = fgetc(fp);

  cur_scroll->mesg = (char *)malloc(scroll_size);
  fread(cur_scroll->mesg, scroll_size, 1, fp);
}

Sensor *load_sensor_allocate(FILE *fp, int savegame)
{
  Sensor *cur_sensor = (Sensor *)malloc(sizeof(Sensor));
  load_sensor(cur_sensor, fp, savegame);

  return cur_sensor;
}

void load_sensor(Sensor *cur_sensor, FILE *fp, int savegame)
{
  fread(cur_sensor->sensor_name, 15, 1, fp);
  cur_sensor->sensor_char = fgetc(fp);
  fread(cur_sensor->robot_to_mesg, 15, 1, fp);
  cur_sensor->used = fgetc(fp);
}

void save_robot(Robot *cur_robot, FILE *fp, int savegame)
{
  int program_size = cur_robot->program_length;
  int i;

  fputw(program_size, fp);
  // This is junk, but put it anyway
  fputw(0, fp);
  fwrite(cur_robot->robot_name, 15, 1, fp);
  fputc(cur_robot->robot_char, fp);
  if(savegame)
  {
    fputw(cur_robot->cur_prog_line, fp);
    fputc(cur_robot->pos_within_line, fp);
    fputc(cur_robot->robot_cycle, fp);
    fputc(cur_robot->cycle_count, fp);
    fputc(cur_robot->bullet_type, fp);
    fputc(cur_robot->is_locked, fp);
    fputc(cur_robot->can_lavawalk, fp);
    fputc(cur_robot->walk_dir, fp);
    fputc(cur_robot->last_touch_dir, fp);
    fputc(cur_robot->last_shot_dir, fp);
    fputw(cur_robot->xpos, fp);
    fputw(cur_robot->ypos, fp);
    fputc(cur_robot->status, fp);
  }
  else
  {
    // Put some "default" values here instead
    fputw(1, fp);
    fputc(0, fp);
    fputc(0, fp);
    fputc(0, fp);
    fputc(1, fp);
    fputc(0, fp);
    fputc(0, fp);
    fputc(0, fp);
    fputc(0, fp);
    fputc(0, fp);
    fputw(cur_robot->xpos, fp);
    fputw(cur_robot->ypos, fp);
    fputc(0, fp);
  }

  // Junk local
  fputw(0, fp);
  fputc(cur_robot->used, fp);
  fputw(cur_robot->loop_count, fp);

  // If savegame, there's some additional information to get
  if(savegame)
  {
    int stack_size = cur_robot->stack_size;

    // Write the local counters
    for(i = 0; i < 32; i++)
    {
      fputd(cur_robot->local[i], fp);
    }

    // Put the stack size
    fputd(stack_size, fp);
    // Put the stack pointer
    fputd(cur_robot->stack_pointer, fp);
    // Put the stack

    for(i = 0; i < stack_size; i++)
    {
      fputd(cur_robot->stack[i], fp);
    }
  }

  // Write the program
  fwrite(cur_robot->program, program_size, 1, fp);
}

void save_scroll(Scroll *cur_scroll, FILE *fp, int savegame)
{
  int scroll_size = cur_scroll->mesg_size;

  fputw(cur_scroll->num_lines, fp);
  fputw(0, fp);
  fputw(scroll_size, fp);
  fputc(cur_scroll->used, fp);

  fwrite(cur_scroll->mesg, scroll_size, 1, fp);
}

void save_sensor(Sensor *cur_sensor, FILE *fp, int savegame)
{
  fwrite(cur_sensor->sensor_name, 15, 1, fp);
  fputc(cur_sensor->sensor_char, fp);
  fwrite(cur_sensor->robot_to_mesg, 15, 1, fp);
  fputc(cur_sensor->used, fp);
}

void clear_robot(Robot *cur_robot)
{
  if(cur_robot->used)
    clear_label_cache(cur_robot->label_list, cur_robot->num_labels);
  free(cur_robot->stack);
  free(cur_robot->program);
  free(cur_robot);
}

void clear_robot_contents(Robot *cur_robot)
{
  if(cur_robot->used)
    clear_label_cache(cur_robot->label_list, cur_robot->num_labels);
  free(cur_robot->stack);
  free(cur_robot->program);
}

void clear_scroll_contents(Scroll *cur_scroll)
{
  free(cur_scroll->mesg);
}

void clear_scroll(Scroll *cur_scroll)
{
  free(cur_scroll->mesg);
  free(cur_scroll);
}

void clear_robot_id(Board *src_board, int id)
{
  Robot *cur_robot = src_board->robot_list[id];

  if(id)
  {
    remove_robot_name_entry(src_board, cur_robot, cur_robot->robot_name);
    clear_robot(cur_robot);
    src_board->robot_list[id] = NULL;
  }
  else
  {
    clear_robot_contents(cur_robot);
    cur_robot->used = 0;
  }
}

void clear_scroll_id(Board *src_board, int id)
{
  clear_scroll(src_board->scroll_list[id]);
  src_board->scroll_list[id] = NULL;
}

void clear_sensor_id(Board *src_board, int id)
{
  clear_sensor(src_board->sensor_list[id]);
  src_board->sensor_list[id] = NULL;
}

void clear_sensor(Sensor *cur_sensor)
{
  free(cur_sensor);
}

void reallocate_robot(Robot *robot, int size)
{
  robot->program = (char *)realloc(robot->program, size);
  robot->program_length = size;
}

void reallocate_scroll(Scroll *scroll, int size)
{
  scroll->mesg = (char *)realloc(scroll->mesg, size);
  scroll->mesg_size = size;
}

int cmp_labels(const void *dest, const void *src)
{
  Label *lsrc = *((Label **)src);
  Label *ldest = *((Label **)dest);
  int cmp_primary = strcasecmp(ldest->name, lsrc->name);

  // A match needs to go on a secondary criteria.
  if(!cmp_primary)
  {
    return ldest->position - lsrc->position;
  }
  else
  {
    return cmp_primary;
  }
}

Label **cache_robot_labels(Robot *robot, int *num_labels)
{
  int labels_allocated = 16;
  int labels_found = 0;
  int cmd;
  int next;
  int i;
  char *robot_program = robot->program;
  Label **label_list = (Label **)malloc(sizeof(Label *) * 16);
  Label *current_label;

  for(i = 1; i < (robot->program_length - 1); i++)
  {
    // Is it a label?
    cmd = robot_program[i + 1];
    next = i + robot_program[i] + 1;

    if((cmd == 106) || (cmd == 108))
    {
      current_label = (Label *)malloc(sizeof(Label));
      current_label->name = robot_program + i + 3;
      current_label->position = next + 1;

      if(cmd == 108)
      {
        current_label->zapped = 1;
      }
      else
      {
        current_label->zapped = 0;
      }

      // Do we need more room?
      if(labels_found == labels_allocated)
      {
        labels_allocated *= 2;
        label_list = (Label **)realloc(label_list,
         sizeof(Label *) * labels_allocated);
      }
      label_list[labels_found] = current_label;
      labels_found++;
    }

    // Go to next command
    i = next;
  }

  if(!labels_found)
  {
    *num_labels = 0;
    free(label_list);
    return NULL;
  }

  if(labels_found != labels_allocated)
  {
    label_list =
     (Label **)realloc(label_list, sizeof(Label *) * labels_found);
  }

  // Now sort the list
  qsort(label_list, labels_found, sizeof(Label *), cmp_labels);

  *num_labels = labels_found;
  return label_list;
}

void clear_label_cache(Label **label_list, int num_labels)
{
  int i;

  if(label_list)
  {
    for(i = 0; i < num_labels; i++)
    {
      free(label_list[i]);
    }

    free(label_list);
  }
}

int find_label_position(Robot *cur_robot, char *name)
{
  Label *cur_label = find_label(cur_robot, name);

  if(cur_label)
  {
    return cur_label->position;
  }
  else
  {
    return -1;
  }
}

int find_zapped_label_position(Robot *cur_robot, char *name)
{
  Label *cur_label = find_zapped_label(cur_robot, name);

  if(cur_label)
  {
    return cur_label->position;
  }
  else
  {
    return -1;
  }
}

Label *find_label(Robot *cur_robot, char *name)
{
  int total = cur_robot->num_labels - 1;
  int bottom = 0, top = total, middle = 0;
  int cmpval = 0;
  Label **base = cur_robot->label_list;
  Label *current;

  while(bottom <= top)
  {
    middle = (top + bottom) / 2;
    current = base[middle];

    cmpval = strcasecmp(name, current->name);

    if(cmpval > 0)
    {
      bottom = middle + 1;
    }
    else
    {
      if(cmpval < 0)
      {
        top = middle - 1;
      }
      else
      {
        // Found a match, see if there's an earlier one
        while(middle)
        {
          current = base[middle - 1];
          if(!strcasecmp(current->name, name))
          {
            middle--;
          }
          else
          {
            break;
          }
        }

        current = base[middle];

        // Now find a non-zapped one
        while(middle <= total)
        {
          if(current->zapped)
          {
            if(middle == total)
              return NULL;

            middle++;
            current = base[middle];
            if(strcasecmp(current->name, name))
            {
              return NULL;
            }
          }
          else
          {
            break;
          }
        }

        return current;
      }
    }
  }

  return NULL;
}

// This should return the last zapped label found, for restore label
// to work correctly.

Label *find_zapped_label(Robot *cur_robot, char *name)
{
  int total = cur_robot->num_labels - 1;
  int bottom = 0, top = total, middle = 0;
  int cmpval = 0;
  Label **base = cur_robot->label_list;
  Label *current;

  while(bottom <= top)
  {
    middle = (top + bottom) / 2;
    current = base[middle];
    cmpval = strcasecmp(name, current->name);

    if(cmpval > 0)
    {
      bottom = middle + 1;
    }
    else
    {
      if(cmpval < 0)
      {
        top = middle - 1;
      }
      else
      {
       // Found a match, see if there's a later one
        while(middle < total)
        {
          current = base[middle + 1];
          if(!strcasecmp(current->name, name))
          {
            middle++;
          }
          else
          {
            break;
          }
        }

        current = base[middle];

        // Now find a zapped one
        while(middle >= 0)
        {
          if(!current->zapped)
          {
            if(!middle)
              return NULL;

            middle--;
            current = base[middle];
            if(strcasecmp(current->name, name))
            {
              return NULL;
            }
          }
          else
          {
            break;
          }
        }

        return current;
      }
    }
  }

  return NULL;
}

// Returns 1 if found, first is the first robot in the list,
// last is the last. If not found, first and last are the position to place
// into.

int find_robot(Board *src_board, char *name, int *first, int *last)
{
  int total = src_board->num_robots_active - 1;
  int bottom = 0, top = total, middle = 0;
  int cmpval = 0;
  Robot **base = src_board->robot_list_name_sorted;
  Robot *current;
  int f, l;

  while(bottom <= top)
  {
    middle = (top + bottom) / 2;
    current = base[middle];
    cmpval = strcasecmp(name, current->robot_name);

    if(cmpval > 0)
    {
      bottom = middle + 1;
    }
    else
    {
      if(cmpval < 0)
      {
        top = middle - 1;
      }
      else
      {
        // Find any prior occurances
        f = middle;
        l = middle;

        while((f > 0) && (!strcasecmp(name, (base[f - 1])->robot_name)))
        {
          f--;
        }

        *first = f;

        while((l < total) && (!strcasecmp(name, (base[l + 1])->robot_name)))
        {
          l++;
        }

        *last = l;
        return 1;
      }
    }
  }

  if(cmpval > 0)
  {
    *first = middle + 1;
    *last = middle + 1;
  }
  else
  {
    *first = middle;
    *last = middle;
  }

  return 0;
}

void send_robot_def(World *mzx_world, int robot_id, int mesg_id)
{
  switch(mesg_id)
  {
    case 0:
      send_robot_id(mzx_world, robot_id, "TOUCH", 0);
      break;

    case 1:
      send_robot_id(mzx_world, robot_id, "BOMBED", 0);
      break;

    case 2:
      send_robot_all(mzx_world, "INVINCO");
      break;

    case 3:
      send_robot_id(mzx_world, robot_id, "PUSHED", 0);
      break;

    case 4:
      if(send_robot_id(mzx_world, robot_id, "PLAYERSHOT", 0))
      {
        send_robot_id(mzx_world, robot_id, "SHOT", 0);
      }
      break;

    case 5:
      if(send_robot_id(mzx_world, robot_id, "NEUTRALSHOT", 0))
      {
        send_robot_id(mzx_world, robot_id, "SHOT", 0);
      }
      break;

    case 6:
      if(send_robot_id(mzx_world, robot_id, "ENEMYSHOT", 0))
      {
        send_robot_id(mzx_world, robot_id, "SHOT", 0);
      }
      break;

    case 7:
      send_robot_all(mzx_world, "PLAYERHIT");
      break;

    case 8:
      send_robot_id(mzx_world, robot_id, "LAZER", 0);
      break;

    case 9:
      send_robot_id(mzx_world, robot_id, "SPITFIRE", 0);
      break;

    case 10:
      send_robot_all(mzx_world, "JUSTLOADED");
      break;

    case 11:
      send_robot_all(mzx_world, "JUSTENTERED");
      break;

    case 12:
      send_robot_all(mzx_world, "GOOPTOUCHED");
      break;

    case 13:
      send_robot_all(mzx_world, "PLAYERHURT");
      break;
  }
}

void send_robot(World *mzx_world, char *name, char *mesg,
 int ignore_lock)
{
  Board *src_board = mzx_world->current_board;
  int first, last;

  if(!strcasecmp(name, "all"))
  {
    send_robot_all(mzx_world, mesg);
  }
  else

  // See if it's the global robot
  if(!strcasecmp(name, mzx_world->global_robot.robot_name) &&
   mzx_world->global_robot.used)
  {
    send_robot_direct(&mzx_world->global_robot, mesg, ignore_lock);
  }
  else

  if(find_robot(src_board, name, &first, &last))
  {
    while(first <= last)
    {
      send_robot_direct(src_board->robot_list_name_sorted[first], mesg, ignore_lock);
      first++;
    }
  }
  else
  {
    send_sensors(mzx_world, name, mesg);
  }
}

void send_sensors(World *mzx_world, char *name, char *mesg)
{
  Board *src_board = mzx_world->current_board;

  if(src_board->num_sensors)
  {
    // Sensors
    // Set command- 0-3 move, 4 die, 256 | # char, 512 | # color (hex)
    int command = -1; // No command yet

    // Check movement commands
    if(mesg[1] == 0)
    {
      char first_letter = mesg[0];
      if((first_letter >= 'a') && (first_letter <= 'z'))
        first_letter -= 32;

      switch(first_letter)
      {
        case 'N':
          command = 0;
          break;

        case 'S':
          command = 1;
          break;

        case 'E':
          command = 2;
          break;

        case 'W':
          command = 3;
          break;
      }
    }

    // Die?
    if(!strcasecmp("DIE", mesg))
      command = 4;

    // Char___? (___ can be ### or 'c')
    if(!strncasecmp("CHAR", mesg, 4))
    {
     command = 256;
      if(mesg[4] == '\'')
        command = 0x100 | mesg[5];
      else
        command = 0x100 | strtol(mesg + 4, NULL, 10) & 0xFF;
    }

    // Color__? (__ is hex)
    if(!strncasecmp("COLOR", mesg, 5))
    {
      command = 512 | strtol(mesg + 5, NULL, 16) & 0xFF;
    }

    if(command != -1)
    {
      Sensor **sensor_list = src_board->sensor_list;
      Sensor *current_sensor;
      int i;

      if(!strcasecmp(name, "ALL"))
      {
        for(i = 1; i <= src_board->num_sensors; i++)
        {
          current_sensor = sensor_list[i];
          if(current_sensor)
          {
            send_sensor_command(mzx_world, i, command);
          }
        }
      }
      else
      {
        for(i = 1; i <= src_board->num_sensors; i++)
        {
          current_sensor = sensor_list[i];
          if(!strcasecmp(name, current_sensor->sensor_name) &&
           (current_sensor != NULL))
          {
            send_sensor_command(mzx_world, i, command);
          }
        }
      }
    }
  }
}

void send_sensor_command(World *mzx_world, int id, int command)
{
  Board *src_board = mzx_world->current_board;
  Sensor *cur_sensor = src_board->sensor_list[id];
  int x, y, under;
  char *level_under_id = src_board->level_under_id;
  char *level_under_param = src_board->level_under_param;
  char *level_under_color = src_board->level_under_color;
  char *level_id = src_board->level_id;
  char *level_param = src_board->level_param;
  char *level_color = src_board->level_color;
  int board_width = src_board->board_width;
  int board_height = src_board->board_height;
  int player_x = mzx_world->player_x;
  int player_y = mzx_world->player_y;
  int player_offset = player_x + (player_y * board_width);
  int offset;
  int move_status;

  // Find sensor
  if(!(command & 0x100))
  {
    // Don't bother for a char cmd
    under = 0;
    if((level_under_id[player_offset] == 122) &&
     (level_under_param[player_offset] == id))
    {
      under = 1;
      x = player_x;
      y = player_y;
    }
    else
    {
      int found = 0;
      for(y = 0, offset = 0; y < board_height; y++)
      {
        for(x = 0; x < board_width; x++, offset++)
        {
          if((level_id[offset] == 122) &&
           (level_param[offset] == id))
          {
            found = 1;
            break;
          }
        }
        if(found)
          break;
      }
    }
  }

  // Cmd
  switch(command)
  {
    case 0:
    case 1:
    case 2:
    case 3:
    {
      move_status = 0;
      if(under)
      {
        // Attempt to move player, then if ok,
        // put sensor underneath and delete from
        // original space.
        move_status = move(mzx_world, x, y, command,
         1 | 2 | 4 | 8 | 16);

        if(!move_status)
        {
          // Moved! Find player...
          find_player(mzx_world);
          player_x = mzx_world->player_x;
          player_y = mzx_world->player_y;
          player_offset = player_x + (player_y * board_width);

          move_status = 2;
        }
        else
        {
          // Sensorthud!
          send_robot(mzx_world, cur_sensor->robot_to_mesg, "SENSORTHUD", 0);
          // Done.
        }
        move_status = 2;
      }
      else
      {
        // Attempt to move sensor.
        move_status = move(mzx_world, x, y, command,
         1 | 2 | 4 | 8 | 16 | 128);
        if(move_status == 2)
        {
          step_sensor(mzx_world, id);
        }
      }

      if(move_status == 2)
      {
        // Met player- so put under player!
        mzx_world->under_player_id = level_under_id[player_offset];
        mzx_world->under_player_param = level_under_param[player_offset];
        mzx_world->under_player_color = level_under_color[player_offset];
        level_under_id[player_offset] = 122;
        level_under_param[player_offset] = id;
        level_under_color[player_offset] = level_color[x + (y * board_width)];
        id_remove_top(mzx_world, x, y);
      }
      else

      if(move_status != 0)
      {
        // Sensorthud!
        send_robot(mzx_world, cur_sensor->robot_to_mesg, "SENSORTHUD", 0);
      }
      break;
    }

    case 4:
    {
      if(under)
      {
        id_remove_under(src_board, x, y);
        clear_sensor_id(src_board, id);
      }
      else
      {
        id_remove_top(mzx_world, x, y);
        clear_sensor_id(src_board, id);
      }
      break;
    }

    default:
    {
      if(command & 0x100)
      {
        cur_sensor->sensor_char = command - 256;
      }
      else
      {
        if(under)
        {
          level_under_color[player_offset] = command - 512;
        }
        else
        {
          src_board->level_color[x + (y * board_width)] =
           command - 512;
        }
      }
      break;
    }
  }
}

int send_robot_id(World *mzx_world, int id, char *mesg, int ignore_lock)
{
  Robot *cur_robot = mzx_world->current_board->robot_list[id];
  return send_robot_direct(cur_robot, mesg, ignore_lock);
}

void send_robot_all(World *mzx_world, char *mesg)
{
  Board *src_board = mzx_world->current_board;
  int i;

  if(mzx_world->global_robot.used)
  {
    send_robot_direct(&(mzx_world->global_robot), mesg, 0);
  }

  for(i = 0; i < src_board->num_robots_active; i++)
  {
    send_robot_direct(src_board->robot_list_name_sorted[i], mesg, 0);
  }
}

void set_robot_position(Robot *cur_robot, int position)
{
  cur_robot->cur_prog_line = position;
  cur_robot->pos_within_line = 0;
  cur_robot->cycle_count = cur_robot->robot_cycle - 1;

  if(cur_robot->status == 1)
    cur_robot->status = 2;
}

int send_robot_direct(Robot *cur_robot, char *mesg, int ignore_lock)
{
  int i = 1;
  char *robot_program = cur_robot->program;
  int new_position;

  if((cur_robot->is_locked) && (!ignore_lock))
    return 1; // Locked

  if(cur_robot->program_length < 3)
    return 2; // No program!

  // Are we going to a subroutine? Returning? - Exo
  if(mesg[0] == '#')
  {
    // returning?
    if(!strcasecmp(mesg + 1, "return"))
    {
      // Don't let a return into nothingness happen..
      if(cur_robot->stack_pointer)
      {
        int return_pos = robot_stack_pop(cur_robot);
        set_robot_position(cur_robot, return_pos);
      }
    }
    else

    // returning to the TOP?
    if(!strcasecmp(mesg + 1, "top"))
    {
      // Don't let a return into nothingness happen..
      if(cur_robot->stack_pointer)
      {
        set_robot_position(cur_robot, cur_robot->stack[0]);
        cur_robot->stack_pointer = 0;
      }
      // Keep this endian safe for now.
    }
    else
    {
      new_position = find_label_position(cur_robot, mesg);

      if(new_position != -1)
      {
        // Push the current address onto the stack
        // If a maximum overflow happened, this simply won't work.
        int robot_position = cur_robot->cur_prog_line;
        int return_position;

        if(robot_position)
          return_position = robot_position + robot_program[robot_position] + 2;
        else
          return_position = 0;

        robot_stack_push(cur_robot, return_position);
        // Do the jump
        set_robot_position(cur_robot, new_position);
      }
    }
  }
  else
  {
    new_position = find_label_position(cur_robot, mesg);

    if(new_position != -1)
    {
      set_robot_position(cur_robot, new_position);
      return 0;
    }

    return 2;
  }

  return 0;
}

// Run a set of x/y pairs through the prefixes
void prefix_first_last_xy(World *mzx_world, int *fx, int *fy,
 int *lx, int *ly, int robotx, int roboty)
{
  Board *src_board = mzx_world->current_board;
  int player_x;
  int player_y;
  int board_width = src_board->board_width;
  int board_height = src_board->board_height;
  int tfx = *fx;
  int tfy = *fy;
  int tlx = *lx;
  int tly = *ly;

  switch(mzx_world->first_prefix)
  {
    case 1:
    case 5:
      tfx += robotx;
      tfy += roboty;
      break;

    case 2:
    case 6:
      find_player(mzx_world);
      tfx += mzx_world->player_x;
      tfy += mzx_world->player_y;
      break;

    case 3:
      tfx += get_counter(mzx_world, "FIRSTXPOS", 0);
      tfy += get_counter(mzx_world, "FIRSTYPOS", 0);
      break;

    case 7:
      tfx += get_counter(mzx_world, "XPOS", 0);
      tfy += get_counter(mzx_world, "YPOS", 0);
      break;
  }

  switch(mzx_world->last_prefix)
  {
    case 1:
    case 5:
      tlx += robotx;
      tly += roboty;
      break;

    case 2:
    case 6:
      find_player(mzx_world);
      tlx += mzx_world->player_x;
      tly += mzx_world->player_y;
      break;

    case 3:
      tlx += get_counter(mzx_world, "LASTXPOS", 0);
      tly += get_counter(mzx_world, "LASTYPOS", 0);
      break;

    case 7:
      tlx += get_counter(mzx_world, "XPOS", 0);
      tly += get_counter(mzx_world, "YPOS", 0);
      break;
  }

  if(tfx < 0) tfx = 0;
  if(tfy < 0) tfy = 0;
  if(tlx < 0) tlx = 0;
  if(tly < 0) tly = 0;
  if(tfx >= board_width) tfx = board_width - 1;
  if(tfy >= board_height) tfy = board_height - 1;
  if(tlx >= board_width) tlx = board_width - 1;
  if(tly >= board_height) tly = board_height - 1;

  *fx = tfx;
  *fy = tfy;
  *lx = tlx;
  *ly = tly;
}

// These are primarily for copy {overlay} block. Allows a variable
// width/height so that coordinates can be fixed against more than
// simply the board (for instance vlayer). In the future this could
// also be used to copy inbetween boards, perhaps.

void prefix_first_xy_var(World *mzx_world, int *fx, int *fy,
 int robotx, int roboty, int width, int height)
{
  Board *src_board = mzx_world->current_board;
  int player_x;
  int player_y;
  int tfx = *fx;
  int tfy = *fy;

  switch(mzx_world->first_prefix)
  {
    case 1:
    case 5:
      tfx += robotx;
      tfy += roboty;
      break;

    case 2:
    case 6:
      find_player(mzx_world);
      tfx += mzx_world->player_x;
      tfy += mzx_world->player_y;
      break;

    case 3:
      tfx += get_counter(mzx_world, "FIRSTXPOS", 0);
      tfy += get_counter(mzx_world, "FIRSTYPOS", 0);
      break;

    case 7:
      tfx += get_counter(mzx_world, "XPOS", 0);
      tfy += get_counter(mzx_world, "YPOS", 0);
      break;
  }

  if(tfx < 0) tfx = 0;
  if(tfy < 0) tfy = 0;
  if(tfx >= width)
    tfx = width - 1;
  if(tfy >= height)
    tfy = height - 1;

  *fx = tfx;
  *fy = tfy;
}

void prefix_last_xy_var(World *mzx_world, int *lx, int *ly,
 int robotx, int roboty, int width, int height)
{
  Board *src_board = mzx_world->current_board;
  int tlx = *lx;
  int tly = *ly;

  switch(mzx_world->last_prefix)
  {
    case 1:
    case 5:
      tlx += robotx;
      tly += roboty;
      break;

    case 2:
    case 6:
      find_player(mzx_world);
      tlx += mzx_world->player_x;
      tly += mzx_world->player_y;
      break;

    case 3:
      tlx += get_counter(mzx_world, "LASTXPOS", 0);
      tly += get_counter(mzx_world, "LASTYPOS", 0);
      break;

    case 7:
      tlx += get_counter(mzx_world, "XPOS", 0);
      tly += get_counter(mzx_world, "YPOS", 0);
      break;
  }

  if(tlx < 0)
    tlx = 0;

  if(tly < 0)
    tly = 0;

  if(tlx >= width)
    tlx = width - 1;

  if(tly >= height)
    tly = height - 1;

  *lx = tlx;
  *ly = tly;
}

void prefix_mid_xy_var(World *mzx_world, int *mx, int *my,
 int robotx, int roboty, int width, int height)
{
  Board *src_board = mzx_world->current_board;
  int player_x;
  int player_y;
  int tmx = *mx;
  int tmy = *my;

  switch(mzx_world->first_prefix)
  {
    case 1:
      tmx += robotx;
      tmy += roboty;
      break;

    case 2:
      find_player(mzx_world);
      tmx += mzx_world->player_x;
      tmy += mzx_world->player_y;
      break;

    case 3:
      tmx += get_counter(mzx_world, "XPOS", 0);
      tmy += get_counter(mzx_world, "YPOS", 0);
      break;
  }

  if(tmx < 0)
    tmx = 0;

  if(tmy < 0)
    tmy = 0;

  if(tmx >= width)
    tmx = width - 1;

  if(tmy >= height)
    tmy = height - 1;

  *mx = tmx;
  *my = tmy;
}

// Just does the middle prefixes, since those are all that's usually
// needed...
void prefix_mid_xy(World *mzx_world, int *mx, int *my, int x, int y)
{
  Board *src_board = mzx_world->current_board;
  int board_width = src_board->board_width;
  int board_height = src_board->board_height;
  int tmx = *mx;
  int tmy = *my;

  switch(mzx_world->mid_prefix)
  {
    case 1:
      tmx += x;
      tmy += y;
      break;

    case 2:
      find_player(mzx_world);
      tmx += mzx_world->player_x;
      tmy += mzx_world->player_y;
      break;

    case 3:
      tmx += get_counter(mzx_world, "XPOS", 0);
      tmy += get_counter(mzx_world, "YPOS", 0);
      break;
  }

  if(tmx < 0)
    tmx = 0;
  if(tmy < 0)
    tmy = 0;
  if(tmx >= board_width)
    tmx = board_width - 1;
  if(tmy >= board_height)
    tmy = board_height - 1;

  *mx = tmx;
  *my = tmy;
}

// Move an x/y pair in a given direction. Returns non-0 if edge reached.
int move_dir(Board *src_board, int *x, int *y, int dir)
{
  int board_width = src_board->board_width;
  int board_height = src_board->board_height;
  int tx = *x;
  int ty = *y;

  switch(dir)
  {
    case 0:
      if(ty == 0)
      {
        return 1;
      }
      ty--;
      break;

    case 1:
      if(ty == (board_height - 1))
      {
        return 1;
      }
      ty++;
      break;

    case 2:
      if(tx == (board_width - 1))
      {
        return 1;
      }
      tx++;
      break;

    case 3:
      if(tx == 0)
      {
        return 1;
      }
      tx--;
      break;
  }

  *x = tx;
  *y = ty;

  return 0;
}

// Returns the numeric value pointed to OR the numeric value represented
// by the counter string pointed to. (the ptr is at the param within the
// command)
// Sign extends the result, for now...

// NOTE- CLIPS COUNTER NAMES!
int parse_param(World *mzx_world, char *program, int id)
{
  char ibuff[64];

  if(program[0] == 0)
  {
    // Numeric
    return (signed short)((int)program[1] | (int)(program[2] << 8));
  }
  // Expressions - Exo
  if((program[1] == '(') && mzx_world->version >= 0x244)
  {
    char *e_ptr = (char *)program + 2;
    int error, val;
    val = parse_expression(mzx_world, &e_ptr, error, id);
    if(!error)
    {
      return val;
    }
  }
  tr_msg(mzx_world, program + 1, id, ibuff);

  return get_counter(mzx_world, ibuff, id);
}

// Returns location of next parameter (pos is loc of current parameter)
int next_param(char *ptr, int pos)
{
  if(ptr[pos])
  {
    return ptr[pos] + 1;
  }
  else
  {
    return 3;
  }
}

char *next_param_pos(char *ptr)
{
  int index = *ptr;
  if(index)
  {
    return ptr + index + 1;
  }
  else
  {
    return ptr + 3;
  }
}

// Internal only. NOTE- IF WE EVER ALLOW ZAPPING OF LABELS NOT IN CURRENT
// ROBOT, USE A COPY OF THE *LABEL BEFORE THE PREPARE_ROBOT_MEM!

int restore_label(Robot *cur_robot, char *label)
{
  Label *dest_label = find_zapped_label(cur_robot, label);

  if(dest_label)
  {
    int position = dest_label->position;
    char *program = cur_robot->program;

    dest_label->zapped = 0;
    program[position - program[position - 1] - 1] = 106;
    return 1;
  }
  else
  {
    return 0;
  }
}

int zap_label(Robot *cur_robot, char *label)
{
  Label *dest_label = find_label(cur_robot, label);

  if(dest_label)
  {
    int position = dest_label->position;
    char *program = cur_robot->program;

    dest_label->zapped = 1;
    program[position - program[position - 1] - 1] = 108;

    return 1;
  }
  else
  {
    return 0;
  }
}

// Turns a color (including those w/??) to a real color (0-255)
int fix_color(int color, int def)
{
  if(color < 256)
    return color;
  if(color < 272)
    return (color & 0x0F) + (def & 0xF0);
  if(color < 288)
    return ((color - 272) << 4) + (def & 0x0F);

  return def;
}

void robot_box_display(World *mzx_world, char *program,
 char *label_storage, int id)
{
  Board *src_board = mzx_world->current_board;
  Robot *cur_robot = src_board->robot_list[id];
  // Important status vars (insert kept in intake.cpp)
  int pos = 0, old_pos; // Where IN robot?
  int key; // Key
  int i, i2;
  int cur_cmd;
  int fade_status;

  label_storage[0] = 0;

  // Draw screen
  save_screen();
  m_show();

  fade_status = get_fade_status();

  if(fade_status)
  {
    clear_screen(32, 0);
    insta_fadein();
  }

  scroll_edging(mzx_world, 4);
  // Write robot name
  if(!cur_robot->robot_name[0])
  {
    write_string("Interaction", 35, 4, mzx_world->scroll_title_color, 0);
  }
  else
  {
    write_string(cur_robot->robot_name,
     40 - strlen(cur_robot->robot_name) / 2, 4, mzx_world->scroll_title_color, 1);
  }

  // Scan section and mark all invalid counter-controlled options as codes
  // 249.

  do
  {
    if(program[pos + 1] == 249)
      program[pos + 1] = 105;

    if(program[pos + 1] == 105)
    {
      if(!parse_param(mzx_world, program + (pos + 2), id))
        program[pos + 1] = 249;
    }

    pos += program[pos] + 2;
  } while(program[pos]);

  pos = 0;

  // Backwards
  do
  {
    if(program[pos + 1] == 249)
      program[pos + 1] = 105;

    if(program[pos + 1] == 105)
    {
      if(!parse_param(mzx_world, program + (pos + 2), id))
        program[pos + 1] = 249;
    }

    if(program[pos - 1] == 0xFF)
      break;

    pos -= program[pos - 1] + 2;
  } while(1);

  pos = 0;

  // Loop
  do
  {
    // Display scroll
    robot_frame(mzx_world, program + pos, id);
    update_screen();

    update_event_status_delay();
    key = get_key(keycode_SDL);

    old_pos = pos;

    if(get_mouse_press())
    {
      int mouse_x, mouse_y;
      get_mouse_position(&mouse_x, &mouse_y);

      if((mouse_y >= 6) && (mouse_y <= 18) &&
       (mouse_x >= 8) && (mouse_x <= 71))
      {
        int count = mouse_y - 12;
        if(!count)
        {
          key = SDLK_RETURN;
        }
        else
        {
          if(count > 0)
            pos = robot_box_down(program, pos, count);
          else
            pos = robot_box_up(program, pos, -count);

          warp_mouse(mouse_x, mouse_y - count);
        }
      }
    }

    switch(key)
    {
      case SDLK_UP://Up
        pos = robot_box_up(program, pos, 1);
        break;

      case SDLK_DOWN://Down
        pos = robot_box_down(program, pos, 1);
        break;

      case SDLK_RETURN: // Enter
      {
        key = SDLK_ESCAPE;

        if((program[pos + 1] == 104) || (program[pos + 1] == 105))
        {
          char *next;
          if(program[pos + 1] == 105)
            next = next_param_pos(program + pos + 2) + 1;
          else
            next = program + pos + 3;

          // Goto option! Stores in label_storage
          strcpy(label_storage, next);
        }
        break;
      }

      case SDLK_PAGEDOWN: // Pagedown (by 6 lines)
        pos = robot_box_down(program, pos, 6);
        break;

      case SDLK_PAGEUP: // Pageup (by 6 lines)
        pos = robot_box_up(program, pos, 6);
        break;

      case SDLK_HOME: // Home
        pos = robot_box_up(program, pos, 100000);
        break;

      case SDLK_END: // End
        pos = robot_box_down(program, pos, 100000);
        break;

      default:
      case SDLK_ESCAPE:
      case 0:
        break;
    }

    // Continue?
  } while(key != SDLK_ESCAPE);

  // Scan section and mark all invalid counter-controlled options as codes
  // 105.

  pos = 0;

  do
  {
    if(program[pos + 1] == 249)
    {
      program[pos + 1] = 105;
    }

    pos += program[pos] + 2;

  } while(program[pos]);

  pos = 0;

  // Backwards
  do
  {
    if(program[pos + 1] == 249)
    {
      program[pos + 1] = 105;
    }
    if(program[pos - 1] == 0xFF)
      break;
    pos -= program[pos - 1] + 2;

  } while(1);

  if(fade_status)
    insta_fadeout();

  // Restore screen and exit
  m_hide();
  restore_screen();
}

int robot_box_up(char *program, int pos, int count)
{
  int i, cur_cmd;
  int old_pos;
  int done = 0;

  for(i = 0; (i < count) && (!done); i++)
  {
    old_pos = pos;
    do
    {
      // Go backwards a line (if possible)
      if(program[pos - 1] == 0xFF)
      {
        pos = old_pos;
        done = 1;
      }
      else
      {
        pos -= program[pos - 1] + 2;
        cur_cmd = program[pos + 1];
        if(((cur_cmd < 103) && (cur_cmd != 47)) ||
         ((cur_cmd > 106) && (cur_cmd < 116)) ||
         ((cur_cmd > 117) && (cur_cmd != 249)))
        {
          pos = old_pos;
          done = 1;
        }
      }
    } while(((cur_cmd == 106) || (cur_cmd == 249)) && (!done));

    if(i == 100000)
      i = 99999;
  }

  return pos;
}

int robot_box_down(char *program, int pos, int count)
{
  int i, cur_cmd;
  int old_pos;
  int done = 0;

  for(i = 0; (i < count) && (!done); i++)
  {
    old_pos = pos;
    do
    {
      // Go forward a line (if possible)
      pos += program[pos] + 2;
      if(program[pos] == 0)
      {
        pos = old_pos;
        done = 1;
      }
      else
      {
        cur_cmd = program[pos + 1];
        if(((cur_cmd < 103) && (cur_cmd != 47)) ||
         ((cur_cmd > 106) && (cur_cmd < 116)) ||
         ((cur_cmd > 117) && (cur_cmd != 249)))
        {
          pos = old_pos;
          done = 1;
        }
      }
    } while(((cur_cmd == 106) || (cur_cmd == 249)) && (!done));

    if(i == 100000)
      i = 99999;
  }

  return pos;
}

void robot_frame(World *mzx_world, char *program, int id)
{
  // Displays one frame of a robot. The scroll edging, arrows, and title
  // must already be shown. Simply prints each line. The pointer points
  // to the center line.
  int scroll_base_color = mzx_world->scroll_base_color;
  int cur_cmd;
  int i, pos = 0;
  int old_pos;
  // Display center line
  fill_line(64, 8, 12, 32, scroll_base_color);
  display_robot_line(mzx_world, program, 12, id);

  // Display lines above center line
  for(i = 11; i >= 6; i--)
  {
    fill_line(64, 8, i, 32, scroll_base_color);
    // Go backward to previous line
    old_pos = pos;
    pos = robot_box_up(program, pos, 1);
    if(old_pos != pos)
      display_robot_line(mzx_world, program + pos, i, id);
  }

  // Display lines below center line
  pos = 0;

  for(i = 13; i <= 18; i++)
  {
    fill_line(64, 8, i, 32, scroll_base_color);
    old_pos = pos;
    pos = robot_box_down(program, pos, 1);
    if(old_pos != pos)
      display_robot_line(mzx_world, program + pos, i, id);
  }
}

void display_robot_line(World *mzx_world, char *program, int y, int id)
{
  char ibuff[64];
  char *next;
  int scroll_base_color = mzx_world->scroll_base_color;
  int scroll_arrow_color = mzx_world->scroll_arrow_color;

  switch(program[1])
  {
    case 103: // Normal message
      tr_msg(mzx_world, program + 3, id, ibuff);
      ibuff[64] = 0; // Clip
      write_string(ibuff, 8, y, scroll_base_color, 1);
      break;

    case 104: // Option
      // Skip over label...
      // next is pos of string
      next = next_param_pos(program + 2);
      tr_msg(mzx_world, next + 1, id, ibuff);
      color_string(ibuff, 10, y, scroll_base_color);
      draw_char('', scroll_arrow_color, 8, y);
      break;

    case 105: // Counter-based option
    {
      // Check counter
      int val = parse_param(mzx_world, program + 2, id);
      if(val)
      {
        // Skip over counter and label...
        // next is pos of string
        next = next_param_pos(program + 2);
        next = next_param_pos(next);
        tr_msg(mzx_world, next + 1, id, ibuff);
        color_string(ibuff, 10, y, scroll_base_color);
        draw_char('', scroll_arrow_color, 8, y);
      }
      break;
    }

    case 116: // Colored message
      tr_msg(mzx_world, program + 3, id, ibuff);
      color_string(ibuff, 8, y, scroll_base_color);
      break;

    case 117: // Centered message
    {
      int length, x_position;
      tr_msg(mzx_world, program + 3, id, ibuff);
      length = strlencolor(ibuff);
      x_position = 40 - (length / 2);
      color_string(ibuff, x_position, y, scroll_base_color);
      break;
    }
  }
  // Others, like 47 and 106, are blank lines
}

void push_sensor(World *mzx_world, int id)
{
  Board *src_board = mzx_world->current_board;
  send_robot(mzx_world, (src_board->sensor_list[id])->robot_to_mesg,
   "SENSORPUSHED", 0);
}

void step_sensor(World *mzx_world, int id)
{
  Board *src_board = mzx_world->current_board;
  send_robot(mzx_world, (src_board->sensor_list[id])->robot_to_mesg,
   "SENSORON", 0);
}

// Translates message at target to the given buffer, returning location
// of this buffer. && becomes &, &INPUT& becomes the last input string,
// and &COUNTER& becomes the value of COUNTER. The size of the string is
// clipped to 80 chars.

char *tr_msg(World *mzx_world, char *mesg, int id, char *buffer)
{
  Board *src_board = mzx_world->current_board;
  int sp = 0, dp = 0, i;
  char counter_name[256];

  do
  {
    // Expression!
    if((mesg[sp] == '(') && (mzx_world->version >= 0x244))
    {
      char *arg = mesg + sp + 1;
      int error;
      int val = parse_expression(mzx_world, &arg, error, id);
      if(!error)
      {
        char temp[8];
        sprintf(temp, "%d", val);
        strcpy(buffer + dp, temp);
        dp += strlen(temp);
        sp = arg - mesg + 1;
      }
    }

    if(mesg[sp] != '&')
    {
      buffer[dp++] = mesg[sp++];
    }
    else
    {
      if(mesg[++sp] == '&')
      {
        buffer[dp++] = '&';
        sp++;
      }
      else
      {
        // Input or Counter?
        for(i = 0; i < 128; i++)
        {
          counter_name[i] = mesg[sp++];
          if(mesg[sp])
          {
            if(mesg[sp] == '&')
            {
              sp++;
              break;
            }
          }
          else
          {
            break;
          }
        }

        counter_name[++i] = 0;
        if(!strcasecmp(counter_name, "INPUT"))
        {
          // Input
          strcpy(buffer + dp, src_board->input_string);
          dp += strlen(src_board->input_string);
        }
        else
        {
          // Counter
          // Now could also be a string
          if(is_string(counter_name))
          {
            // Write the value of the counter name
            char t_buf[64];
            t_buf[0] = 0;
            get_string(mzx_world, counter_name, 0, t_buf);
            strcpy(buffer + dp, t_buf);
            dp += strlen(t_buf);
          }
          else
          {
            // #(counter) is a hex representation.
            if(counter_name[0] == '+')
            {
              sprintf(counter_name, "%x", get_counter(mzx_world, counter_name + 1, id));
            }
            else
            {
              if(counter_name[0] == '#')
              {
                char temp[4];
                sprintf(temp, "%x", get_counter(mzx_world, counter_name + 1, id));

                if(temp[1] == 0)
                {
                  temp[2] = 0;
                  temp[1] = temp[0];
                  temp[0] = '0';
                }
                memcpy(counter_name, temp, 4);
              }
              else
              {
                sprintf(counter_name, "%d", get_counter(mzx_world, counter_name, id));
              }
            }

            strcpy(buffer + dp, counter_name);
            dp += strlen(counter_name);
          }
        }
      }
    }

    if(dp > 80)
    {
      dp = 80;
      break;
    }
  } while(mesg[sp]);

  buffer[dp] = 0;
  return buffer;
}

// Don't do this if the entry is not in the normal list
void add_robot_name_entry(Board *src_board, Robot *cur_robot, char *name)
{
  // Find the position
  int first, last;
  int active = src_board->num_robots_active;
  Robot **name_list = src_board->robot_list_name_sorted;

  find_robot(src_board, name, &first, &last);
  // Insert into name list, if it's not at the end
  if(first != active)
  {
    memmove(name_list + first + 1, name_list + first,
     (active - first) * sizeof(Robot *));
  }
  name_list[first] = cur_robot;
  src_board->num_robots_active = active + 1;
}

// Does not remove entry from the normal list
void remove_robot_name_entry(Board *src_board, Robot *cur_robot, char *name)
{
  // Find the position
  int first, last;
  int active = src_board->num_robots_active;
  Robot **name_list = src_board->robot_list_name_sorted;

  find_robot(src_board, name, &first, &last);
  // Find the one that matches the robot
  while(name_list[first] != cur_robot)
    first++;

  // Remove from name list
  active--;

  if(first != active)
  {
    memmove(name_list + first, name_list + first + 1,
     (active - first) * sizeof(Robot *));
  }
  src_board->num_robots_active = active;
}

// This could probably be done in a more efficient manner.
void change_robot_name(Board *src_board, Robot *cur_robot, char *new_name)
{
  // Remove the old one
  remove_robot_name_entry(src_board, cur_robot, cur_robot->robot_name);
  // Add the new one
  add_robot_name_entry(src_board, cur_robot, new_name);
  // And change the actual name
  strcpy(cur_robot->robot_name, new_name);
}

// Works with the ID-list. Will make room for a new one if there aren't any.
int find_free_robot(Board *src_board)
{
  int num_robots = src_board->num_robots;
  int i;
  Robot **robot_list = src_board->robot_list;

  for(i = 1; i <= num_robots; i++)
  {
    if(robot_list[i] == NULL)
      break;
  }

  if(i < 256)
  {
    // Perhaps make a new one
    if(i > num_robots)
    {

      int num_robots_allocated = src_board->num_robots_allocated;
      if(num_robots == num_robots_allocated)
      {
        if(num_robots_allocated)
          num_robots_allocated *= 2;
        else
          num_robots_allocated = 1;

        src_board->robot_list = (Robot **)realloc(robot_list,
         (num_robots_allocated + 1) * sizeof(Robot *));

        src_board->robot_list_name_sorted =
         (Robot **)realloc(src_board->robot_list_name_sorted,
         (num_robots_allocated) * sizeof(Robot *));
        src_board->num_robots_allocated = num_robots_allocated;
      }
      src_board->num_robots = num_robots + 1;
    }
    return i;
  }
  return -1;
}

// Like find_free_robot, but for scrolls. Will also expand the list if
// necessary.
int find_free_scroll(Board *src_board)
{
  int num_scrolls = src_board->num_scrolls;
  int i;
  Scroll **scroll_list = src_board->scroll_list;

  for(i = 1; i <= num_scrolls; i++)
  {
    if(scroll_list[i] == NULL)
      break;
  }

  if(i < 256)
  {
    // Perhaps make a new one
    if(i > num_scrolls)
    {
      int num_scrolls_allocated = src_board->num_scrolls_allocated;
      if(num_scrolls == num_scrolls_allocated)
      {
        if(num_scrolls_allocated)
          num_scrolls_allocated *= 2;
        else
          num_scrolls_allocated = 1;

        src_board->scroll_list = (Scroll **)realloc(scroll_list,
         (num_scrolls_allocated + 1) * sizeof(Scroll *));
        src_board->num_scrolls_allocated = num_scrolls_allocated;
      }
      src_board->num_scrolls = num_scrolls + 1;
    }
    return i;
  }

  return -1;
}

// Like find_free_robot, but for sensors. Will also expand the list if
// necessary.
int find_free_sensor(Board *src_board)
{
  int num_sensors = src_board->num_sensors;
  int i;
  Sensor **sensor_list = src_board->sensor_list;

  for(i = 1; i <= num_sensors; i++)
  {
    if(sensor_list[i] == NULL)
      break;
  }

  if(i < 256)
  {
    // Perhaps make a new one
    if(i > num_sensors)
    {
      int num_sensors_allocated = src_board->num_sensors_allocated;
      if(num_sensors == num_sensors_allocated)
      {
        if(num_sensors_allocated)
          num_sensors_allocated *= 2;
        else
          num_sensors_allocated = 1;

        src_board->sensor_list = (Sensor **)realloc(sensor_list,
         (num_sensors_allocated + 1) * sizeof(Sensor *));
        src_board->num_sensors_allocated = num_sensors_allocated;
      }
      src_board->num_sensors = num_sensors + 1;
    }
    return i;
  }

  return -1;
}

// Duplicates the contents of one robot to a new one, returning the ID
// of the duplicate. Supply the x/y position of where you want it to be
// placed. Returns the ID of location. Does NOT place the robot on the
// board (so be sure to do that). The given id is the slot to add it in;
// be sure that this is a valid (NULL) entry!

void duplicate_robot_direct(Robot *cur_robot, Robot *copy_robot,
 int x, int y)
{
  Label *src_label, *dest_label;
  char *dest_program_location, *src_program_location;
  int program_offset;
  int program_length = cur_robot->program_length;
  int num_labels = cur_robot->num_labels;
  int i;

  // Copy all the contents
  memcpy(copy_robot, cur_robot, sizeof(Robot));
  // We need unique copies of the program and the label cache.
  copy_robot->program = (char *)malloc(program_length);

  src_program_location = cur_robot->program;
  dest_program_location = copy_robot->program;

  memcpy(dest_program_location, src_program_location, program_length);

  if(num_labels)
    copy_robot->label_list = (Label **)malloc(num_labels * sizeof(Label *));
  else
    copy_robot->label_list = NULL;

  program_offset = dest_program_location - src_program_location;

  // Copy each individual label pointer over
  for(i = 0; i < num_labels; i++)
  {
    copy_robot->label_list[i] = (Label *)malloc(sizeof(Label));

    src_label = cur_robot->label_list[i];
    dest_label = copy_robot->label_list[i];

    memcpy(dest_label, src_label, sizeof(Label));
    // The name pointer actually has to be readjusted to match the new program
    dest_label->name += program_offset;
  }

  // Give the robot a new, fresh stack
  copy_robot->stack = (int *)malloc(ROBOT_START_STACK * sizeof(int));
  copy_robot->stack_size = ROBOT_START_STACK;
  copy_robot->stack_pointer = 0;
  copy_robot->xpos = x;
  copy_robot->ypos = y;

  if(cur_robot->cur_prog_line)
    copy_robot->cur_prog_line = 1;

  copy_robot->pos_within_line = 0;
  copy_robot->status = 0;
}

// Finds a robot ID then duplicates a robot there.

int duplicate_robot(Board *src_board, Robot *cur_robot, int x, int y)
{
  int dest_id = find_free_robot(src_board);
  if(dest_id != -1)
  {
    Robot *copy_robot = (Robot *)malloc(sizeof(Robot));
    duplicate_robot_direct(cur_robot, copy_robot, x, y);
    add_robot_name_entry(src_board, copy_robot, copy_robot->robot_name);
    src_board->robot_list[dest_id] = copy_robot;
  }

  return dest_id;
}

// Makes the dest robot a replication of the source. ID's are given
// so that it can modify the ID table. The dest position is assumed
// to already contain something, and is thus cleared first.
// Will not allow replacing the global robot.
void replace_robot(Board *src_board, Robot *src_robot, int dest_id)
{
  char old_name[64];
  int x = (src_board->robot_list[dest_id])->xpos;
  int y = (src_board->robot_list[dest_id])->ypos;
  Robot *cur_robot = src_board->robot_list[dest_id];

  strcpy(old_name, cur_robot->robot_name);

  clear_robot_contents(cur_robot);
  duplicate_robot_direct(src_robot, cur_robot, x, y);
  strcpy(cur_robot->robot_name, old_name);

  if(dest_id)
    change_robot_name(src_board, cur_robot, src_robot->robot_name);
}

void replace_scroll(Board *src_board, Scroll *src_scroll, int dest_id)
{
  Scroll *cur_scroll = src_board->scroll_list[dest_id];
  clear_scroll_contents(cur_scroll);
  duplicate_scroll_direct(src_scroll, cur_scroll);
}

void replace_sensor(Board *src_board, Sensor *src_sensor, int dest_id)
{
  Sensor *cur_sensor = src_board->sensor_list[dest_id];
  duplicate_sensor_direct(src_sensor, cur_sensor);
}

// Like duplicate_robot_direct, but for scrolls.

void duplicate_scroll_direct(Scroll *cur_scroll, Scroll *copy_scroll)
{
  int mesg_size = cur_scroll->mesg_size;

  // Copy all the contents
  memcpy(copy_scroll, cur_scroll, sizeof(Scroll));
  // We need unique copies of the program and the label cache.
  copy_scroll->mesg = (char *)malloc(mesg_size);
  memcpy(copy_scroll->mesg, cur_scroll->mesg, mesg_size);
}

int duplicate_scroll(Board *src_board, Scroll *cur_scroll)
{
  int dest_id = find_free_scroll(src_board);
  if(dest_id != -1)
  {
    int i;
    Scroll *copy_scroll = (Scroll *)malloc(sizeof(Scroll));
    duplicate_scroll_direct(cur_scroll, copy_scroll);
    src_board->scroll_list[dest_id] = copy_scroll;
  }

  return dest_id;
}

// Like duplicate_robot_direct, but for sensors.

void duplicate_sensor_direct(Sensor *cur_sensor, Sensor *copy_sensor)
{
  // Copy all the contents
  memcpy(copy_sensor, cur_sensor, sizeof(Sensor));
}

int duplicate_sensor(Board *src_board, Sensor *cur_sensor)
{
  int dest_id = find_free_sensor(src_board);
  if(dest_id != -1)
  {
    Sensor *copy_sensor = (Sensor *)malloc(sizeof(Sensor));
    duplicate_sensor_direct(cur_sensor, copy_sensor);
    src_board->sensor_list[dest_id] = copy_sensor;
  }

  return dest_id;
}

// These aren't really needed right now, I think, but they're there...

int get_robot_board_offset(Board *src_board, Robot *cur_robot)
{
  int x = cur_robot->xpos;
  int y = cur_robot->ypos;
  int board_width = src_board->board_width;
  int offset = x + (y * board_width);
  char *level_id = src_board->level_id;
  char *level_param = src_board->level_param;
  Robot **robot_list = src_board->robot_list;
  int d_id = level_id[offset];

  if((d_id == 123) || (d_id == 124))
  {
    if(robot_list[level_param[offset]] == cur_robot)
      return offset;
  }
  else
  {
    int board_size = board_width * (src_board->board_height);

    for(offset = 0; offset < board_size; offset++)
    {
      d_id = level_id[offset];
      if((d_id == 123) || (d_id == 124))
      {
        if(robot_list[level_param[offset]] == cur_robot)
          return offset;
      }
    }
  }

  return -1;
}

int get_scroll_board_offset(Board *src_board, Scroll *cur_scroll)
{
  char *level_id = src_board->level_id;
  char *level_param = src_board->level_param;
  Scroll **scroll_list = src_board->scroll_list;
  int offset;
  int d_id;
  int board_size = src_board->board_width * src_board->board_height;

  for(offset = 0; offset < board_size; offset++)
  {
    d_id = level_id[offset];
    if((d_id == 125) || (d_id == 126))
    {
      if(scroll_list[level_param[offset]] == cur_scroll)
        return offset;
    }
  }

  return -1;
}

int get_sensor_board_offset(Board *src_board, Sensor *cur_sensor)
{
  char *level_id = src_board->level_id;
  char *level_param = src_board->level_param;
  Sensor **sensor_list = src_board->sensor_list;
  int offset;
  int d_id;
  int board_size = src_board->board_width * src_board->board_height;

  for(offset = 0; offset < board_size; offset++)
  {
    d_id = level_id[offset];
    if(d_id == 122)
    {
      if(sensor_list[level_param[offset]] == cur_sensor)
        return offset;
    }
  }

  return -1;
}

// This function will remove any null entries in the object lists
// (for robots, scrolls, and sensors), and adjust all of the board
// params to compensate. This should always be used before saving
// the world/game, and ideally when loading too.

void optimize_null_objects(Board *src_board)
{
  int num_robots = src_board->num_robots;
  int num_scrolls = src_board->num_scrolls;
  int num_sensors = src_board->num_sensors;
  Robot **robot_list = src_board->robot_list;
  Scroll **scroll_list = src_board->scroll_list;
  Sensor **sensor_list = src_board->sensor_list;
  Robot **optimized_robot_list =
   (Robot **)malloc(sizeof(Robot *) * num_robots + 1);
  Scroll **optimized_scroll_list =
   (Scroll **)malloc(sizeof(Scroll *) * num_scrolls + 1);
  Sensor **optimized_sensor_list =
   (Sensor **)malloc(sizeof(Sensor *) * num_sensors + 1);
  int *robot_id_translation_list =
   (int *)malloc(sizeof(int) * num_robots + 1);
  int *scroll_id_translation_list =
   (int *)malloc(sizeof(int) * num_scrolls + 1);
  int *sensor_id_translation_list =
   (int *)malloc(sizeof(int) * num_sensors + 1);
  Robot *cur_robot;
  Scroll *cur_scroll;
  Sensor *cur_sensor;
  int board_width = src_board->board_width;
  int board_height = src_board->board_height;
  int i, i2;
  int x, y, offset;
  char *level_id = src_board->level_id;
  char *level_param = src_board->level_param;
  int d_id, d_param, d_new_param;
  int do_modify = 0;

  for(i = 1, i2 = 1; i <= num_robots; i++)
  {
    cur_robot = robot_list[i];
    if(cur_robot != NULL)
    {
      optimized_robot_list[i2] = cur_robot;
      robot_id_translation_list[i] = i2;
      i2++;
    }
  }

  if(i2 != i)
  {
		do_modify |= 1;
    optimized_robot_list[0] = robot_list[0];
    free(robot_list);
    src_board->robot_list =
     (Robot **)realloc(optimized_robot_list, sizeof(Robot *) * i2);
    src_board->num_robots = i2 - 1;
    src_board->num_robots_allocated = i2 - 1;
  }
  else
  {
    free(optimized_robot_list);
  }

  for(i = 1, i2 = 1; i <= num_scrolls; i++)
  {
    cur_scroll = scroll_list[i];
    if(cur_scroll != NULL)
    {
      optimized_scroll_list[i2] = cur_scroll;
      scroll_id_translation_list[i] = i2;
      i2++;
    }
  }

  if(i2 != i)
  {
		do_modify |= 1;
    optimized_scroll_list[0] = scroll_list[0];
    free(scroll_list);
    src_board->scroll_list =
     (Scroll **)realloc(optimized_scroll_list, sizeof(Scroll *) * i2);
    src_board->num_scrolls = i2 - 1;
    src_board->num_scrolls_allocated = i2 - 1;
  }
  else
  {
    free(optimized_scroll_list);
  }

  for(i = 1, i2 = 1; i <= num_sensors; i++)
  {
    cur_sensor = sensor_list[i];
    if(cur_sensor != NULL)
    {
      // If there's a gap, fill it up
      optimized_sensor_list[i2] = cur_sensor;
      sensor_id_translation_list[i] = i2;
      i2++;
    }
  }

  if(i2 != i)
  {
		do_modify |= 1;
    optimized_sensor_list[0] = sensor_list[0];
    free(sensor_list);
    src_board->sensor_list =
     (Sensor **)realloc(optimized_sensor_list, sizeof(Sensor *) * i2);
    src_board->num_sensors = i2 - 1;
    src_board->num_sensors_allocated = i2 - 1;
  }
  else
  {
    free(optimized_sensor_list);
  }

  // Make sure this is up to date
  robot_list = src_board->robot_list;

	if(do_modify)
	{
		// Now, physically modify all references on the board
		for(y = 0, offset = 0; y < board_height; y++)
		{
			for(x = 0; x < board_width; x++, offset++)
			{
				d_id = level_id[offset];
				// Is it a robot?
				if((d_id == 123) || (d_id == 124))
				{
					d_param = level_param[offset];
					d_new_param = robot_id_translation_list[d_param];
					level_param[offset] = d_new_param;
					// Also, as a service, set the x/y coordinates, just in case
					// they haven't been initialized (this is a potential pitfall)
					cur_robot = robot_list[d_new_param];
					cur_robot->xpos = x;
					cur_robot->ypos = y;
				}
				else
	
				// Is it a scoll?
				if((d_id == 125) || (d_id == 126))
				{
					d_param = level_param[offset];
					d_new_param = scroll_id_translation_list[d_param];
					level_param[offset] = d_new_param;
				}
				else
	
				// Is it a sensor?
				if(d_id == 122)
				{
					d_param = level_param[offset];
					d_new_param = sensor_id_translation_list[d_param];
					level_param[offset] = d_new_param;
				}
			}
		}
	}

  // Free the lists
  free(robot_id_translation_list);
  free(scroll_id_translation_list);
  free(sensor_id_translation_list);
}

Robot *create_blank_robot(int x, int y)
{
  Robot *cur_robot = (Robot *)malloc(sizeof(Robot));
  create_blank_robot_direct(cur_robot, x, y);
}

void create_blank_robot_direct(Robot *cur_robot, int x, int y)
{
  char *program = (char *)malloc(2);

  memset(cur_robot, 0, sizeof(Robot));

  cur_robot->robot_name[0] = 0;
  cur_robot->program = program;
  cur_robot->program_length = 2;
  program[0] = 0xFF;
  program[1] = 0x00;

  cur_robot->xpos = x;
  cur_robot->ypos = y;
  cur_robot->robot_char = 2;
  cur_robot->bullet_type = 1;
}

Scroll *create_blank_scroll()
{
  Scroll *cur_scroll = (Scroll *)malloc(sizeof(Scroll));
  create_blank_scroll_direct(cur_scroll);
}

void create_blank_scroll_direct(Scroll *cur_scroll)
{
  char *message = (char *)malloc(3);

  memset(cur_scroll, 0, sizeof(Scroll));

  cur_scroll->num_lines = 1;
  cur_scroll->mesg_size = 3;
  cur_scroll->mesg = message;
  message[0] = 0x01;
  message[1] = '\n';
  message[2] = 0x00;
}

Sensor *create_blank_sensor()
{
  Sensor *cur_sensor = (Sensor *)malloc(sizeof(Sensor));
  create_blank_sensor_direct(cur_sensor);
}

void create_blank_sensor_direct(Sensor *cur_sensor)
{
  memset(cur_sensor, 0, sizeof(Sensor));
}

int get_robot_id(Board *src_board, char *name)
{
  int first, last;

  if(find_robot(src_board, name, &first, &last))
  {
    Robot *cur_robot = src_board->robot_list_name_sorted[first];
    // This is a cheap trick for now since robots don't have
    // a back-reference for ID's
    int offset = cur_robot->xpos +
     (cur_robot->ypos * src_board->board_width);
		int d_id = src_board->level_id[offset];

		if((d_id == 123) || (d_id == 124))
		{
			return src_board->level_param[offset];
		}
		else
		{
			int i;
			for(i = 1; i <= src_board->num_robots; i++)
			{
				if(!strcmp(name, (src_board->robot_list[i])->robot_name))
					return i;
			}
		}
  }

  return -1;
}
