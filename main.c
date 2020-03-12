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
  This file is a lightweight re-implementation of pspy in c. See
  following for inspiration: http://github.com/DominicBreuker/pspy

  It was implemented to better understand some odd timings observed
  with original tool when running multiple instances by cutting out
  the go run-time from the equation.

  Currently, it only support scanning the procfs at regular
  intervals. pspy uses a more advanced system of using inotify to also
  trigger scanning procfs.
*/

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#define xstr(s) str(s)
#define str(s) #s

#if defined(DEBUG)
  #define DEBUG_PRINTF(...) fprintf(stderr, __VA_ARGS__)
#else
  #define DEBUG_PRINTF(...)
#endif
#define PERROR() perror(__FILE__ ":" xstr(__LINE__))

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

/* global memory storage for getColour function */
char g_c_colour[64];

/* enable printing of coloured output */
bool g_b_colour = true;

/* length that the cmd is truncated */
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

/*
  below function copied from:
    http://ctips.pbworks.com/w/page/7277591/FNV%20Hash
*/
#define FNV_PRIME_32  16777619
#define FNV_OFFSET_32 2166136261U
static
uint32_t FNV32(const char *p_c_data) {
  uint32_t hash = FNV_OFFSET_32;
  for(const char* p = p_c_data; *p != 0; p++) {
    hash ^= *p;
    hash *= FNV_PRIME_32;
  }
  return hash;
}

/*
  getColour is based on pspy colouring
 */
static
const char* getColour(uid_t ui_uid) {
  // "\x1B[%dm"i_color,

  snprintf(g_c_colour,
           sizeof(g_c_colour) - 1,
           "%" PRIu32,
           ui_uid);
  int i_colour = (FNV32(g_c_colour) % 6) + 91;
  snprintf(g_c_colour,
           sizeof(g_c_colour) - 1,
           "\x1B[%" PRId32 "m",
           i_colour);
  return g_c_colour;
}

static
void print_pid(uint64_t ui_pid) {
  char c_file_name_buff[100];
  snprintf(c_file_name_buff,
           sizeof(c_file_name_buff),
           "/proc/%" PRIu64,
           ui_pid);
  struct stat s_proc_stat;
  if (lstat(c_file_name_buff, &s_proc_stat) < 0) {
#if defined(DEBUG)
    PERROR();
#endif
    return;
  }

  snprintf(c_file_name_buff,
           sizeof(c_file_name_buff),
           "/proc/%" PRIu64 "/cmdline",
           ui_pid);
  int fd = open(c_file_name_buff, O_RDONLY);
  if (fd >= 0) {
    ssize_t l = read(fd, g_p_c_cmdline, g_ui_max_cmdline);
    close(fd);
    if (l < 0) {
      DEBUG_PRINTF("failed to read %s\n", c_file_name_buff);
      *((uint32_t*)g_p_c_cmdline) = 0x3f3f3f;
    } else {
      g_p_c_cmdline[l] = 0;
      for (ssize_t i = 0; i < l; i++) {
        if (g_p_c_cmdline[i] == 0) {
          g_p_c_cmdline[i] = ' ';
        }
      }
    }
  } else {
    DEBUG_PRINTF("failed to open %s\n", c_file_name_buff);
    *((uint32_t*)g_p_c_cmdline) = 0x3f3f3f;
  }

  char c_ppid[24];
  c_ppid[0] = 0;
  if (g_b_ppid) {
    snprintf(c_file_name_buff,
             sizeof(c_file_name_buff),
             "/proc/%" PRIu64 "/stat",
             ui_pid);
    FILE* f_stat = fopen(c_file_name_buff, "r");
    if (f_stat == NULL) {
#if defined(DEBUG)
      PERROR();
#endif
    } else {
      int i_ppid;
      // todo: second format could fail on exec names with white space
      // in them. not sure if this is a thing.
      if (fscanf(f_stat, "%*d %*s %*c %d ", &i_ppid) == 1) {
        sprintf(c_ppid, "PPID=%-7" PRIu32, i_ppid);
      } else {
        sprintf(c_ppid, "PPID=%-7s", "???");
      }
      fclose(f_stat);
    }
  }

  time_t now;
  time(&now);
  struct tm* p_s_now = localtime(&now);
  printf("%02u:%02u:%02u :: %sUID=%-6" PRIu32 "PID=%-7" PRIu64 "%s| %s%s\n",
         p_s_now->tm_hour,
         p_s_now->tm_min,
         p_s_now->tm_sec,
         g_b_colour ? getColour(s_proc_stat.st_uid) : "",
         s_proc_stat.st_uid,
         ui_pid,
         c_ppid,
         g_p_c_cmdline,
         g_b_colour ? "\x1B[39m" : "");
}

static
void scan_procfs(void) {
  DIR *d = opendir("/proc");
  struct dirent *dir;
  if (d) {
    while ((dir = readdir(d)) != NULL) {
      if (dir->d_type & DT_DIR) {
        char* end = NULL;
        uint64_t ui_pid = strtoul(dir->d_name, &end, 10);
        if (*end == '\0') {
          if (!g_p_ui_process_list[ui_pid]) {
            g_p_ui_process_list[ui_pid]++;
            DEBUG_PRINTF("found proc %" PRIu64 "\n", ui_pid);
            print_pid(ui_pid);
          }
        }
      }
    }
    closedir(d);
  }
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


  if (!init()) {
    fprintf(stderr, "failed to initilize\n");
    return 1;
  }

  if (signal(SIGINT, &signal_handler) == SIG_ERR) {
    PERROR();
    goto done;
  }

  struct timespec s_sleep = {
    .tv_sec=(g_ui_interval * 1000000) / 1000000000,
    .tv_nsec=(g_ui_interval * 1000000) % 1000000000,
  };

  while (!g_b_exit) {
    if (nanosleep(&s_sleep, NULL) < 0) {
      if (errno == EINTR) {
        continue;
      }
      PERROR();
      break;
    }
    scan_procfs();
  }

 done:
  printf("\ndone\n");
  free(g_p_ui_process_list);
  g_p_ui_process_list = NULL;
  free(g_p_c_cmdline);
  g_p_c_cmdline = NULL;
  return 0;
}
