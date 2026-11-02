/* The poll loop of Droidspaces' route monitor (network.c) and USB-Arsenal's
 * tether daemon (tether.c), in one process with no threads: rtnetlink
 * events, a signalfd and the ICMPv6 socket of the client discovery.
 * Copyright (C) 2026 ravindu644 <droidcasts@protonmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "monitor/monitor.h"

#include <errno.h>
#include <net/if.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "core/lock.h"
#include "core/log.h"
#include "core/pidfile.h"
#include "monitor/upstream.h"
#include "monitor/v6disc.h"

/* Quiet for 200 ms after the last event before looking, so a burst (netd
 * swapping a network rewrites dozens of rules) costs one reprobe; a second
 * at most, so a steady trickle cannot hold it off. */
#define DEBOUNCE_MS 200
#define DEBOUNCE_MAX_MS 1000
/* A look every 3 s regardless: netd rewrites iptables without a netlink
 * event, and the shared /64's clients are pinged on the same beat. */
#define BEAT_MS 3000
/* Known IPv6 clients are re-installed and pruned every 20 beats. */
#define REFRESH_BEATS 20

static long long now_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000LL + ts.tv_nsec / 1000000;
}

/* Links, addresses, routes and rules, both families: everything a network
 * switch or a netd restart touches. */
static int open_events(void) {
  int fd = socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC | SOCK_NONBLOCK,
                  NETLINK_ROUTE);
  if (fd < 0)
    return -1;
  struct sockaddr_nl sa = {.nl_family = AF_NETLINK,
                           .nl_groups = RTMGRP_LINK | RTMGRP_IPV4_IFADDR |
                                        RTMGRP_IPV4_ROUTE | RTMGRP_IPV6_IFADDR |
                                        RTMGRP_IPV6_ROUTE};
  if (bind(fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
    close(fd);
    return -1;
  }
  /* Rule groups are past the 32-bit mask. */
  int groups[] = {RTNLGRP_IPV4_RULE, RTNLGRP_IPV6_RULE};
  for (int i = 0; i < 2; i++)
    setsockopt(fd, SOL_NETLINK, NETLINK_ADD_MEMBERSHIP, &groups[i],
               sizeof(groups[i]));
  return fd;
}

/* True when anything arrived; an overrun (ENOBUFS) counts, since events
 * were lost. */
static bool drain(int fd) {
  char buf[8192];
  bool any = false;
  for (;;) {
    ssize_t n = recv(fd, buf, sizeof(buf), 0);
    if (n > 0) {
      any = true;
      continue;
    }
    if (n < 0 && errno == ENOBUFS) {
      any = true;
      continue;
    }
    return any;
  }
}

/* The discovery socket a shared /64 depends on. Without one the prefix must
 * not be advertised: clients would get addresses nothing routes back. */
static int discovery(struct nl *nl, const struct vap_opts *o,
                     struct routed *r) {
  if (r->v6.mode != V6_SHARED)
    return -1;
  int fd = v6disc_open(&r->v6);
  if (fd < 0)
    ipv6_off(nl, o, r, "client discovery could not start");
  return fd;
}

static void run(const struct vap_opts *o, struct routed *r, int ev, int sig,
                int ready) {
  struct nl nl;
  if (nl_open(&nl) < 0 || pidfile_write(MONITOR_PID, getpid()) < 0)
    _exit(1);
  int v6 = discovery(&nl, o, r);
  vap_log("Monitor running (PID %d).", (int)getpid());
  if (write(ready, "1", 1) < 0)
    _exit(1);
  close(ready);

  long long beat = now_ms(), due = 0, first = 0;
  int beats = 0;
  for (;;) {
    long long t = now_ms(), until = beat + BEAT_MS;
    if (due && due < until)
      until = due;
    struct pollfd p[3] = {{ev, POLLIN, 0}, {sig, POLLIN, 0}, {v6, POLLIN, 0}};
    int wait = until > t ? (int)(until - t) : 0;
    if (poll(p, v6 >= 0 ? 3 : 2, wait) < 0 && errno != EINTR)
      break;
    if (p[1].revents)
      break; /* SIGTERM or SIGINT: stop owns the teardown. */
    if (p[0].revents && drain(ev)) {
      long long t2 = now_ms();
      if (!due)
        first = t2;
      due = t2 + DEBOUNCE_MS;
      if (due > first + DEBOUNCE_MAX_MS)
        due = first + DEBOUNCE_MAX_MS;
    }
    if (v6 >= 0 && p[2].revents)
      v6disc_read(v6, &nl, &r->v6);
    t = now_ms();
    bool look = (due && t >= due) || t >= beat + BEAT_MS;
    if (!look)
      continue;
    due = 0;
    /* A stop whose signal it missed, or an AP gone under it: nothing left
     * to keep right, and putting rules back would only leak them. */
    if (access(VAP_STATE, F_OK) < 0 || !if_nametoindex(AP_IFACE))
      break;
    if (reprobe(&nl, o, r)) {
      /* A new /64 (or none): a new discovery socket, nothing known. */
      if (v6 >= 0)
        close(v6);
      v6disc_reset();
      v6 = discovery(&nl, o, r);
    }
    if (t >= beat + BEAT_MS) {
      beat = t;
      if (v6 >= 0) {
        if (++beats % REFRESH_BEATS == 0)
          v6disc_refresh();
        v6disc_ping(v6);
      }
    }
  }
  unlink(MONITOR_PID);
  _exit(0);
}

int monitor_spawn(const struct vap_opts *o, const struct routed *r) {
  int pfd[2];
  if (pipe2(pfd, O_CLOEXEC) < 0)
    return -1;
  fflush(stdout);
  pid_t pid = fork();
  if (pid < 0) {
    close(pfd[0]);
    close(pfd[1]);
    vap_warn("Could not start the monitor: the AP will not follow upstream "
             "changes.");
    return -1;
  }
  if (pid == 0) {
    close(pfd[0]);
    /* Off the app's pipe and out of its session: the app waits for the
     * start's output to end, and that must not wait for us. */
    setsid();
    int null = open("/dev/null", O_RDWR | O_CLOEXEC);
    dup2(null, STDIN_FILENO);
    dup2(null, STDOUT_FILENO);
    dup2(null, STDERR_FILENO);
    /* The start's lock is the parent's to give back. */
    vap_unlock();
    signal(SIGHUP, SIG_IGN);
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGTERM);
    sigaddset(&set, SIGINT);
    sigprocmask(SIG_BLOCK, &set, NULL);
    int sig = signalfd(-1, &set, SFD_CLOEXEC | SFD_NONBLOCK);
    int ev = open_events();
    if (sig < 0 || ev < 0)
      _exit(1);
    struct routed live = *r;
    run(o, &live, ev, sig, pfd[1]);
  }
  close(pfd[1]);
  char c;
  ssize_t n = read(pfd[0], &c, 1);
  close(pfd[0]);
  if (n != 1) {
    vap_warn("Could not start the monitor: the AP will not follow upstream "
             "changes.");
    return -1;
  }
  return 0;
}

void monitor_stop(void) {
  pid_t pid = pidfile_ours(MONITOR_PID, "virtualap");
  pidfile_kill(MONITOR_PID, "virtualap");
  /* It may be in the middle of a reprobe that waits on netd's xtables lock;
   * unlike hostapd it holds no firmware state, so it can be killed outright
   * rather than left to put back what the teardown removes. */
  if (pid > 0 && kill(pid, 0) == 0)
    kill(pid, SIGKILL);
}
