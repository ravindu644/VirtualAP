/* The settings: the option table, ap.conf and run.state.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_CLI_H
#define VAP_CLI_H

#include <stdbool.h>
#include <stddef.h>

/* options.c: The settings: one table gives each its flag, its ap.conf key,
 * its default and its usage line, so the parser, the config file and --help
 * never drift. */

/* Every value stays a string until start validates it, so a bad value in
 * ap.conf and a bad flag fail the same way. */
#define OPT_LEN 256

struct vap_opts {
  char phy_iface[OPT_LEN];
  char upstream[OPT_LEN];
  char ssid[OPT_LEN];
  char password[OPT_LEN];
  char band[OPT_LEN];
  char channel[OPT_LEN];
  char width[OPT_LEN];
  char ip_gw[OPT_LEN];
  char dns_servers[OPT_LEN];
  char hidden[OPT_LEN];
  char security[OPT_LEN];
  char pmf[OPT_LEN];
  char ttl_fix[OPT_LEN];
  char ipv6[OPT_LEN];
  char container[OPT_LEN];
  char gw_mac[OPT_LEN];
  char ula[OPT_LEN];
  /* What follows the options, NULL terminated: block and unblock take a MAC. */
  char **args;
};

struct vap_key {
  char flag;            /* the start flag */
  const char *conf;     /* the ap.conf variable */
  size_t off;           /* field in struct vap_opts */
  const char *def;      /* built-in default */
  const char *arg;      /* usage: the value's name */
  const char *help;     /* usage: what it does, may span lines */
  int empty_is_default; /* -g '' means the default gateway */
};

void opts_defaults(struct vap_opts *o);
/* getopt over argv (argv[0] is the command). 0, or -1 after printing what
 * was wrong. */
int opts_parse(struct vap_opts *o, int argc, char **argv);
void print_usage(const char *prog);

/* conf.c: ap.conf, which the app writes, in the format v2.1.0's shell engine
 * sourced, so a downgrade still reads it. */

/* Missing file: nothing changes. Unknown keys are skipped, as sourcing a
 * variable nothing reads was harmless. 0, or -1 when unreadable. */
int conf_load(const char *path, struct vap_opts *o);

/* conf.c: run.state: what a running AP was started with, key=value per line.
 * stop tears down from it and status prints it, so one that v2.1.0's shell
 * engine wrote, under an AP it started, is read the same way. */

#define STATE_MAX 32

struct state {
  size_t n;
  bool full; /* a key was dropped: state_save refuses to write it */
  struct {
    char key[32];
    char val[256];
  } kv[STATE_MAX];
};

/* Appends, or replaces a key already set. A key past STATE_MAX is dropped
 * and marks the state full. */
void state_set(struct state *s, const char *key, const char *val);
const char *state_get(const struct state *s, const char *key);
int state_save(const char *path, const struct state *s);
/* false when there is no file: no AP is up. */
bool state_load(const char *path, struct state *s);

#endif
