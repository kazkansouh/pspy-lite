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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <dirent.h>
#include <time.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "pspy-lite.h"

/* global memory storage for getColour function */
static char g_c_colour[64];
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

/*
  Scan the proc directory once.
*/
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
