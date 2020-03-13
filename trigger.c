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

#define _GNU_SOURCE
#include <signal.h>
#include <poll.h>
#include <sys/inotify.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

#include "pspy-lite.h"

/*
  list of files/directories to watch for files being opened. it
  appears the most important one is /etc/ld.so.cache which is opened
  each time a library is loaded.
*/
const static char* g_c_watches[] = {
  "/etc/ld.so.cache",
};

#define NUM_WATCHES (sizeof(g_c_watches)/sizeof(char*))

static int g_i_wds[NUM_WATCHES];

bool process_watch_events(int i_watch_fd) {
  uint8_t ui_events[4096]
    __attribute__ ((aligned(__alignof__(struct inotify_event))));
  const struct inotify_event *p_s_event;

  ssize_t len = read(i_watch_fd, ui_events, sizeof(ui_events));
  if (len == -1) {
    PERROR();
    return false;
  }

  for (uint8_t* ptr = ui_events;
       ptr < ui_events + len;
       ptr += sizeof(struct inotify_event) + p_s_event->len) {

    p_s_event = (const struct inotify_event *) ptr;

#if defined(DEBUG)
    for (int i = 0; i < NUM_WATCHES; i++) {
      if (g_i_wds[i] == p_s_event->wd) {
        DEBUG_PRINTF("%s", g_c_watches[i]);
        break;
      }
    }

    if (p_s_event->len) {
      DEBUG_PRINTF("/%s %s\n",
                   p_s_event->name,
                   p_s_event->mask & IN_ISDIR ? "[DIR]" : "[FILE]");
    } else {
      DEBUG_PRINTF("\n");
    }
#endif

    /* most interested in file accesses, e.g. /etc/ld.so.conf */
    if (!(p_s_event->mask & IN_ISDIR)) {
      scan_procfs();
    }
  }
  return true;
}

bool trigger(void) {
  bool b_ok = false;
  int i_watcher_fd = inotify_init();
  if (i_watcher_fd == -1) {
    PERROR();
    return false;
  }

  for (int i = 0; i < NUM_WATCHES; i++) {
    g_i_wds[i] = inotify_add_watch(i_watcher_fd, g_c_watches[i], IN_OPEN);
    if (g_i_wds[i] == -1) {
      fprintf(stderr,
              "unable to watch: '%s', could miss proceses.\n",
              g_c_watches[i]);
      if (errno == ENOMEM) {
        fprintf(stderr,
                "please increase /proc/sys/fs/inotify/max_user_watches\n");
      } else {
        PERROR();
      }
      /* pause to let user acknowledge error */
      sleep(1);
    }
  }

  const struct timespec s_sleep = {
    .tv_sec=(g_ui_interval * 1000000) / 1000000000,
    .tv_nsec=(g_ui_interval * 1000000) % 1000000000,
  };

  /* g_b_exit will be set by ctrl-c */
  while (!g_b_exit) {
    struct pollfd s_pollfd = { .fd=i_watcher_fd, .events=POLLIN };
    switch (ppoll(&s_pollfd, 1, &s_sleep, NULL)) {
    case -1:
      if (errno == EINTR) {
        continue;
      }
      PERROR();
      goto done;
    case 0: /* timeout */
      scan_procfs();
      continue;
    case 1: /* 1 fd ready for reading */
      if (!process_watch_events(i_watcher_fd)) {
        goto done;
      }
      continue;
    default:
      fprintf(stderr, __FILE__ ":" str(__LINE__) ": internal error\n");
      goto done;
    }
  }

  b_ok = true;
 done:
  close(i_watcher_fd);
  return b_ok;
}
