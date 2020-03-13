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

/*
  This program is a lightweight re-implementation of pspy in c. See
  following for inspiration: http://github.com/DominicBreuker/pspy

  It was implemented to better understand some odd timings observed
  with original tool when running multiple instances. This
  implementation was primarily to rule out any impact of the go
  run-time from the equation.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <signal.h>
#include <inttypes.h>
#include <string.h>

#include "pspy-lite.h"

#define MAX_PID_FILE "/proc/sys/kernel/pid_max"
#define MAX_CMD_LINE 125
#define INTERVAL     55

#define IS_FLAG(arg,val) (strcmp(arg, val) == 0)
#define IS_ARG(arg,val)                                                 \
  (strncmp(arg, val, strlen(val)) == 0 && arg[strlen(val)] == '=')

/* space to store a list of processes being tracked */
uint8_t* g_p_ui_process_list = NULL;

/* length that the cmd is truncated */
uint64_t g_ui_max_cmdline = MAX_CMD_LINE;

/* temporary space to store command line before printing */
char* g_p_c_cmdline = NULL;

/* global set by signal handler */
bool g_b_exit = false;

/* enable printing of coloured output */
bool g_b_colour = true;

/* duration between scanning procfs */
uint32_t g_ui_interval = INTERVAL;

/* enable showing ppid of the process */
bool g_b_ppid = false;

/* allocate memory to store processes */
static
bool init(void) {
  FILE* f = fopen(MAX_PID_FILE, "r");
  if (f == NULL) {
    PERROR();
    return false;
  }
  bool b_ok = false;

  uint64_t ui_max_pid;
  if (fscanf(f, "%" SCNu64, &ui_max_pid) != 1) {
    fprintf(stderr, "unable to parse " MAX_PID_FILE "\n");
    goto done;
  }
  DEBUG_PRINTF("max procs: %" PRIu64 "\n", ui_max_pid);

  g_p_ui_process_list = (uint8_t*)calloc(ui_max_pid, 1);
  if (g_p_ui_process_list == NULL) {
    goto done;
  }

  g_p_c_cmdline = (char*)malloc(g_ui_max_cmdline + 1);
  if (g_p_ui_process_list == NULL) {
    free(g_p_ui_process_list);
    g_p_ui_process_list = NULL;
    goto done;
  }

  b_ok = true;
 done:
  fclose(f);
  return b_ok;
}

void signal_handler(int i_signum) {
  g_b_exit = true;
}

void show_help(const char* p_c_name) {
  printf("usage: %s [--no-colour|-n] [--truncate=INT|-t=INT] "
                   "[--interval=INT|-i=INT] [--ppid|-p]\n"
         "\n"
         "where\n"
         "  --no-colour     do not colour code according to uid\n"
         "  --truncate=INT  only print first INT chars of each processes\n"
         "                  cmdline (default: %d)\n"
         "  --interval=INT  how often to scan the /proc directory in ms\n"
         "                  (default: %d)\n"
         "  --ppid          include ppids in the output\n"
         "\n"
         "pspy-lite is based on the pspy program, but re-implemented in\n"
         "c with the goal of being lightweight, small and fast.\n",
         p_c_name,
         MAX_CMD_LINE,
         INTERVAL);
}

int main(int argc, char** argv) {

  for (int i = 1; i < argc; i++) {
    if (IS_FLAG(argv[i], "--help") || IS_FLAG(argv[i], "-h")) {
      show_help(argv[0]);
      return 0;
    }
    if (IS_FLAG(argv[i], "--no-colour") || IS_FLAG(argv[i], "-n")) {
      g_b_colour = false;
      continue;
    }
    if (IS_FLAG(argv[i], "--ppid") || IS_FLAG(argv[i], "-p")) {
      g_b_ppid = true;
      continue;
    }
    if (IS_ARG(argv[i], "--truncate")) {
      if (sscanf(argv[i], "--truncate=%"SCNu64, &g_ui_max_cmdline) != 1) {
        fprintf(stderr, "could not parse %s\n", argv[i]);
        return 1;
      }
      continue;
    }
    if (IS_ARG(argv[i], "-t")) {
      if (sscanf(argv[i], "-t=%"SCNu64, &g_ui_max_cmdline) != 1) {
        fprintf(stderr, "could not parse %s\n", argv[i]);
        return 1;
      }
      continue;
    }
    if (IS_ARG(argv[i], "--interval")) {
      if (sscanf(argv[i], "--interval=%"SCNu32, &g_ui_interval) != 1) {
        fprintf(stderr, "could not parse %s\n", argv[i]);
        return 1;
      }
      continue;
    }
    if (IS_ARG(argv[i], "-i")) {
      if (sscanf(argv[i], "-i=%"SCNu32, &g_ui_interval) != 1) {
        fprintf(stderr, "could not parse %s\n", argv[i]);
        return 1;
      }
      continue;
    }
    fprintf(stderr,
            "unexpected parameter: %s\n"
            "see --help for usage\n",
            argv[i]);
    return 1;
  }

  printf("pspy-lite: the low fat pspy\n"
         "Copyright Karim Kanso 2020. All rights reserved.\n");

  if (!init()) {
    fprintf(stderr, "failed to initilize\n");
    return 1;
  }

  if (signal(SIGINT, &signal_handler) == SIG_ERR) {
    PERROR();
    goto done;
  }

  if (signal(SIGTERM, &signal_handler) == SIG_ERR) {
    PERROR();
    goto done;
  }

  scan_procfs();

  if (!trigger()) {
    fprintf(stderr, "an error occurred\n");
  }


 done:
  printf("\ndone\n");
  free(g_p_ui_process_list);
  g_p_ui_process_list = NULL;
  free(g_p_c_cmdline);
  g_p_c_cmdline = NULL;
  return 0;
}
