/*
 * Copyright (c) 2020 Karim Kanso. All Rights Reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#if !defined(_PSPY_LITE_H)

#include <stdio.h>
#include <stdbool.h>

#define xstr(s) str(s)
#define str(s) #s

#if defined(DEBUG)
  #define DEBUG_PRINTF(...) fprintf(stderr, __VA_ARGS__)
#else
  #define DEBUG_PRINTF(...)
#endif
#define PERROR() perror(__FILE__ ":" xstr(__LINE__))

bool trigger(void);
void scan_procfs(void);

/* space to store a list of processes being tracked */
extern uint8_t* g_p_ui_process_list;

/* length that the cmd is truncated */
extern uint64_t g_ui_max_cmdline;

/* temporary space to store command line before printing */
extern char* g_p_c_cmdline;

/* global set by signal handler */
extern bool g_b_exit;

/* enable printing of coloured output */
extern bool g_b_colour;

/* duration between scanning procfs milliseconds */
extern uint32_t g_ui_interval;

/* duration delay after fd event detected microseconds */
extern uint32_t g_ui_delay;

/* enable showing ppid of the process */
extern bool g_b_ppid;

#endif
